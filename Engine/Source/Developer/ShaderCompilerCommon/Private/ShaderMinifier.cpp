// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderMinifier.h"

#include "HAL/PlatformTime.h"
#include "Hash/xxhash.h"
#include "Logging/LogMacros.h"
#include "Misc/AutomationTest.h"
#include "String/Find.h"
#include "Algo/BinarySearch.h"

#define UE_SHADER_MINIFIER_SSE (PLATFORM_CPU_X86_FAMILY && PLATFORM_ENABLE_VECTORINTRINSICS && PLATFORM_ALWAYS_HAS_SSE4_2)

#if UE_SHADER_MINIFIER_SSE
#include <emmintrin.h>
#include <nmmintrin.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogShaderMinifier, Log, All);

// TODO:
// - preserve multi-line #define

namespace UE::ShaderMinifier
{

static FStringView SubStrView(FStringView S, int32 Start)
{
	Start = FMath::Min(Start, S.Len());
	int32 Len = S.Len() - Start;
	return FStringView(S.GetData() + Start, Len);
}

static FStringView SubStrView(FStringView S, int32 Start, int32 Len)
{
	Start = FMath::Min(Start, S.Len());
	Len	  = FMath::Min(Len, S.Len() - Start);
	return FStringView(S.GetData() + Start, Len);
}

template<typename TCondition>
static FStringView SkipUntil(FStringView Source, TCondition Cond)
{
	int32 Cursor = 0;
	const int32 SourceLen = Source.Len();
	while (Cursor < SourceLen)
	{
		if (Cond(FStringView(Source.GetData() + Cursor, SourceLen - Cursor)))
		{
			break;
		}
		++Cursor;
	}
	return FStringView(Source.GetData() + Cursor, SourceLen - Cursor);
}

static bool Equals(FStringView A, FStringView B)
{
	int32 Len = A.Len();
	if (Len != B.Len())
	{
		return false;
	}
	const TCHAR* DataA = A.GetData();
	const TCHAR* DataB = B.GetData();
	for (int32 I = 0; I < Len; ++I)
	{
		if (DataA[I] != DataB[I])
		{
			return false;
		}
	}
	return true;
}

static bool StartsWith(FStringView Source, FStringView Prefix)
{
	const int32 SourceLen = Source.Len();
	const int32 PrefixLen = Prefix.Len();
	if (PrefixLen > SourceLen)
	{
		return false;
	}
	FStringView SourceView(Source.GetData(), PrefixLen);
	return Equals(SourceView, Prefix);
}

struct FCharacterFlags
{
	static constexpr uint8 None = 0;

	static constexpr uint8 Letter     = 1 << 0;
	static constexpr uint8 Number     = 1 << 1;
	static constexpr uint8 Underscore = 1 << 2;
	static constexpr uint8 Space      = 1 << 3;
	static constexpr uint8 Special    = 1 << 4;

	static constexpr uint8 PossibleIdentifierMask = Letter | Number | Underscore;

	FCharacterFlags()
	{
		for (char C = '0'; C <= '9'; ++C)
		{
			Flags[C] |= Number;
		}

		for (char C = 'a'; C <= 'z'; ++C)
		{
			Flags[C] |= Letter;
		}

		for (char C = 'A'; C <= 'Z'; ++C)
		{
			Flags[C] |= Letter;
		}

		Flags['_'] |= Underscore;

		Flags[' ']  |= Space;
		Flags['\f'] |= Space;
		Flags['\r'] |= Space;
		Flags['\n'] |= Space;
		Flags['\t'] |= Space;
		Flags['\v'] |= Space;

		Flags['\\'] |= Special;
		Flags['/']  |= Special;
		Flags['#']  |= Special;
		Flags['{']  |= Special;
		Flags['}']  |= Special;
		Flags['(']  |= Special;
		Flags[')']  |= Special;
	}

	bool IsSpace(TCHAR C) const
	{
		return (Flags[uint8(C)] & Space) != 0;
	}

	bool IsNumber(TCHAR C) const
	{
		return (Flags[uint8(C)] & Number) != 0;
	}

	bool IsPossibleIdentifierCharacter(TCHAR C) const
	{
		return (Flags[uint8(C)] & PossibleIdentifierMask) != 0;
	}

	bool IsSpecial(TCHAR C) const
	{
		return (Flags[uint8(C)] & Special) != 0;
	}

	uint8 Flags[256] = {};
};

static const FCharacterFlags GCharacterFlags;

static bool IsSpace(TCHAR C)
{
	return GCharacterFlags.IsSpace(C);
}

static bool IsNumber(TCHAR C)
{
	return GCharacterFlags.IsNumber(C);
}

static bool IsPossibleIdentifierCharacter(TCHAR C)
{
	return GCharacterFlags.IsPossibleIdentifierCharacter(C);
}

static FStringView ExtractOperator(FStringView Source)
{
	FStringView Result;

	if (Source.IsEmpty() || IsPossibleIdentifierCharacter(Source[0]))
	{
		return Result;
	}

	// NOTE: array is sorted by length to match complete operator character sequences first
	static const FStringView SupportedOperators[] =
	{
		// three-character operators
		FStringView(TEXT("<<=")),
		FStringView(TEXT(">>=")),
		FStringView(TEXT("->*")),

		// two-character operators
		FStringView(TEXT("+=")),
		FStringView(TEXT("++")),
		FStringView(TEXT("-=")),
		FStringView(TEXT("--")),
		FStringView(TEXT("->")),
		FStringView(TEXT("*=")),
		FStringView(TEXT("/=")),
		FStringView(TEXT("%=")),
		FStringView(TEXT("^=")),
		FStringView(TEXT("&=")),
		FStringView(TEXT("&&")),
		FStringView(TEXT("|=")),
		FStringView(TEXT("||")),
		FStringView(TEXT("<<")),
		FStringView(TEXT("<=")),
		FStringView(TEXT(">>")),
		FStringView(TEXT(">=")),
		FStringView(TEXT("==")),
		FStringView(TEXT("!=")),
		FStringView(TEXT("()")),
		FStringView(TEXT("[]")),

		// single character operators
		FStringView(TEXT("+")),
		FStringView(TEXT("-")),
		FStringView(TEXT("*")),
		FStringView(TEXT("/")),
		FStringView(TEXT("%")),
		FStringView(TEXT("^")),
		FStringView(TEXT("&")),
		FStringView(TEXT("|")),
		FStringView(TEXT("~")),
		FStringView(TEXT("!")),
		FStringView(TEXT("=")),
		FStringView(TEXT("<")),
		FStringView(TEXT(">")),
	};

	for (FStringView Operator : SupportedOperators)
	{
		if (StartsWith(Source, Operator))
		{
			Result = Source.SubStr(0, Operator.Len());
			break;
		}
	}

	return Result;
}

static FStringView SkipUntilNonIdentifierCharacter(FStringView Source)
{
	const int32 SourceLen = Source.Len();
	int32 Cursor = 0;
	const TCHAR* SourceData = Source.GetData();

#if UE_SHADER_MINIFIER_SSE
	{
		const int32 AlignedLen = SourceLen & (~7); // align down to multiple of 8 TCHAR-s
		const __m128i NeedleVec = _mm_setr_epi16(L'0', L'9', L'a', L'z', L'A', L'Z', L'_', L'_');
		while (Cursor < AlignedLen)
		{
			__m128i Chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(SourceData + Cursor));
			constexpr int32 Mode = _SIDD_UWORD_OPS | _SIDD_CMP_RANGES | _SIDD_MASKED_NEGATIVE_POLARITY;
			const int32 CompareResult = _mm_cmpistrc(NeedleVec, Chunk, Mode);
			if (CompareResult)
			{
				Cursor += _mm_cmpistri(NeedleVec, Chunk, Mode);
				return FStringView(SourceData + Cursor, SourceLen - Cursor);
			}
			Cursor += 8;
		}
	}
#endif // UE_SHADER_MINIFIER_SSE

	while (Cursor < SourceLen)
	{
		if (!IsPossibleIdentifierCharacter(SourceData[Cursor]))
		{
			break;
		}
		++Cursor;
	}
	return FStringView(SourceData + Cursor, SourceLen - Cursor);
}

static FStringView SkipUntilNonNumber(FStringView Source) 
{
	const int32 SourceLen = Source.Len();
	int32 Cursor = 0;
	const TCHAR* SourceData = Source.GetData();
	while (Cursor < SourceLen)
	{
		if (!IsNumber(SourceData[Cursor]))
		{
			break;
		}
		++Cursor;
	}
	return FStringView(SourceData + Cursor, SourceLen - Cursor);
}

static FStringView SkipSpace(FStringView Source)
{
	const int32 SourceLen = Source.Len();
	int32 Cursor = 0;
	const TCHAR* SourceData = Source.GetData();

#if UE_SHADER_MINIFIER_SSE
	{
		const int32 AlignedLen = SourceLen & (~7); // align down to multiple of 8 TCHAR-s
		const __m128i NeedleVec = _mm_setr_epi16(L' ', L'\f', L'\r', L'\n', L'\t', L'\v', 0, 0);
		while (Cursor < AlignedLen)
		{
			__m128i Chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(SourceData + Cursor));
			constexpr int32 Mode = _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_MASKED_NEGATIVE_POLARITY;
			const int32 CompareResult = _mm_cmpistrc(NeedleVec, Chunk, Mode);
			if (CompareResult)
			{
				Cursor += _mm_cmpistri(NeedleVec, Chunk, Mode);
				return FStringView(SourceData + Cursor, SourceLen - Cursor);
			}
			Cursor += 8;
		}
	}
#endif // UE_SHADER_MINIFIER_SSE

	while (Cursor < SourceLen)
	{
		if (!IsSpace(SourceData[Cursor]))
		{
			break;
		}
		++Cursor;
	}
	return FStringView(SourceData + Cursor, SourceLen - Cursor);
}

static FStringView TrimSpace(FStringView Source)
{
	int32 CursorBegin = 0;
	int32 CursorEnd = Source.Len();

	while (CursorBegin != CursorEnd)
	{
		if (!IsSpace(Source[CursorBegin]))
		{
			break;
		}
		++CursorBegin;
	}

	while (CursorBegin != CursorEnd)
	{
		if (!IsSpace(Source[CursorEnd-1]))
		{
			break;
		}
		--CursorEnd;
	}

	FStringView Result = SubStrView(Source, CursorBegin, CursorEnd-CursorBegin);

	return Result;
}

static FStringView SkipUntilNextLine(FStringView Source)
{
	int32 Index = INDEX_NONE;
	if (Source.FindChar('\n', Index))
	{
		return FStringView(Source.GetData() + Index, Source.Len() - Index);
	}
	else
	{
		return FStringView {};
	}
}

static FStringView SkipUntilStr(FStringView Haystack, FStringView Needle)
{
	return SkipUntil(Haystack, [Needle](FStringView  S) { return StartsWith(S, Needle); });
}

static FStringView ExtractBlock(FStringView Source, TCHAR DelimBegin, TCHAR DelimEnd)
{
	// TODO: handle comments
	// TODO: handle #if 0 blocks

	int32 PosEnd = INDEX_NONE;
	int32 Stack  = 0;
	const int32 SourceLen = Source.Len();
	const TCHAR* SourceData = Source.GetData();

	int32 Cursor = 0;

	enum class EStatus
	{
		Finished,
		Continue,
	};

	EStatus Status = EStatus::Continue;

	auto ProcessCharacter = [&Stack, &PosEnd, SourceData, DelimBegin, DelimEnd](int32 Cursor) -> EStatus
	{
		TCHAR C = SourceData[Cursor];

		if (C == DelimBegin)
		{
			Stack++;
		}
		else if (C == DelimEnd)
		{
			if (Stack == 0)
			{
				// delimiter mismatch
				return EStatus::Finished;
			}

			Stack--;

			if (Stack == 0)
			{
				PosEnd = Cursor;
				return EStatus::Finished;
			}
		}

		return EStatus::Continue;
	};

#if UE_SHADER_MINIFIER_SSE
	{
		const int32 AlignedLen = SourceLen & (~7); // align down to multiple of 8 TCHAR-s
		const __m128i NeedleVec = _mm_setr_epi16(DelimBegin, DelimEnd, 0, 0, 0, 0, 0, 0);
		while (Cursor < AlignedLen && Status != EStatus::Finished)
		{
			__m128i Chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(SourceData + Cursor));
			constexpr int32 Mode = _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_MOST_SIGNIFICANT;
			const int32 CompareResult = _mm_cmpistrc(NeedleVec, Chunk, Mode);
			if (CompareResult)
			{
				__m128i MaskVec = _mm_cmpistrm(NeedleVec, Chunk, Mode);
				uint32 Mask = _mm_movemask_epi8(MaskVec);
				while (Mask != 0 && Status != EStatus::Finished)
				{
					const uint32 BitIndex = FMath::CountTrailingZeros(Mask);
					const uint32 ChunkCharIndex = BitIndex / 2;
					Status = ProcessCharacter(Cursor + ChunkCharIndex);
					Mask &= ~(3 << BitIndex);
				}
			}
			Cursor += 8;
		}
	}
#endif // UE_SHADER_MINIFIER_SSE

	while (Cursor < SourceLen && Status != EStatus::Finished)
	{
		Status = ProcessCharacter(Cursor);
		++Cursor;
	}

	if (Stack == 0 && PosEnd != INDEX_NONE)
	{
		return FStringView(Source.GetData(), PosEnd + 1);
	}
	else
	{
		return FStringView{};
	}
}

enum class EBlockType : uint8 {
	Unknown,	// various identifiers and keywords that we did not need to or could not identify
	Keyword,	// e.g. struct, switch, register
	Attribute,	// e.g. `[numthreads(8,8,1)]`
	Type,		// return type of function or struct/cbuffer/variable type
	Base,		// inheritance base type
	Name,		// struct/variable/function name
	Binding,	// e.g. `register(t0, space1)` or `SV_Target0`
	Args,
	Body,
	Subscript,
	TemplateArgs,
	Expression,
	Directive,  // #define, #pragma, #line, etc.
	NamespaceDelimiter, // e.g. :: in an identifier like Foo::bar
	PtrOrRef, // e.g. '*' or '&' as part of the type
	OperatorName, // overladed operator, e.g. '+', '+=', etc.
};

struct FCodeBlock
{
	const TCHAR* CodePtr = nullptr;
	int32        CodeLen = 0;
	EBlockType   Type = EBlockType::Unknown;
	uint8        Padding[3] = {};

	operator FStringView () const
	{
		return GetCode();
	}

	bool operator == (const FStringView S) const
	{
		return Equals(GetCode(), S);
	}

	void SetCode(FStringView Code)
	{
		CodePtr = Code.GetData();
		CodeLen = Code.Len();
	}

	FStringView GetCode() const
	{
		return FStringView(CodePtr, CodeLen);
	}
};

static_assert(sizeof(FCodeBlock) == 16, "Unexpected FCodeBlock size");

enum class ECodeChunkType {
	Unknown,
	Struct,
	CBuffer,  // HLSL cbuffer block possibly without trailing ';'
	Function,
	Operator,
	Variable,
	Enum,
	Define,
	Pragma,
	CommentLine, // Single line comment
	Namespace,
	Using,
	Typedef,
};

struct FNamespace
{
	FNamespace() = default;
	FNamespace(TConstArrayView<FStringView> InStack)
	{
		if (!InStack.IsEmpty())
		{
			for (const FStringView& Part : InStack)
			{
				FullName += Part;
				FullName += TEXT("::");
			}
			FullName.LeftChopInline(2);
		}
		Stack = InStack;
	}

	FString FullName; // i.e. Foo::Bar::Baz
	TArray<FStringView> Stack; // i.e. [Foo, Bar, Baz]
};

using FCodeBlockArray = TArray<FCodeBlock, TInlineAllocator<6>>;

struct FCodeChunk
{
	ECodeChunkType Type = ECodeChunkType::Unknown;
	FCodeBlockArray Blocks;

	int32 Namespace = INDEX_NONE; // Unique namespace ID (INDEX_NONE = global)

	// Indicates whether the code for this chunk can be used as-is.
	// One example where we have to do custom code emission is when a named struct and a variable are declared in one chunk.
	// The struct type may be referenced, but the variable may be removed. In this case we have to emit the type declaration only.
	bool bVerbatim = true;

	FStringView FindFirstBlockByType(EBlockType InType) const
	{
		for (const FCodeBlock& Block : Blocks)
		{
			if (Block.Type == InType)
			{
				return Block;
			}
		}
		return {};
	}

	// String view covering the entire code chunk
	FStringView GetCode() const
	{
		if (Blocks.IsEmpty())
		{
			return {};
		}
		else
		{
			const FCodeBlock& FirstBlock = Blocks[0];
			const FCodeBlock& LastBlock = Blocks[Blocks.Num()-1];
			const TCHAR* Begin = FirstBlock.CodePtr;
			const TCHAR* End = LastBlock.CodePtr + LastBlock.CodeLen;
			return FStringView(Begin, int32(End-Begin));
		}
	}
};

struct FParsedShader
{
	FStringView Source;
	TArray<FCodeChunk> Chunks;
	TArray<FNamespace> Namespaces;
	TArray<FStringView> LineDirectives;
};

struct FNamespaceTracker
{
	TMap<FString, int32> UniqueNamespaceMap;
	TArray<FNamespace> UniqueNamespaceArray;
	TArray<FStringView> NamespaceStack;
	TArray<int32> NamespaceIdStack;

	FNamespaceTracker() = default;

	void Push(FStringView Name)
	{
		NamespaceStack.Push(Name);
		FNamespace NamespaceEntry(NamespaceStack);
		int32& EntryIndex = UniqueNamespaceMap.FindOrAdd(NamespaceEntry.FullName, INDEX_NONE);
		if (EntryIndex == INDEX_NONE)
		{
			EntryIndex = UniqueNamespaceArray.Num();
			UniqueNamespaceArray.Add(MoveTemp(NamespaceEntry));
		}
		NamespaceIdStack.Push(EntryIndex);
	}

	bool Pop()
	{
		if (NamespaceStack.IsEmpty())
		{
			return false;
		}
		else
		{
			NamespaceStack.Pop();
			NamespaceIdStack.Pop();
			return true;
		}
	}

	int32 CurrentId() const
	{
		return NamespaceIdStack.IsEmpty() ? INDEX_NONE : NamespaceIdStack.Last();
	}
};

FStringView ExtractNextIdentifier(FStringView Source)
{
	FStringView Remainder = SkipUntilNonIdentifierCharacter(Source);
	FStringView Identifier = SubStrView(Source, 0, Source.Len() - Remainder.Len());
	return Identifier;
}

static FParsedShader ParseShader(FStringView InSource, FDiagnostics& Output)
{
	FParsedShader Result;

	Result.Source = InSource;

	FStringView		   Source = InSource;
	FCodeBlockArray    PendingBlocks;
	TArray<FCodeChunk> Chunks;

	ECodeChunkType ChunkType = ECodeChunkType::Unknown;
	bool		   bFoundBody = false;
	bool		   bFoundColon = false;
	bool		   bFoundIdentifier = false;
	bool		   bFoundAssignment = false;

	int32 ArgsBlockIndex = INDEX_NONE;
	int32 CbufferBlockIndex = INDEX_NONE;
	int32 StructBlockIndex = INDEX_NONE;
	int32 EnumBlockIndex = INDEX_NONE;
	int32 BodyBlockIndex = INDEX_NONE;
	int32 ExpressionBlockIndex = INDEX_NONE;
	int32 OperatorKeywordBlockIndex = INDEX_NONE;

	FNamespaceTracker NamespaceTracker;
	FStringView		  PendingNamespace;

	auto AddDiagnostic = [InSource, &Source](TArray<FDiagnosticMessage>& Output, FStringView Message)
	{
		FDiagnosticMessage Diagnostic;

		Diagnostic.Message = FString(Message);
		Diagnostic.Offset = int32(Source.GetData() - InSource.GetData());
		// Diagnostic.Line = ...; // TODO
		// Diagnostic.Column = ...; // TODO

		Output.Add(MoveTemp(Diagnostic));
	};

	auto AddBlock = [&PendingBlocks](EBlockType Type, FStringView Code)
	{
		FCodeBlock NewBlock;
		NewBlock.Type = Type;
		NewBlock.SetCode(Code);
		PendingBlocks.Push(NewBlock);
	};

	auto FinalizeChunk = [&]()
	{
		const bool bFoundArgs = ArgsBlockIndex >= 0;

		bool bHasType = false;
		bool bHasName = false;

		if (!PendingBlocks.IsEmpty())
		{
			if (ChunkType == ECodeChunkType::Unknown)
			{
				if (bFoundIdentifier && bFoundArgs && bFoundBody)
				{
					ChunkType = ECodeChunkType::Function;
				}
				else if (bFoundIdentifier)
				{
					ChunkType = ECodeChunkType::Variable;
				}
			}

			int32 NameBlockIndex = INDEX_NONE;

			if (ChunkType == ECodeChunkType::Struct)
			{
				check(StructBlockIndex >= 0);

				PendingBlocks[StructBlockIndex].Type = EBlockType::Keyword;

				int32 TypeBlockIndex = StructBlockIndex + 1;
				if (TypeBlockIndex != BodyBlockIndex && TypeBlockIndex < PendingBlocks.Num())
				{
					PendingBlocks[TypeBlockIndex].Type = EBlockType::Type;
					bHasType = true;
				}

				// If struct body is not the last block, it must be followed by a variable name
				// i.e. `struct Foo { ... } Blah;` or `struct { ... } Blah;` or `struct Foo { ... } Blah = { expression };`
				if (BodyBlockIndex > 0 && BodyBlockIndex + 1 < PendingBlocks.Num())
				{
					NameBlockIndex = BodyBlockIndex + 1;
					PendingBlocks[NameBlockIndex].Type = EBlockType::Name;
					bHasName = true;
				}

				// If there is an expression block, we expect a named variable to also exist
				// i.e. `struct Foo { ... } Blah = { expression };` 
				if (ExpressionBlockIndex > 0 && NameBlockIndex == INDEX_NONE)
				{
					AddDiagnostic(Output.Errors, TEXT("Initialized struct variables must be named"));
					return;
				}
			}
			else if (ChunkType == ECodeChunkType::CBuffer)
			{
				check(CbufferBlockIndex >= 0);

				PendingBlocks[CbufferBlockIndex].Type = EBlockType::Keyword;

				int32 TypeBlockIndex = CbufferBlockIndex + 1;
				if (TypeBlockIndex != BodyBlockIndex && TypeBlockIndex < PendingBlocks.Num())
				{
					PendingBlocks[TypeBlockIndex].Type = EBlockType::Type;
				}
			}
			else if (ChunkType == ECodeChunkType::Enum)
			{
				check(EnumBlockIndex >= 0);

				PendingBlocks[EnumBlockIndex].Type = EBlockType::Keyword;

				if (BodyBlockIndex > 1)
				{
					PendingBlocks[BodyBlockIndex - 1].Type = EBlockType::Type;
				}
			}
			else if (ChunkType == ECodeChunkType::Function)
			{
				NameBlockIndex = ArgsBlockIndex - 1;
				if (NameBlockIndex >= 0)
				{
					PendingBlocks[NameBlockIndex].Type = EBlockType::Name;
				}
			}
			else if (ChunkType == ECodeChunkType::Variable)
			{
				// TODO: tag name / type / binding
			}
			else if (ChunkType == ECodeChunkType::Typedef)
			{
				NameBlockIndex = PendingBlocks.Num() - 1;
				for (int32 Index = 1; Index < NameBlockIndex; Index++)
				{
					PendingBlocks[Index].Type = EBlockType::Type;
				}
				PendingBlocks[NameBlockIndex].Type = EBlockType::Name;
			}
			else if (ChunkType == ECodeChunkType::Operator)
			{
				// Treat any uncategorized blocks as part of the type
				for (int32 Index = 0; Index < OperatorKeywordBlockIndex; Index++)
				{
					if (PendingBlocks[Index].Type == EBlockType::Unknown)
					{
						PendingBlocks[Index].Type = EBlockType::Type;
					}
				}
			}

			if (ChunkType == ECodeChunkType::Struct && bHasName && !bHasType)
			{
				ChunkType = ECodeChunkType::Variable;
			}

			const int32 Namespace = NamespaceTracker.CurrentId();

			if (ChunkType == ECodeChunkType::Struct && bHasName && bHasType)
			{
				// Handle simultaneous struct type and variable declaration

				FCodeChunk StructChunk;
				StructChunk.Type = ECodeChunkType::Struct;
				StructChunk.bVerbatim = false;

				for (int32 i = int32(StructBlockIndex); i < NameBlockIndex; ++i)
				{
					StructChunk.Blocks.Push(PendingBlocks[i]);
				}

				FCodeChunk VarChunk;
				VarChunk.Type = ECodeChunkType::Variable;
				VarChunk.bVerbatim = false;

				for (int32 i = 0; i < PendingBlocks.Num(); ++i)
				{
					if (i == StructBlockIndex || i == BodyBlockIndex)
					{
						continue;
					}
					VarChunk.Blocks.Push(PendingBlocks[i]);
				}

				StructChunk.Namespace = Namespace;
				VarChunk.Namespace = Namespace;

				Chunks.Push(StructChunk);
				Chunks.Push(VarChunk);
			}
			else
			{
				FCodeChunk Chunk;
				Chunk.Type = ChunkType;
				Chunk.Namespace = Namespace;
				Swap(Chunk.Blocks, PendingBlocks);
				Chunks.Push(Chunk);
			}

			ChunkType = ECodeChunkType::Unknown;
			ArgsBlockIndex = INDEX_NONE;
			CbufferBlockIndex = INDEX_NONE;
			StructBlockIndex = INDEX_NONE;
			EnumBlockIndex = INDEX_NONE;
			BodyBlockIndex = INDEX_NONE;
			ExpressionBlockIndex = INDEX_NONE;
			OperatorKeywordBlockIndex = INDEX_NONE;
			bFoundBody = false;
			bFoundColon = false;
			bFoundIdentifier = false;
			bFoundAssignment = false;

			PendingBlocks.Reset();
		}
	};

	while (Output.Errors.IsEmpty())
	{
		Source = SkipSpace(Source);

		if (Source.IsEmpty())
		{
			break;
		}

		const TCHAR FirstChar = *Source.GetData();

		if (GCharacterFlags.IsSpecial(FirstChar))
		{
			if (FirstChar == '/')
			{
				if (StartsWith(Source, TEXTVIEW("//")))
				{
					FStringView Remainder = SkipUntilNextLine(Source);

					// Save comment lines that are outside of blocks
					if (PendingBlocks.IsEmpty())
					{
						FStringView Block = SubStrView(Source, 0, Source.Len() - Remainder.Len());
						AddBlock(EBlockType::Unknown, Block);
						ChunkType = ECodeChunkType::CommentLine;
						FinalizeChunk();
					}

					Source = Remainder;

					continue;
				}
				else if (StartsWith(Source, TEXTVIEW("/*")))
				{
					Source = SkipUntilStr(Source, TEXTVIEW("*/"));
					if (Source.Len() >= 2)
					{
						Source = SubStrView(Source, 2);
					}
					continue;
				}
			}
			else if (FirstChar == '#')
			{
				if (StartsWith(Source, TEXTVIEW("#line")))
				{
					FStringView Remainder = SkipUntilNextLine(Source);
					FStringView Block = SubStrView(Source, 0, Source.Len() - Remainder.Len());
					Result.LineDirectives.Add(Block);
					Source = Remainder;
					continue;
				}
				else if (StartsWith(Source, TEXTVIEW("#pragma")))
				{
					FStringView Remainder = SkipUntilNextLine(Source);
					FStringView Block = SubStrView(Source, 0, Source.Len() - Remainder.Len());
					AddBlock(EBlockType::Directive, Block);
					ChunkType = ECodeChunkType::Pragma;
					FinalizeChunk();
					Source = Remainder;
					continue;
				}
				else if (StartsWith(Source, TEXTVIEW("#define")))
				{
					// TODO: handle `\` new lines in defines
					FStringView Remainder = SkipUntilNextLine(Source);
					FStringView Block = SubStrView(Source, 0, Source.Len() - Remainder.Len());
					AddBlock(EBlockType::Directive, Block);
					ChunkType = ECodeChunkType::Define;
					FinalizeChunk();
					Source = Remainder;
					continue;
				}
				else if (StartsWith(Source, TEXTVIEW("#if 0")))
				{
					Source = SkipUntilStr(Source, TEXTVIEW("#endif"));
					if (Source.Len() >= 6)
					{
						Source = SubStrView(Source, 6);
					}
					continue;
				}
			}
			else if (PendingBlocks.IsEmpty() && (FirstChar == '{' || FirstChar == '}'))
			{
				if (StartsWith(Source, TEXTVIEW("{")))
				{
					if (ChunkType == ECodeChunkType::Namespace)
					{
						if (PendingNamespace.IsEmpty())
						{
							AddDiagnostic(Output.Errors, TEXT("HLSL does not support anonymous namespaces"));
							break;
						}
						else
						{
							NamespaceTracker.Push(PendingNamespace);
							ChunkType = ECodeChunkType::Unknown;
							PendingNamespace = {};
							Source = Source.Mid(1);
						}
						continue;
					}
					else
					{
						AddDiagnostic(Output.Errors, TEXT("Expected token '{'"));
					}
					continue;
				}
				else if (StartsWith(Source, TEXTVIEW("}")))
				{
					if (NamespaceTracker.Pop())
					{
						Source = Source.Mid(1);
						continue;
					}
					else
					{
						AddDiagnostic(Output.Errors, TEXT("Expected token '}'"));
						break;
					}
				}
			}
		}

		if (ChunkType == ECodeChunkType::Operator
			&& !PendingBlocks.IsEmpty()
			&& OperatorKeywordBlockIndex == (PendingBlocks.Num()-1))
		{
			// Operator keyword found in the last processed block. Expect to find operator name character sequence next.

			FStringView OperatorName = ExtractOperator(Source);

			if (OperatorName.IsEmpty())
			{
				AddDiagnostic(Output.Errors, TEXT("Unexpected operator overload type"));
				break;
			}

			AddBlock(EBlockType::OperatorName, OperatorName);
			Source = SubStrView(Source, OperatorName.Len());

			continue;
		}

		FStringView Remainder = SkipUntilNonIdentifierCharacter(Source);
		FStringView Identifier = SubStrView(Source, 0, Source.Len() - Remainder.Len());

		if (Identifier.Len())
		{
			if (ChunkType == ECodeChunkType::Unknown)
			{
				if (Equals(Identifier, TEXTVIEW("struct")))
				{
					ChunkType = ECodeChunkType::Struct;
					StructBlockIndex = PendingBlocks.Num();
				}
				else if (Equals(Identifier, TEXTVIEW("cbuffer")) || Equals(Identifier, TEXTVIEW("ConstantBuffer")))
				{
					ChunkType = ECodeChunkType::CBuffer;
					CbufferBlockIndex = PendingBlocks.Num();
				}
				else if (Equals(Identifier, TEXTVIEW("enum")))
				{
					ChunkType = ECodeChunkType::Enum;
					EnumBlockIndex = PendingBlocks.Num();
				}
				else if (Equals(Identifier, TEXTVIEW("namespace")))
				{
					ChunkType = ECodeChunkType::Namespace;
					Source = Remainder;
					continue;
				}
				else if (Equals(Identifier, TEXTVIEW("using")))
				{
					ChunkType = ECodeChunkType::Using;
					Source = Remainder;
					AddBlock(EBlockType::Keyword, Identifier);
					continue;
				}
				else if (Equals(Identifier, TEXTVIEW("typedef")))
				{
					ChunkType = ECodeChunkType::Typedef;
					Source = Remainder;
					AddBlock(EBlockType::Keyword, Identifier);
					continue;
				}
				else if (Equals(Identifier, TEXTVIEW("template")))
				{
					Source = Remainder;
					AddBlock(EBlockType::Keyword, Identifier);
					continue;
				}
				else if (Equals(Identifier, TEXTVIEW("operator")))
				{
					ChunkType = ECodeChunkType::Operator;
					Source = Remainder;
					OperatorKeywordBlockIndex = PendingBlocks.Num();
					AddBlock(EBlockType::Keyword, Identifier);
					continue;
				}
			}
			else if (ChunkType == ECodeChunkType::Namespace)
			{
				PendingNamespace = Identifier;
				Source = Remainder;
				continue;
			}

			EBlockType BlockType = EBlockType::Unknown;

			if (bFoundColon)
			{
				if (ChunkType == ECodeChunkType::Struct)
				{
					BlockType = EBlockType::Base;
				}
				else
				{
					BlockType = EBlockType::Binding;
				}

				bFoundColon = false;
			}

			AddBlock(BlockType, Identifier);

			Source = Remainder;
			bFoundIdentifier = true;

			continue;
		}

		FStringView Block;

		TCHAR C = Source[0];

		EBlockType BlockType = EBlockType::Unknown;

		if (StartsWith(Source, TEXTVIEW("==")))
		{
			AddDiagnostic(Output.Errors, TEXT("Unexpected sequence '=='"));
			break;
		}
		else if (StartsWith(Source, TEXTVIEW("::")))
		{
			Block = SubStrView(Source, 0, 2);
			Source = SubStrView(Source, 2);

			AddBlock(EBlockType::NamespaceDelimiter, Block);

			continue;
		}
		else if (C == '=')
		{
			bFoundAssignment = true;

			Source = SkipSpace(Source.Mid(1));

			char C2 = Source[0];

			if (C2 == '{')
			{
				// extract block on the next loop iteration
				continue;
			}
			else
			{
				int32 Pos = INDEX_NONE;

				if (!Source.FindChar(TCHAR(';'), Pos))
				{
					AddDiagnostic(Output.Errors, TEXT("Expected semicolon after assignment expression"));
					break;
				}

				Block = SubStrView(Source, 0, Pos);
				Block = TrimSpace(Block);

				int32 BlockOffset = int32(Block.GetData() - Source.GetData());
				Source = SubStrView(Source, BlockOffset);

				BlockType = EBlockType::Expression;
				ExpressionBlockIndex = int32(PendingBlocks.Num());
			}
		}
		else if (C == ':')
		{
			Source = SubStrView(Source, 1);
			bFoundColon = true;
			continue;
		}
		else if (C == '(')
		{
			Block = ExtractBlock(Source, '(', ')');
			BlockType = EBlockType::Args;

			if (ArgsBlockIndex < 0)
			{
				ArgsBlockIndex = PendingBlocks.Num();
			}
		}
		else if (C == '{')
		{
			Block = ExtractBlock(Source, '{', '}');

			if (BodyBlockIndex == INDEX_NONE && !bFoundAssignment)
			{
				BlockType = EBlockType::Body;
				BodyBlockIndex = PendingBlocks.Num();
				bFoundBody = true;
			}
			else if (bFoundAssignment)
			{
				BlockType = EBlockType::Expression;
				ExpressionBlockIndex = PendingBlocks.Num();
			}
		}
		else if (C == '[')
		{
			Block = ExtractBlock(Source, '[', ']');

			if (bFoundIdentifier)
			{
				BlockType = EBlockType::Subscript;
			}
			else
			{
				BlockType = EBlockType::Attribute;
			}
		}
		else if (C == '<')
		{
			Block = ExtractBlock(Source, '<', '>');
			BlockType = EBlockType::TemplateArgs;

			if (ChunkType == ECodeChunkType::CBuffer)
			{
				// `ConstantBuffer<Foo>` is treated as a variable/resource declaration rather than a cbuffer block
				ChunkType = ECodeChunkType::Variable;
			}
		}
		else if (C == ';')
		{
			FinalizeChunk();
			Source = SubStrView(Source, 1);
			continue;
		}
		else if ((C == '*' || C == '&') && !PendingBlocks.IsEmpty()) // Part of a pointer or reference declaration
		{
			Block = SubStrView(Source, 0, 1);
			Source = SubStrView(Source, 1);

			AddBlock(EBlockType::PtrOrRef, Block);

			continue;
		}
		else
		{
			AddDiagnostic(Output.Errors, FString::Printf(TEXT("Unexpected character '%c'"), C));
			break;
		}

		if (Block.IsEmpty())
		{
			AddDiagnostic(Output.Errors, TEXT("Failed to extract code block"));
			break;
		}
		else
		{
			AddBlock(BlockType, Block);

			Source = SubStrView(Source, Block.Len());

			if (BlockType == EBlockType::Body && ArgsBlockIndex != INDEX_NONE)
			{
				FinalizeChunk();
			}
			else if (BlockType == EBlockType::Body && CbufferBlockIndex != INDEX_NONE)
			{
				FinalizeChunk();
			}
			else if (BlockType == EBlockType::Expression)
			{
				FinalizeChunk();
			}
		}
	}

	Swap(Result.Chunks, Chunks);
	Swap(Result.Namespaces, NamespaceTracker.UniqueNamespaceArray);

	return Result;
}

template<typename CallbackT>
void FindChunksByIdentifier(TConstArrayView<FCodeChunk> Chunks, FStringView Identifier, CallbackT Callback)
{
	for (const FCodeChunk& Chunk : Chunks)
	{
		for (const FCodeBlock& Block : Chunk.Blocks)
		{
			if (Block == Identifier)
			{
				Callback(Chunk);
			}
		}
	}
}

static TArray<FStringView> SplitByChar(FStringView Source, TCHAR Delimiter)
{
	TArray<FStringView> Result;

	int32 Start = 0;
	const int32 SourceLen = Source.Len();

	for (int32 I = 0; I < SourceLen; ++I)
	{
		TCHAR C = Source[I];
		if (C == Delimiter)
		{
			size_t Len = I - Start;
			Result.Push(SubStrView(Source, Start, Len));
			Start = I + 1;
		}
	}

	if (Start != Source.Len())
	{
		int32 Len = Source.Len() - Start;
		Result.Push(SubStrView(Source, Start, Len));
	}

	return Result;
}

static void ExtractIdentifiers(FStringView InSource, TArray<FStringView>& Result)
{
	FStringView Source = InSource;

	while (!Source.IsEmpty())
	{
		TCHAR FirstChar = Source.GetData()[0];

		if (FirstChar == L'#')
		{
			if (StartsWith(Source, TEXTVIEW("#line"))
				|| StartsWith(Source, TEXTVIEW("#pragma")))
			{
				Source = SkipUntilNextLine(Source);
				Source = SkipSpace(Source);
				continue;
			}
			else if (StartsWith(Source, TEXTVIEW("#if 0")))
			{
				Source = SkipUntilStr(Source, TEXTVIEW("#endif"));
				if (Source.Len() >= 6)
				{
					Source = SubStrView(Source, 6);
				}
				Source = SkipSpace(Source);
				continue;
			}
		}
		else if (FirstChar == L'/')
		{
			if (StartsWith(Source, TEXTVIEW("//")))
			{
				Source = SkipUntilNextLine(Source);
				Source = SkipSpace(Source);
				continue;
			}
			else if (StartsWith(Source, TEXTVIEW("/*")))
			{
				Source = SkipUntilStr(Source, TEXTVIEW("*/"));
				if (Source.Len() >= 2)
				{
					Source = SubStrView(Source, 2);
				}
				Source = SkipSpace(Source);
				continue;
			}
		}

		FStringView Remainder = SkipUntilNonIdentifierCharacter(Source);

		FStringView Identifier = SubStrView(Source, 0, Source.Len() - Remainder.Len());

		if (Identifier.IsEmpty())
		{
			if (!Remainder.IsEmpty())
			{
				Remainder = SubStrView(Remainder, 1);
			}
		}
		else
		{
			if (!IsNumber(Identifier[0]))  // Identifiers can't start with numbers
			{
				Result.Push(Identifier);
			}
		}

		Source = SkipSpace(Remainder);
	}
}

static void ExtractIdentifiers(const FCodeChunk& Chunk, TArray<FStringView>& Result)
{
	for (const FCodeBlock& Block : Chunk.Blocks)
	{
		ExtractIdentifiers(Block, Result);
	}
}

static void OutputChunk(const FCodeChunk& Chunk, FString& OutputStream)
{
	if (Chunk.Blocks.IsEmpty())
	{
		return;
	}

	if (Chunk.bVerbatim)
	{
		// Fast path to output entire code block verbatim, preserving any new lines and whitespace		
		OutputStream.Append(Chunk.GetCode());
	}
	else
	{
		int32 Index = 0;
		for (const FCodeBlock& Block : Chunk.Blocks)
		{
			if (Index != 0)
			{
				OutputStream.AppendChar(L' ');
			}

			if (Block.Type == EBlockType::Expression)
			{
				OutputStream.Append(TEXTVIEW("= "));
			}
			else if (Block.Type == EBlockType::Body)
			{
				OutputStream.AppendChar(L'\n');
			}

			if (Block.Type == EBlockType::Binding || Block.Type == EBlockType::Base)
			{
				OutputStream.Append(TEXTVIEW(": "));
			}

			OutputStream.Append(Block.GetCode());

			++Index;
		}
	}

	if (Chunk.Type != ECodeChunkType::Function
		&& Chunk.Type != ECodeChunkType::Operator
		&& Chunk.Type != ECodeChunkType::CBuffer
		&& Chunk.Type != ECodeChunkType::Pragma
		&& Chunk.Type != ECodeChunkType::Define
		&& Chunk.Type != ECodeChunkType::CommentLine)
	{
		OutputStream.AppendChar(L';');
	}

	OutputStream.AppendChar(L'\n');
}

struct FCasedStringViewKeyFuncs : public DefaultKeyFuncs<FStringView>
{
	static FORCEINLINE FStringView GetSetKey(FStringView K) { return K; }
	template <typename T>
	static FORCEINLINE FStringView GetSetKey(const TPair<FStringView, T>& P) { return P.Key; }
	static FORCEINLINE bool Matches(FStringView A, FStringView B) { return Equals(A, B); }
	static FORCEINLINE uint32 GetKeyHash(FStringView Key)
	{
		return FXxHash64::HashBuffer(Key.GetData(), Key.Len() * sizeof(*Key.GetData())).Hash;
	}
};

static void BuildLineBreakMap(FStringView Source, TArray<int32>& OutLineBreakMap)
{
	OutLineBreakMap.Reset();

	OutLineBreakMap.Add(0); // Lines numbers are 1-based, so add a dummy element to make UpperBound later return the line number directly

	const int32 SourceLen = Source.Len();
	const TCHAR* Chars = Source.GetData(); // avoid bounds check overhead in [] operator

	int32 Cursor = 0;

#if UE_SHADER_MINIFIER_SSE
	static_assert(sizeof(*Chars) == 2, "BuildLineBreakMap expects 16 bit characters");
	const int32 AlignedLen = SourceLen & (~7); // align down to multiple of 8 TCHAR-s
	const __m128i Needle = _mm_set1_epi16(L'\n');
	while (Cursor < AlignedLen)
	{
		__m128i Chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Chars + Cursor));
		__m128i MaskVec = _mm_cmpeq_epi16(Chunk, Needle);
		uint32 Mask = _mm_movemask_epi8(MaskVec);
		while (Mask != 0)
		{
			// NOTE: 2 bits represent each character
			const uint32 BitIndex = FMath::CountTrailingZeros(Mask);
			const uint32 ChunkCharIndex = BitIndex / 2;
			OutLineBreakMap.Add(Cursor + ChunkCharIndex);
			Mask &= ~(3 << BitIndex);
		}
		Cursor += 8;
	}
#endif //UE_SHADER_MINIFIER_SSE

	while (Cursor < SourceLen)
	{
		if (Chars[Cursor] == TCHAR('\n'))
		{
			OutLineBreakMap.Add(Cursor);
		}
		++Cursor;
	}
}

static int32 FindLineDirective(const TArray<FStringView>& LineDirectives, const TCHAR* Ptr)
{
	int32 FoundIndex = Algo::UpperBoundBy(LineDirectives, Ptr, [](FStringView Item)
	{
		return Item.GetData();
	});

	if (FoundIndex < 1 || FoundIndex > LineDirectives.Num())
	{
		return INDEX_NONE;
	}

	// UpperBound returns element that's greater than predicate, but we need the closest preceeding line directive.
	return FoundIndex - 1;
}

static int32 FindLineNumber(FStringView Source, const TArray<int32>& LineBreakMap, const TCHAR* Ptr)
{
	if (Ptr < Source.GetData() || Ptr >= Source.GetData() + Source.Len())
	{
		return INDEX_NONE;
	}

	const int32 Index = int32(Ptr - Source.GetData());

	int32 FoundLineNumber = Algo::UpperBound(LineBreakMap, Index);

	return FoundLineNumber;
}

static bool ParseLineDirective(FStringView Input, int32& OutLineNumber, FStringView& OutFileName)
{
	if (!StartsWith(Input, TEXTVIEW("#line")))
	{
		return false;
	}

	Input = Input.Mid(5); // skip `#line` itself
	Input = SkipSpace(Input);

	if (Input.IsEmpty() || !IsNumber(Input[0]))
	{
		return false;
	}

	OutLineNumber = FCString::Atoi(Input.GetData());

	int32 FileNameBeginIndex = INDEX_NONE;
	if (Input.FindChar(TCHAR('"'), FileNameBeginIndex))
	{
		int32 FileNameEndIndex = INDEX_NONE;
		Input.MidInline(FileNameBeginIndex + 1);
		if (Input.FindChar(TCHAR('"'), FileNameEndIndex))
		{
			OutFileName = Input.Mid(0, FileNameEndIndex);
		}
		else
		{
			return false;
		}
	}

	return true;
}

static void OpenNamespace(FString& OutputStream, const FNamespace& Namespace)
{
	for (const FStringView& Name : Namespace.Stack)
	{
		OutputStream.Append(TEXTVIEW("namespace "));
		OutputStream.Append(Name);
		OutputStream.Append(TEXTVIEW(" { "));
	}
}

static void CloseNamespace(FString& OutputStream, const FNamespace& Namespace)
{
	for (const FStringView& Name : Namespace.Stack)
	{
		OutputStream.AppendChar(L'}');
	}

	OutputStream.Append(TEXTVIEW(" // namespace "));
	OutputStream.Append(Namespace.FullName);
}

static FString MinifyShader(const FParsedShader& Parsed, TConstArrayView<FStringView> RequiredSymbols, EMinifyShaderFlags Flags, FDiagnostics& Diagnostics)
{
	FString OutputStream;

	OutputStream.Reserve(Parsed.Source.Len() / 3); // Heuristic pre-allocation based on average measured reduced code size

	TSet<FStringView, FCasedStringViewKeyFuncs, FDefaultSetAllocator> RelevantIdentifiers;

	TSet<const FCodeChunk*> RelevantChunks;
	TSet<FStringView, FCasedStringViewKeyFuncs, FDefaultSetAllocator>  ProcessedIdentifiers;

	TArray<const FCodeChunk*> PendingChunks;

	for (FStringView Entry : RequiredSymbols)
	{
		RelevantIdentifiers.Add(Entry);
		ProcessedIdentifiers.Add(Entry);
		FindChunksByIdentifier(Parsed.Chunks, Entry, [&PendingChunks](const FCodeChunk& Chunk) { PendingChunks.Push(&Chunk); });
	}

	for (const FCodeChunk* Chunk : PendingChunks)
	{
		RelevantChunks.Add(Chunk);
	}

	{
		// Some known builtin words to ignore
		ProcessedIdentifiers.Add(TEXT("asfloat"));
		ProcessedIdentifiers.Add(TEXT("asint"));
		ProcessedIdentifiers.Add(TEXT("asuint"));
		ProcessedIdentifiers.Add(TEXT("bool"));
		ProcessedIdentifiers.Add(TEXT("bool2"));
		ProcessedIdentifiers.Add(TEXT("bool3"));
		ProcessedIdentifiers.Add(TEXT("bool4"));
		ProcessedIdentifiers.Add(TEXT("break"));
		ProcessedIdentifiers.Add(TEXT("cbuffer"));
		ProcessedIdentifiers.Add(TEXT("const"));
		ProcessedIdentifiers.Add(TEXT("else"));
		ProcessedIdentifiers.Add(TEXT("extern"));
		ProcessedIdentifiers.Add(TEXT("false"));
		ProcessedIdentifiers.Add(TEXT("float"));
		ProcessedIdentifiers.Add(TEXT("float2"));
		ProcessedIdentifiers.Add(TEXT("float3"));
		ProcessedIdentifiers.Add(TEXT("float3x3"));
		ProcessedIdentifiers.Add(TEXT("float3x4"));
		ProcessedIdentifiers.Add(TEXT("float4"));
		ProcessedIdentifiers.Add(TEXT("float4x4"));
		ProcessedIdentifiers.Add(TEXT("for"));
		ProcessedIdentifiers.Add(TEXT("groupshared"));
		ProcessedIdentifiers.Add(TEXT("if"));
		ProcessedIdentifiers.Add(TEXT("in"));
		ProcessedIdentifiers.Add(TEXT("inout"));
		ProcessedIdentifiers.Add(TEXT("int"));
		ProcessedIdentifiers.Add(TEXT("int2"));
		ProcessedIdentifiers.Add(TEXT("int3"));
		ProcessedIdentifiers.Add(TEXT("int4"));
		ProcessedIdentifiers.Add(TEXT("interface"));
		ProcessedIdentifiers.Add(TEXT("out"));
		ProcessedIdentifiers.Add(TEXT("packoffset"));
		ProcessedIdentifiers.Add(TEXT("precise"));
		ProcessedIdentifiers.Add(TEXT("register"));
		ProcessedIdentifiers.Add(TEXT("return"));
		ProcessedIdentifiers.Add(TEXT("static"));
		ProcessedIdentifiers.Add(TEXT("struct"));
		ProcessedIdentifiers.Add(TEXT("switch"));
		ProcessedIdentifiers.Add(TEXT("tbuffer"));
		ProcessedIdentifiers.Add(TEXT("true"));
		ProcessedIdentifiers.Add(TEXT("uint"));
		ProcessedIdentifiers.Add(TEXT("uint2"));
		ProcessedIdentifiers.Add(TEXT("uint3"));
		ProcessedIdentifiers.Add(TEXT("uint4"));
		ProcessedIdentifiers.Add(TEXT("void"));
		ProcessedIdentifiers.Add(TEXT("while"));
		ProcessedIdentifiers.Add(TEXT("typedef"));
		ProcessedIdentifiers.Add(TEXT("template"));
		ProcessedIdentifiers.Add(TEXT("operator"));
		ProcessedIdentifiers.Add(TEXT("enum"));

		// HLSL resource types
		ProcessedIdentifiers.Add(TEXT("TextureCubeArray"));
		ProcessedIdentifiers.Add(TEXT("TextureCube"));
		ProcessedIdentifiers.Add(TEXT("TextureBuffer"));
		ProcessedIdentifiers.Add(TEXT("Texture3D"));
		ProcessedIdentifiers.Add(TEXT("Texture2DMSArray"));
		ProcessedIdentifiers.Add(TEXT("Texture2DMS"));
		ProcessedIdentifiers.Add(TEXT("Texture2DArray"));
		ProcessedIdentifiers.Add(TEXT("Texture2D"));
		ProcessedIdentifiers.Add(TEXT("Texture1DArray"));
		ProcessedIdentifiers.Add(TEXT("Texture1D"));
		ProcessedIdentifiers.Add(TEXT("StructuredBuffer"));
		ProcessedIdentifiers.Add(TEXT("SamplerState"));
		ProcessedIdentifiers.Add(TEXT("SamplerComparisonState"));
		ProcessedIdentifiers.Add(TEXT("RWTextureCubeArray"));
		ProcessedIdentifiers.Add(TEXT("RWTextureCube"));
		ProcessedIdentifiers.Add(TEXT("RWTexture3D"));
		ProcessedIdentifiers.Add(TEXT("RWTexture2DMSArray"));
		ProcessedIdentifiers.Add(TEXT("RWTexture2DMS"));
		ProcessedIdentifiers.Add(TEXT("RWTexture2DArray"));
		ProcessedIdentifiers.Add(TEXT("RWTexture2D"));
		ProcessedIdentifiers.Add(TEXT("RWTexture1DArray"));
		ProcessedIdentifiers.Add(TEXT("RWTexture1D"));
		ProcessedIdentifiers.Add(TEXT("RWStructuredBuffer"));
		ProcessedIdentifiers.Add(TEXT("RWByteAddressBuffer"));
		ProcessedIdentifiers.Add(TEXT("RWBuffer"));
		ProcessedIdentifiers.Add(TEXT("RaytracingAccelerationStructure"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedTexture3D"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedTexture2DArray"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedTexture2D"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedTexture1DArray"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedTexture1D"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedStructuredBuffer"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedByteAddressBuffer"));
		ProcessedIdentifiers.Add(TEXT("RasterizerOrderedBuffer"));
		ProcessedIdentifiers.Add(TEXT("FeedbackTexture2DArray"));
		ProcessedIdentifiers.Add(TEXT("FeedbackTexture2D"));
		ProcessedIdentifiers.Add(TEXT("ConsumeStructuredBuffer"));
		ProcessedIdentifiers.Add(TEXT("ConstantBuffer"));
		ProcessedIdentifiers.Add(TEXT("ByteAddressBuffer"));
		ProcessedIdentifiers.Add(TEXT("Buffer"));
		ProcessedIdentifiers.Add(TEXT("AppendStructuredBuffer"));

		// Alternative spelling of some resource types
		ProcessedIdentifiers.Add(TEXT("AppendRegularBuffer"));
		ProcessedIdentifiers.Add(TEXT("ByteBuffer"));
		ProcessedIdentifiers.Add(TEXT("ConsumeRegularBuffer"));
		ProcessedIdentifiers.Add(TEXT("DataBuffer"));
		ProcessedIdentifiers.Add(TEXT("MS_Texture2D"));
		ProcessedIdentifiers.Add(TEXT("MS_Texture2D_Array"));
		ProcessedIdentifiers.Add(TEXT("RegularBuffer"));
		ProcessedIdentifiers.Add(TEXT("RW_ByteBuffer"));
		ProcessedIdentifiers.Add(TEXT("RW_DataBuffer"));
		ProcessedIdentifiers.Add(TEXT("RW_RegularBuffer"));
		ProcessedIdentifiers.Add(TEXT("RW_Texture1D"));
		ProcessedIdentifiers.Add(TEXT("RW_Texture1D_Array"));
		ProcessedIdentifiers.Add(TEXT("RW_Texture2D"));
		ProcessedIdentifiers.Add(TEXT("RW_Texture2D_Array"));
		ProcessedIdentifiers.Add(TEXT("RW_Texture3D"));
		ProcessedIdentifiers.Add(TEXT("RW_TextureCube"));
		ProcessedIdentifiers.Add(TEXT("Texture1D_Array"));
		ProcessedIdentifiers.Add(TEXT("Texture2D_Array"));
		ProcessedIdentifiers.Add(TEXT("TextureBuffer"));
		ProcessedIdentifiers.Add(TEXT("TextureCube_Array"));

		// Some shaders define template versions of some built-in functions, so we can't trivially ignore them
		//ProcessedIdentifiers.Add(TEXT("abs"));
		//ProcessedIdentifiers.Add(TEXT("any"));
		//ProcessedIdentifiers.Add(TEXT("clamp"));
		//ProcessedIdentifiers.Add(TEXT("clip"));
		//ProcessedIdentifiers.Add(TEXT("cos"));
		//ProcessedIdentifiers.Add(TEXT("cross"));
		//ProcessedIdentifiers.Add(TEXT("dot"));
		//ProcessedIdentifiers.Add(TEXT("frac"));
		//ProcessedIdentifiers.Add(TEXT("lerp"));
		//ProcessedIdentifiers.Add(TEXT("max"));
		//ProcessedIdentifiers.Add(TEXT("min"));
		//ProcessedIdentifiers.Add(TEXT("mul"));
		//ProcessedIdentifiers.Add(TEXT("normalize"));
		//ProcessedIdentifiers.Add(TEXT("pow"));
		//ProcessedIdentifiers.Add(TEXT("saturate"));
		//ProcessedIdentifiers.Add(TEXT("sign"));
		//ProcessedIdentifiers.Add(TEXT("sin"));
		//ProcessedIdentifiers.Add(TEXT("sqrt"));
	}

	if (PendingChunks.IsEmpty())
	{
		// Entry point chunk is not found in the shader
		return {};
	}

	TArray<FStringView> TempIdentifiers;

	TMap<FStringView, TArray<const FCodeChunk*>, FDefaultSetAllocator, FCasedStringViewKeyFuncs> ChunksByIdentifier;
	for (const FCodeChunk& Chunk : Parsed.Chunks)
	{
		for (const FCodeBlock& Block : Chunk.Blocks)
		{
			if (Block.Type == EBlockType::Keyword)
			{
				continue;
			}

			if (Chunk.Type == ECodeChunkType::Function && Block.Type != EBlockType::Name)
			{
				continue;
			}

			if (Chunk.Type == ECodeChunkType::Struct && Block.Type != EBlockType::Type)
			{
				continue;
			}

			if (Chunk.Type == ECodeChunkType::Typedef && Block.Type != EBlockType::Name)
			{
				continue;
			}

			if ((Chunk.Type == ECodeChunkType::CBuffer || Chunk.Type == ECodeChunkType::Enum) && Block.Type == EBlockType::Body)
			{
				TempIdentifiers.Reset();
				ExtractIdentifiers(Block, TempIdentifiers);
				for (FStringView Identifier : TempIdentifiers)
				{
					ChunksByIdentifier.FindOrAdd(Identifier).Push(&Chunk);
				}

				continue;
			}

			if (Chunk.Type == ECodeChunkType::Operator
				&& Block.Type != EBlockType::Type
				&& Block.Type != EBlockType::Args)
			{
				continue;
			}

			ChunksByIdentifier.FindOrAdd(Block).Push(&Chunk);
		}
	}

	TMap<const FCodeChunk*, const FCodeChunk*> ChunkRequestedBy;

	while (!PendingChunks.IsEmpty())
	{
		TempIdentifiers.Reset();

		const FCodeChunk* CurrentChunk = PendingChunks.Last();
		PendingChunks.Pop();

		ExtractIdentifiers(*CurrentChunk, TempIdentifiers);

		for (FStringView Identifier : TempIdentifiers)
		{
			bool bIdentifierWasAlreadyInSet = false;
			ProcessedIdentifiers.Add(Identifier, &bIdentifierWasAlreadyInSet);
			if (!bIdentifierWasAlreadyInSet)
			{
				auto FoundChunks = ChunksByIdentifier.Find(Identifier);
				if (FoundChunks == nullptr)
				{
					continue;
				}
				for (const FCodeChunk* Chunk : *FoundChunks)
				{
					if (Chunk == CurrentChunk)
					{
						continue;
					}

					bool bChunkWasAlreadyInSet = false;
					RelevantChunks.Add(Chunk, &bChunkWasAlreadyInSet);
					if (!bChunkWasAlreadyInSet)
					{
						PendingChunks.Push(Chunk);

						if (Chunk->Type == ECodeChunkType::Function 
							|| Chunk->Type == ECodeChunkType::Struct
							|| Chunk->Type == ECodeChunkType::CBuffer
							|| Chunk->Type == ECodeChunkType::Variable)
						{
							ChunkRequestedBy.FindOrAdd(Chunk) = CurrentChunk;
						}
					}
				}
			}
		}
	}

	uint32 NumFunctions		= 0;
	uint32 NumStructs		= 0;
	uint32 NumVariables		= 0;
	uint32 NumCBuffers		= 0;
	uint32 NumOtherChunks	= 0;

	for (const FCodeChunk& Chunk : Parsed.Chunks)
	{
		if (RelevantChunks.Find(&Chunk) == nullptr)
		{
			continue;
		}

		if (Chunk.Type == ECodeChunkType::Function)
		{
			NumFunctions += 1;
		}
		else if (Chunk.Type == ECodeChunkType::Struct)
		{
			NumStructs += 1;
		}
		else if (Chunk.Type == ECodeChunkType::Variable)
		{
			NumVariables += 1;
		}
		else if (Chunk.Type == ECodeChunkType::CBuffer)
		{
			NumCBuffers += 1;
		}
		else
		{
			NumOtherChunks += 1;
		}
	}

	if (EnumHasAnyFlags(Flags, EMinifyShaderFlags::OutputStats))
	{
		OutputStream.Append(TEXTVIEW("// Total code chunks: "));
		OutputStream.AppendInt(RelevantChunks.Num());
		OutputStream.AppendChar(L'\n');

		OutputStream.Append(TEXTVIEW("// - Functions: "));
		OutputStream.AppendInt(NumFunctions);
		OutputStream.AppendChar(L'\n');

		OutputStream.Append(TEXTVIEW("// - Structs: "));
		OutputStream.AppendInt(NumStructs);
		OutputStream.AppendChar(L'\n');

		OutputStream.Append(TEXTVIEW("// - CBuffers: "));
		OutputStream.AppendInt(NumCBuffers);
		OutputStream.AppendChar(L'\n');

		OutputStream.Append(TEXTVIEW("// - Variables: "));
		OutputStream.AppendInt(NumVariables);
		OutputStream.AppendChar(L'\n');

		OutputStream.Append(TEXTVIEW("// - Other: "));
		OutputStream.AppendInt(NumOtherChunks);
		OutputStream.AppendChar(L'\n');

		OutputStream.AppendChar(L'\n');
	}

	TArray<int32> LineBreakMap;
	const TArray<FStringView>& LineDirectives = Parsed.LineDirectives;
	if (EnumHasAnyFlags(Flags, EMinifyShaderFlags::OutputLines))
	{
		BuildLineBreakMap(Parsed.Source, LineBreakMap);
	}

	const FNamespace* CurrentNamespace = nullptr;

	int32 LastLineNumber = -1;
	FStringView LastLineFileName;

	for (const FCodeChunk& Chunk : Parsed.Chunks)
	{
		auto ShouldSkipChunk = [&RelevantChunks, &Chunk, Flags]()
		{
			// Pragmas and defines that remain after preprocessing must be preserved as they may control important compiler behaviors.
			if (Chunk.Type == ECodeChunkType::Pragma || Chunk.Type == ECodeChunkType::Define)
			{
				return false;
			}

			// The preprocessed shader may have auto-generated comments such as `// #define FOO 123` that may be useful to keep for debugging.
			if (Chunk.Type == ECodeChunkType::CommentLine && EnumHasAnyFlags(Flags, EMinifyShaderFlags::OutputCommentLines))
			{
				return false;
			}

			// Always include `using` statements if they are present in the global scope
			if (Chunk.Type == ECodeChunkType::Using)
			{
				return false;
			}

			if (RelevantChunks.Find(&Chunk))
			{
				return false;
			}

			return true;
		};

		if (ShouldSkipChunk())
		{
			continue;
		}

		const FNamespace* PendingNamespace = Chunk.Namespace != INDEX_NONE ? &Parsed.Namespaces[Chunk.Namespace] : nullptr;

		if (PendingNamespace != CurrentNamespace)
		{
			if (CurrentNamespace)
			{
				CloseNamespace(OutputStream, *CurrentNamespace);
				OutputStream.Append(TEXTVIEW("\n\n"));
			}

			if (PendingNamespace)
			{
				OpenNamespace(OutputStream, *PendingNamespace);
				OutputStream.Append(TEXTVIEW("\n\n"));
			}

			CurrentNamespace = PendingNamespace;
		}

		if (EnumHasAnyFlags(Flags, EMinifyShaderFlags::OutputReasons))
		{
			auto RequestedBy = ChunkRequestedBy.Find(&Chunk);
			if (RequestedBy != nullptr)
			{
				const FCodeChunk* RequestedByChunk = *RequestedBy;
				FStringView  RequestedByName = RequestedByChunk->FindFirstBlockByType(EBlockType::Name);
				if (!RequestedByName.IsEmpty())
				{
					OutputStream.Append(TEXTVIEW("// REASON: "));
					OutputStream.Append(RequestedByName);
					OutputStream.AppendChar(L'\n');
				}
			}
		}

		if (EnumHasAnyFlags(Flags, EMinifyShaderFlags::OutputLines))
		{
			const FStringView ChunkCode = Chunk.Blocks[0];
			int32 LineDirectiveIndex = FindLineDirective(LineDirectives, ChunkCode.GetData());
			int32 ChunkLine = FindLineNumber(Parsed.Source, LineBreakMap, ChunkCode.GetData());

			if (ChunkLine != INDEX_NONE && LineDirectiveIndex == INDEX_NONE)
			{
				// There was no valid line directive for this chunk, but we do know the line in the input source, so just emit that.
				if (ChunkLine > LastLineNumber + 1 || !LastLineFileName.IsEmpty())
				{
					OutputStream.Append(TEXTVIEW("#line "));
					OutputStream.AppendInt(ChunkLine);
					OutputStream.AppendChar(L'\n');
				}

				LastLineNumber = ChunkLine;
				LastLineFileName = FStringView();
			}
			else if (ChunkLine != INDEX_NONE && LineDirectiveIndex != INDEX_NONE)
			{
				// We have a valid line directive and line number in the input source.
				// Some of the input source code may have been removed, so we need to adjust 
				// the line number before emitting the line directive.

				FStringView LineDirective = LineDirectives[LineDirectiveIndex];
				int32 LineDirectiveLine = FindLineNumber(Parsed.Source, LineBreakMap, LineDirective.GetData());
				int32 ParsedLineNumber = INDEX_NONE;
				FStringView ParsedFileName;
				if (LineDirectiveLine != INDEX_NONE && ParseLineDirective(LineDirective, ParsedLineNumber, ParsedFileName))
				{
					int32 OffsetFromLineDirective = ChunkLine - (LineDirectiveLine + 1); // Line directive identifies the *next* line, hence +1 when computing the offset
					int32 PatchedLineNumber = ParsedLineNumber + OffsetFromLineDirective;

					if (PatchedLineNumber > LastLineNumber + 1 || LastLineFileName != ParsedFileName)
					{
						// Separate the next block from the previous one when it starts with a line directive
						if (OutputStream.Len())
						{
							OutputStream.AppendChar(L'\n');
						}

						OutputStream.Append(TEXTVIEW("#line "));
						OutputStream.AppendInt(PatchedLineNumber);

						if (!ParsedFileName.IsEmpty())
						{
							OutputStream.Append(TEXTVIEW(" \""));
							OutputStream.Append(ParsedFileName);
							OutputStream.AppendChar(L'\"');
						}
						OutputStream.AppendChar(L'\n');
					}

					LastLineNumber = PatchedLineNumber;
					LastLineFileName = ParsedFileName;
				}
			}
		}

		OutputChunk(Chunk, OutputStream);
	}

	if (CurrentNamespace)
	{
		CloseNamespace(OutputStream, *CurrentNamespace);
		OutputStream.AppendChar(L'\n');
		CurrentNamespace = nullptr;
	}

	return OutputStream;
}

static FString MinifyShader(const FParsedShader& Parsed, FStringView EntryPoint, EMinifyShaderFlags Flags, FDiagnostics& Diagnostics)
{
	TArray<FStringView> RequiredSymbols = SplitByChar(EntryPoint, ';');
	return MinifyShader(Parsed, RequiredSymbols, Flags, Diagnostics);
}

FMinifiedShader Minify(const FStringView PreprocessedShader, TConstArrayView<FStringView> RequiredSymbols, EMinifyShaderFlags Flags)
{
	FMinifiedShader Result;

	FParsedShader Parsed = ParseShader(PreprocessedShader, Result.Diagnostics);

	if (!Parsed.Chunks.IsEmpty())
	{
		Result.Code = MinifyShader(Parsed, RequiredSymbols, Flags, Result.Diagnostics);
	}

	return Result;
}

FMinifiedShader Minify(const FStringView PreprocessedShader, const FStringView EntryPoint, EMinifyShaderFlags Flags)
{
	return Minify(PreprocessedShader, MakeArrayView(&EntryPoint, 1), Flags);
}

} // namespace UE::ShaderMinifier

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaderMinifierParserTest, "System.Shaders.ShaderMinifier.Parse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

namespace UE::ShaderMinifier
{
// Convenience wrapper for tests where we don't care about diagnostic messages
static FParsedShader ParseShader(FStringView InSource)
{
	FDiagnostics Diagnostics;
	return ParseShader(InSource, Diagnostics);
}
}

bool FShaderMinifierParserTest::RunTest(const FString& Parameters)
{
	using namespace UE::ShaderMinifier;

	TestEqual(TEXT("SkipSpace"), 
		FString(SkipSpace(TEXT("  \n\r\f \tHello"))), 
		FString(TEXT("Hello")));

	TestEqual(TEXT("SkipUntilStr (found)"), 
		FString(SkipUntilStr(TEXT("Hello World"), TEXT("World"))),
		FString(TEXT("World")));

	TestEqual(TEXT("SkipUntilStr (not found)"),
		FString(SkipUntilStr(TEXT("Hello World"), TEXT("Blah"))),
		FString());

	{
		auto P = ParseShader(TEXT("static const struct { int Blah; } Foo = { 123; };"));
		TestEqual(TEXT("Anonymous struct variable with initializer, total chunks"), P.Chunks.Num(), 1);
		if (P.Chunks.Num() == 1)
		{
			TestEqual(TEXT("Anonymous struct variable with initializer, main chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("float4 PSMain() : SV_Target { return float4(1,0,0,1); };"));
		TestEqual(TEXT("Pixel shader entry point, total chunks"), P.Chunks.Num(), 1);
		if (P.Chunks.Num() == 1)
		{
			TestEqual(TEXT("Pixel shader entry point, main chunk type"), P.Chunks[0].Type, ECodeChunkType::Function);
		}
	}

	{
		TArray<FStringView> R;
		ExtractIdentifiers(TEXT("Hello[World]; Foo[0];\n"), R);
		if (TestEqual(TEXT("ExtractIdentifiers1: Num"), R.Num(), 3))
		{
			TestEqual(TEXT("ExtractIdentifiers1: R[0]"), FString(R[0]), TEXT("Hello"));
			TestEqual(TEXT("ExtractIdentifiers1: R[1]"), FString(R[1]), TEXT("World"));
			TestEqual(TEXT("ExtractIdentifiers1: R[2]"), FString(R[2]), TEXT("Foo"));
		}
	}

	{
		TArray<FStringView> R;
		ExtractIdentifiers(TEXT("#line 0\nStructuredBuffer<uint4> Blah : register(t0, space123);#line 1\n#pragma foo\n"), R);
		if (TestEqual(TEXT("ExtractIdentifiers2: Num"), R.Num(), 6))
		{
			TestEqual(TEXT("ExtractIdentifiers2: R[0]"), FString(R[0]), TEXT("StructuredBuffer"));
			TestEqual(TEXT("ExtractIdentifiers2: R[1]"), FString(R[1]), TEXT("uint4"));
			TestEqual(TEXT("ExtractIdentifiers2: R[2]"), FString(R[2]), TEXT("Blah"));
			TestEqual(TEXT("ExtractIdentifiers2: R[3]"), FString(R[3]), TEXT("register"));
			TestEqual(TEXT("ExtractIdentifiers2: R[4]"), FString(R[4]), TEXT("t0"));
			TestEqual(TEXT("ExtractIdentifiers2: R[5]"), FString(R[5]), TEXT("space123"));
		}
	}

	{
		auto P = ParseShader(TEXT("StructuredBuffer<uint4> Blah : register(t0, space123);"));
		if (TestEqual(TEXT("ParseShader: structured buffer: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: structured buffer: chunk"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("const float Foo = 123.45f;"));
		if (TestEqual(TEXT("ParseShader: const float with initializer: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: const float with initializer: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("struct Blah { int A; };"));
		if (TestEqual(TEXT("ParseShader: struct: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: struct: chunk type"), P.Chunks[0].Type, ECodeChunkType::Struct);
		}
	}

	{
		auto P = ParseShader(TEXT("struct Foo { int FooA; }; struct Bar : Foo { int BarA; };"));
		if (TestEqual(TEXT("ParseShader: inherited struct: num chunks"), P.Chunks.Num(), 2))
		{
			TestEqual(TEXT("ParseShader: inherited struct: chunk 0 type"), P.Chunks[0].Type, ECodeChunkType::Struct);
			TestEqual(TEXT("ParseShader: inherited struct: chunk 1 type"), P.Chunks[1].Type, ECodeChunkType::Struct);
		}
	}

	{
		auto P = ParseShader(TEXT("[numthreads(8,8,1)] void Main() {};"));
		if (TestEqual(TEXT("ParseShader: compute shader entry point: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: compute shader entry point: chunk type"), P.Chunks[0].Type, ECodeChunkType::Function);

			if (TestEqual(TEXT("ParseShader: compute shader entry point: num blocks"), P.Chunks[0].Blocks.Num(), 5))
			{
				TestEqual(TEXT("ParseShader: compute shader entry point: attribute block type"), P.Chunks[0].Blocks[0].Type, EBlockType::Attribute);
			}
		}
	}

	{
		auto P = ParseShader(TEXT("Texture2D Blah : register(t0);"));
		if (TestEqual(TEXT("ParseShader: texture with register: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: texture with register: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}
	{
		auto P = ParseShader(TEXT("Texture2D Blah;"));
		if (TestEqual(TEXT("ParseShader: texture: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: texture: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("SamplerState Blah : register(s0, space123);"));
		if (TestEqual(TEXT("ParseShader: sampler state with register: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: sampler state with register: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

#if 0
	{
		// TODO: handle function forward declarations
		auto P = ParseShader(TEXT("Foo Fun(int a);"));
		TestEqual(TEXT("ParseShader: function forward declaration"), P.Chunks[0].Type, ECodeChunkType::FunctionDecl);
	}
#endif

	{
		auto P = ParseShader(TEXT("void Fun(int a) {};"));
		if (TestEqual(TEXT("ParseShader: function with trailing semicolon: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: function with trailing semicolon: chunk type"), P.Chunks[0].Type, ECodeChunkType::Function);
		}
	}

	{
		auto P = ParseShader(TEXT("void Fun(int a) {}"));
		if (TestEqual(TEXT("ParseShader: function: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: function: chunk type"), P.Chunks[0].Type, ECodeChunkType::Function);
		}
	}

	{
		auto P = ParseShader(TEXT("cbuffer Foo {blah} SamplerState S;"));

		if (TestEqual(TEXT("ParseShader: cbuffer and sampler state: num chunks"), P.Chunks.Num(), 2))
		{
			TestEqual(TEXT("ParseShader: cbuffer and sampler state: chunk type [0]"), P.Chunks[0].Type, ECodeChunkType::CBuffer);
			TestEqual(TEXT("ParseShader: cbuffer and sampler state: chunk type [1]"), P.Chunks[1].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("struct Foo { int a; };"));
		if (TestEqual(TEXT("ParseShader: struct: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: struct: chunk type"), P.Chunks[0].Type, ECodeChunkType::Struct);
		}
	}

	{
		auto P = ParseShader(TEXT("struct { int a; } Foo = { 123; };"));
		if (TestEqual(TEXT("ParseShader: anonymous struct with variable and initializer: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: anonymous struct with variable and initializer: chunk type [0]"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

#if 0
	{
		// TODO: handle struct forward declarations
		auto P = ParseShader(TEXT("struct Foo;"));
	}
#endif

	{
		auto P = ParseShader(
			TEXT("cbuffer MyBuffer : register(b3)")
			TEXT("{ float4 Element1 : packoffset(c0); float1 Element2 : packoffset(c1); float1 Element3 : packoffset(c1.y); }"));
		if (TestEqual(TEXT("ParseShader: cbuffer with packoffset: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: cbuffer with packoffset: chunk type"), P.Chunks[0].Type, ECodeChunkType::CBuffer);
		}
	}

	{
		auto P = ParseShader(TEXT("static const struct { float4 Param; } Foo;"));
		if (TestEqual(TEXT("ParseShader: static const anonymous struct with variable: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: static const anonymous struct with variable: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("static const struct { float4 Param; } Foo = { FooCB_Param; };"));
		if (TestEqual(TEXT("ParseShader: static const anonymous struct with variable and initializer: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: static const anonymous struct with variable and initializer: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("template <typename T> float Fun(T x) { return (float)x; }"));
		if (TestEqual(TEXT("ParseShader: template function: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: template function: chunk type"), P.Chunks[0].Type, ECodeChunkType::Function);
		}
	}

	{
		auto P = ParseShader(TEXT("enum EFoo { A, B = 123 };"));
		if (TestEqual(TEXT("ParseShader: enum: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: enum: chunk type"), P.Chunks[0].Type, ECodeChunkType::Enum);
		}
	}

	{
		auto P = ParseShader(TEXT("enum class EFoo { A, B };"));
		if (TestEqual(TEXT("ParseShader: enum class: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: enum class: chunk type"), P.Chunks[0].Type, ECodeChunkType::Enum);
		}
	}

	{
		auto P = ParseShader(TEXT("#define Foo 123"));
		if (TestEqual(TEXT("ParseShader: define: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: define: chunk type"), P.Chunks[0].Type, ECodeChunkType::Define);
		}
	}

	{
		auto P = ParseShader(TEXT("#pragma Foo"));
		if (TestEqual(TEXT("ParseShader: pragma: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: pragma: chunk type"), P.Chunks[0].Type, ECodeChunkType::Pragma);
		}
	}

	{
		auto P = ParseShader(TEXT("ConstantBuffer<Foo> CB : register ( b123, space456);"));
		if (TestEqual(TEXT("ParseShader: ConstantBuffer<Foo>: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: ConstantBuffer<Foo>: chunk type"), P.Chunks[0].Type, ECodeChunkType::Variable);
		}
	}

	{
		auto P = ParseShader(TEXT("namespace NS1 { void Fun() {}; } namespace NS2 { void Fun() {}; }"));
		if (TestEqual(TEXT("ParseShader: namespaces: num chunks"), P.Chunks.Num(), 2)
			&& TestEqual(TEXT("ParseShader: namespaces: num namespaces"), P.Namespaces.Num(), 2))
		{
			TestEqual(TEXT("ParseShader: namespaces: chunk 0 namespace"), P.Chunks[0].Namespace, 0);
			TestEqual(TEXT("ParseShader: namespaces: chunk 1 namespace"), P.Chunks[1].Namespace, 1);
		}
	}

	{
		auto P = ParseShader(TEXT("template< typename T > TMyStruct<T> operator + ( TMyStruct<T> A, T B ) { /*...*/ }"));
		if (TestEqual(TEXT("ParseShader: operators: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: operators: chunk type"), P.Chunks[0].Type, ECodeChunkType::Operator);
			if (TestEqual(TEXT("ParseShader: operators: chunk 0: num blocks"), P.Chunks[0].Blocks.Num(), 8))
			{
				FString Args = FString(P.Chunks[0].FindFirstBlockByType(EBlockType::Args));
				TestEqual(TEXT("ParseShader: operators: chunk 0: type name"), *Args, TEXT("( TMyStruct<T> A, T B )"));

				FString TypeName = FString(P.Chunks[0].FindFirstBlockByType(EBlockType::Type));
				TestEqual(TEXT("ParseShader: operators: chunk 0: type name"), *TypeName, TEXT("TMyStruct"));

				FString OperatorName = FString(P.Chunks[0].FindFirstBlockByType(EBlockType::OperatorName));
				TestEqual(TEXT("ParseShader: operators: chunk 0: operator name"), *OperatorName, TEXT("+"));
			}
		}
	}

	{
		auto P = ParseShader(TEXT("typedef Bar Foo;"));
		if (TestEqual(TEXT("ParseShader: standard typedef: num chunks"), P.Chunks.Num(), 1))
		{
			TestEqual(TEXT("ParseShader: standard typedef: chunk type"), P.Chunks[0].Type, ECodeChunkType::Typedef);
		}
	}

	{
		auto P = ParseShader(TEXT("typedef Bar Foo; static const Foo = Bar(0);"));
		if (TestEqual(TEXT("ParseShader: standard typedef: num chunks"), P.Chunks.Num(), 2))
		{
			TestEqual(TEXT("ParseShader: standard typedef: chunk type"), P.Chunks[0].Type, ECodeChunkType::Typedef);
			TestEqual(TEXT("ParseShader: standard typedef: chunk type"), P.Chunks[1].Type, ECodeChunkType::Variable);
		}
	}

	int32 NumErrors = ExecutionInfo.GetErrorTotal();

	return NumErrors == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShaderMinifierTest, "System.Shaders.ShaderMinifier.Minify", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FShaderMinifierTest::RunTest(const FString& Parameters)
{
	using namespace UE::ShaderMinifier;

	FStringView TestShaderCode = 
		TEXT(R"(// dxc /T cs_6_6 /E MainCS MinifierTest.hlsl 
struct FFoo
{
	float X;
	float Y;
};

#pragma test_pragma
struct FBar
{
	FFoo Foo;
};

uint GUnreferencedParameter;

struct FUnreferencedStruct
{
	uint X;
};

uint UnreferencedFunction()
{
	return GUnreferencedParameter;
}

typedef Texture2D<float4> UnreferencedTypedef;

#define COMPILER_DEFINITION_TEST 123
float Sum(in FBar Param)
{
	return Param.Foo.X + Param.Foo.Y;
}

float FunA()
{
	// Comment inside function
	FBar Temp;
	Temp.Foo.X = 1;
	Temp.Foo.Y = 2;
	return Sum(Temp);
}

float FunB(int Param)
{
	return FunA() * (float)Param;
}

#line 1000 "MinifierTest.hlsl"
// Test comment 1
void EmptyFunction(){}

struct
{
	int Foo;
	int Bar;
} GAnonymousStruct;

struct FStructA
{
	int Foo;
	int Bar;
} GStructA;

namespace NS1 {
namespace NS2 {
static const struct FStructB
{
	int Foo;
} GStructB = {123};

static const struct FStructC
{
	int Foo;
} GStructC = { GStructA.Foo };
}} // NS1::NS2

namespace NS3 {
static const struct
{
	int Foo;
} GInitializedAnonymousStructA = { GStructA.Foo };
} // NS3

static const struct
{
	int Foo;
} GInitializedAnonymousStructB = { 123 };

typedef RWBuffer<float4> OutputBufferType;
OutputBufferType OutputBuffer;

struct FTypedefUsedStruct
{
	float Foo;
};
typedef StructuredBuffer<FTypedefUsedStruct> FTypedefUsed;
typedef FTypedefUsed FTypedefUsedChained;
typedef FTypedefUsedChained FTypedefUsedChainedUnused;
FTypedefUsedChained TypedefUsedBuffer;

struct FTypedefUnusedStruct
{
	float Foo;
};
typedef StructuredBuffer<FTypedefUnusedStruct> FTypedefUnused;
FTypedefUnused TypedefUnusedBuffer;

template<typename T> struct TUsedTemplate { T Value; };
template<typename T> TUsedTemplate<T> operator*(TUsedTemplate<T> A, T B) { return (TUsedTemplate<T>)0; }

template<typename T> struct TUnusedTemplate { T Value[2]; };
template<typename T> TUnusedTemplate<T> operator%(TUnusedTemplate<T> A, T B) { return (TUnusedTemplate<T>)0; }

enum EEnumUsed : int
{
	ENUM_USED_PART_1 = 0,
	ENUM_USED_PART_2 = 1,
};

enum EEnumUnused
{
	ENUM_UNUSED_PART_1,
	ENUM_UNUSED_PART_2,
};

// Test comment 2
[numthreads(1,1,1)]
// Comment during function declaration
void MainCS()
{
	using namespace NS1::NS2;
	using namespace NS3;
	TUsedTemplate Foo;
	float A = FunB(GAnonymousStruct.Foo);
	float B = FunB(GStructA.Bar + GStructB.Foo + GStructC.Foo);
	float C = FunB(GInitializedAnonymousStructA.Foo + GInitializedAnonymousStructB.Foo);
	float D = TypedefUsedBuffer[ENUM_USED_PART_1].Foo;
	OutputBuffer[0] = A + B + D;
}
)");

	auto ChunkPresent = [](const FParsedShader& Parsed, FStringView Name)
	{
		for (const FCodeChunk& Chunk : Parsed.Chunks)
		{
			for (const FCodeBlock& Block : Chunk.Blocks)
			{
				if (Block == Name)
				{
					return true;
				}
			}
		}
		return false;
	};

	FParsedShader Parsed = ParseShader(TestShaderCode);

	{
		FDiagnostics Diagnostics;
		FString Minified = MinifyShader(Parsed, TEXT("EmptyFunction"), EMinifyShaderFlags::None, Diagnostics);
		FParsedShader MinifiedParsed = ParseShader(Minified);
		if (TestEqual(TEXT("MinifyShader: EmptyFunction: num chunks"), MinifiedParsed.Chunks.Num(), 3))
		{
			TestEqual(TEXT("MinifyShader: EmptyFunction: pragma"), *FString(MinifiedParsed.Chunks[0].GetCode()), TEXT("#pragma test_pragma"));
			TestEqual(TEXT("MinifyShader: EmptyFunction: define"), *FString(MinifiedParsed.Chunks[1].GetCode()), TEXT("#define COMPILER_DEFINITION_TEST 123"));
			TestEqual(TEXT("MinifyShader: EmptyFunction: function"), *FString(MinifiedParsed.Chunks[2].GetCode()), TEXT("void EmptyFunction(){}"));
		}
	}

	{
		FDiagnostics Diagnostics;
		FString Minified = MinifyShader(Parsed, TEXT("MainCS"), EMinifyShaderFlags::OutputReasons, Diagnostics);
		FParsedShader MinifiedParsed = ParseShader(Minified);

		// Expect true:
		TestTrue(TEXT("MinifyShader: MainCS: contains MainCS"), ChunkPresent(MinifiedParsed, TEXT("MainCS")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FFoo"), ChunkPresent(MinifiedParsed, TEXT("FFoo")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FBar"), ChunkPresent(MinifiedParsed, TEXT("FBar")));
		TestTrue(TEXT("MinifyShader: MainCS: contains Sum"), ChunkPresent(MinifiedParsed, TEXT("Sum")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FunA"), ChunkPresent(MinifiedParsed, TEXT("FunA")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FunB"), ChunkPresent(MinifiedParsed, TEXT("FunB")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GAnonymousStruct"), ChunkPresent(MinifiedParsed, TEXT("GAnonymousStruct")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GStructA"), ChunkPresent(MinifiedParsed, TEXT("GStructA")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GStructB"), ChunkPresent(MinifiedParsed, TEXT("GStructB")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GStructC"), ChunkPresent(MinifiedParsed, TEXT("GStructC")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GInitializedAnonymousStructA"), ChunkPresent(MinifiedParsed, TEXT("GInitializedAnonymousStructA")));
		TestTrue(TEXT("MinifyShader: MainCS: contains GInitializedAnonymousStructB"), ChunkPresent(MinifiedParsed, TEXT("GInitializedAnonymousStructB")));
		TestTrue(TEXT("MinifyShader: MainCS: contains OutputBufferType"), ChunkPresent(MinifiedParsed, TEXT("OutputBufferType")));
		TestTrue(TEXT("MinifyShader: MainCS: contains OutputBuffer"), ChunkPresent(MinifiedParsed, TEXT("OutputBuffer")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FTypedefUsedStruct"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUsedStruct")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FTypedefUsed"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUsed")));
		TestTrue(TEXT("MinifyShader: MainCS: contains FTypedefUsedChained"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUsedChained")));
		TestTrue(TEXT("MinifyShader: MainCS: contains TypedefUsedBuffer"), ChunkPresent(MinifiedParsed, TEXT("TypedefUsedBuffer")));
		TestTrue(TEXT("MinifyShader: MainCS: contains struct TUnusedTemplate"), MinifiedParsed.Source.Contains(TEXT("struct TUsedTemplate")));
		TestTrue(TEXT("MinifyShader: MainCS: contains TUsedTemplate<T> operator*"), MinifiedParsed.Source.Contains(TEXT("TUsedTemplate<T> operator*")));
		TestTrue(TEXT("MinifyShader: MainCS: contains EEnumUsed"), MinifiedParsed.Source.Contains(TEXT("EEnumUsed")));
		TestTrue(TEXT("MinifyShader: MainCS: contains ENUM_USED_PART_1"), MinifiedParsed.Source.Contains(TEXT("ENUM_USED_PART_1")));
		TestTrue(TEXT("MinifyShader: MainCS: contains ENUM_USED_PART_2"), MinifiedParsed.Source.Contains(TEXT("ENUM_USED_PART_2")));

		// Expect false:
		TestFalse(TEXT("MinifyShader: MainCS: contains UnreferencedFunction"), ChunkPresent(MinifiedParsed, TEXT("UnreferencedFunction")));
		TestFalse(TEXT("MinifyShader: MainCS: contains FUnreferencedStruct"), ChunkPresent(MinifiedParsed, TEXT("FUnreferencedStruct")));
		TestFalse(TEXT("MinifyShader: MainCS: contains GUnreferencedParameter"), ChunkPresent(MinifiedParsed, TEXT("GUnreferencedParameter")));
		TestFalse(TEXT("MinifyShader: MainCS: contains FTypedefUsedChainedUnused"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUsedChainedUnused")));
		TestFalse(TEXT("MinifyShader: MainCS: contains FTypedefUnusedStruct"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUnusedStruct")));
		TestFalse(TEXT("MinifyShader: MainCS: contains FTypedefUnused"), ChunkPresent(MinifiedParsed, TEXT("FTypedefUnused")));
		TestFalse(TEXT("MinifyShader: MainCS: contains TypedefUnusedBuffer"), ChunkPresent(MinifiedParsed, TEXT("TypedefUnusedBuffer")));
		TestFalse(TEXT("MinifyShader: MainCS: contains struct TUnusedTemplate"), MinifiedParsed.Source.Contains(TEXT("struct TUnusedTemplate")));
		TestFalse(TEXT("MinifyShader: MainCS: contains TUnusedTemplate<T> operator%"), MinifiedParsed.Source.Contains(TEXT("TUnusedTemplate<T> operator%")));
		TestFalse(TEXT("MinifyShader: MainCS: contains EEnumUnused"), MinifiedParsed.Source.Contains(TEXT("EEnumUnused")));
		TestFalse(TEXT("MinifyShader: MainCS: contains ENUM_UNUSED_PART_1"), MinifiedParsed.Source.Contains(TEXT("ENUM_UNUSED_PART_1")));
		TestFalse(TEXT("MinifyShader: MainCS: contains ENUM_UNUSED_PART_2"), MinifiedParsed.Source.Contains(TEXT("ENUM_UNUSED_PART_2")));
	}

	int32 NumErrors = ExecutionInfo.GetErrorTotal();

	return NumErrors == 0;
}

#endif // WITH_AUTOMATION_TESTS
