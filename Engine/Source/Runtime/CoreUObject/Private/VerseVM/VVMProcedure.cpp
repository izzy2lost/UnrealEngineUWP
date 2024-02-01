// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMProcedure.h"
#include "Templates/MemoryOps.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VProcedure);
TGlobalTrivialEmergentTypePtr<&VProcedure::StaticCppClassInfo> VProcedure::GlobalTrivialEmergentType;

VProcedure::~VProcedure()
{
	// NOTE: (yiliang.siew) This could be raised to a location such as in `VProgram` when that
	// exists and each opcode store only the index + size to index into that array, so that we don't
	// need to store a separate `TArray` of operand values per-opcode struct.
	ForEachOpCode([](auto& Op) {
		DestructItem(&Op);
	});
}

template <typename FuncType>
void VProcedure::ForEachOpCode(FuncType&& Func)
{
	for (FOp* CurrentOp = GetOpsBegin(); CurrentOp != GetOpsEnd();)
	{
		checkf(CurrentOp != nullptr, TEXT("The current opcode was invalid!"));
		switch (CurrentOp->Opcode)
		{
#define VISIT_OP(Name)                                                    \
	case EOpcode::Name:                                                   \
	{                                                                     \
		FOp##Name* CurrentDerivedOp = static_cast<FOp##Name*>(CurrentOp); \
		Func(*CurrentDerivedOp);                                          \
		CurrentOp = BitCast<FOp*>(CurrentDerivedOp + 1);                  \
		break;                                                            \
	}
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
			default:
				V_DIE("Invalid opcode encountered: %u", static_cast<FOpcodeInt>(CurrentOp->Opcode));
				break;
		}
	}
}

template <typename TVisitor>
void VProcedure::VisitOpCodes(TVisitor& Visitor)
{
	ForEachOpCode([&Visitor](auto& Op) {
		Op.ForEachOperand([&Visitor](EOperandRole Role, auto& Operand) {
			using DecayedType = std::decay_t<decltype(Operand)>;
			if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>)
			{
				return;
			}
			else if (Role == EOperandRole::Immediate)
			{
				Visitor.Visit(Operand, TEXT("")); // The "WithName" is not widechar and the existing code was wrong anyway.  Using blank for now.
			}
		});
	});
}

void VProcedure::SaveOpCodes(FAbstractVisitor& Visitor)
{
	int32 ValueCount = 0;
	TArray<uint8> SanitizedOpCodes(BitCast<uint8*>(GetOpsBegin()), NumOpBytes);

	// Scan the opcodes looking for any operands that will need to be written out seperately.
	// If one is found, we blank out that value in the sanitized op codes to make the output
	// more deterministic.
	ForEachOpCode([this, &SanitizedOpCodes, &ValueCount](auto& Op) {
		Op.ForEachOperand([this, &SanitizedOpCodes, &ValueCount](EOperandRole Role, auto& Operand) {
			using DecayedType = std::decay_t<decltype(Operand)>;
			if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>)
			{
				return;
			}
			else if (Role == EOperandRole::Immediate)
			{
				++ValueCount;
				uint32 ByteOffset = BytecodeOffset(&Operand);
				FMemory::Memset(&SanitizedOpCodes[ByteOffset], 0, sizeof(Operand));
			}
		});
	});

	Visitor.VisitBulkData(SanitizedOpCodes.GetData(), SanitizedOpCodes.Num(), TEXT("OpBytes"));

	// Scan again writing the values
	uint64 ScratchNumValues = ValueCount;
	Visitor.BeginArray(TEXT("OpCodeValues"), ScratchNumValues);
	if (ValueCount > 0)
	{
		ForEachOpCode([this, &Visitor](auto& Op) {
			Op.ForEachOperand([this, &Visitor](EOperandRole Role, auto& Operand) {
				using DecayedType = std::decay_t<decltype(Operand)>;
				if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>)
				{
					return;
				}
				else if (Role == EOperandRole::Immediate)
				{
					Visitor.Visit(Operand, TEXT(""));
				}
			});
		});
	}
	Visitor.EndArray();
}

void VProcedure::LoadOpCodes(FAbstractVisitor& Visitor)
{
	Visitor.VisitBulkData(GetOpsBegin(), NumOpBytes, TEXT("OpBytes"));

	uint64 ScratchNumValues = 0;
	Visitor.BeginArray(TEXT("OpCodeValues"), ScratchNumValues);
	if (ScratchNumValues > 0)
	{
		int32 ValueCount = 0;
		ForEachOpCode([this, &Visitor, &ValueCount](auto& Op) {
			Op.ForEachOperand([this, &Visitor, &ValueCount](EOperandRole Role, auto& Operand) {
				using DecayedType = std::decay_t<decltype(Operand)>;
				if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>)
				{
					return;
				}
				else if (Role == EOperandRole::Immediate)
				{
					++ValueCount;
					Visitor.Visit(Operand, TEXT(""));
				}
			});
		});
		checkSlow(ScratchNumValues == ValueCount);
	}
	Visitor.EndArray();
}

template <typename TVisitor>
void VProcedure::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumConstants = NumConstants;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants);
		Visitor.Visit(Constants, Constants + NumConstants);
		Visitor.EndArray();
		VisitOpCodes(Visitor);
	}
	else
	{
		Visitor.Visit(Constants, Constants + NumConstants);
		VisitOpCodes(Visitor);
	}
}

void VProcedure::SerializeImpl(VProcedure*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint32 ScratchNumParameters = 0;
		uint32 ScratchNumRegisters = 0;
		uint32 ScratchNumConstants = 0;
		uint64 ScratchNumOpBytes = 0;
		Visitor.Visit(ScratchNumParameters, TEXT("NumParameters"));
		Visitor.Visit(ScratchNumRegisters, TEXT("NumRegisters"));
		Visitor.Visit(ScratchNumConstants, TEXT("NumConstants"));
		Visitor.Visit(ScratchNumOpBytes, TEXT("NumOpBytes"));

		This = &VProcedure::New(Context, (uint32)ScratchNumParameters, (uint32)ScratchNumRegisters, (uint32)ScratchNumConstants, (size_t)ScratchNumOpBytes);

		uint64 ScratchNumConstants64 = 0;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants64);
		Visitor.Visit(This->Constants, This->Constants + This->NumConstants);
		Visitor.EndArray();

		This->LoadOpCodes(Visitor);
	}
	else
	{
		uint32 ScratchNumParameters = This->NumParameters;
		uint32 ScratchNumRegisters = This->NumRegisters;
		uint32 ScratchNumConstants = This->NumConstants;
		uint64 ScratchNumOpBytes = (uint64)This->NumOpBytes;
		Visitor.Visit(ScratchNumParameters, TEXT("NumParameters"));
		Visitor.Visit(ScratchNumRegisters, TEXT("NumRegisters"));
		Visitor.Visit(ScratchNumConstants, TEXT("NumConstants"));
		Visitor.Visit(ScratchNumOpBytes, TEXT("NumOpBytes"));

		uint64 ScratchNumConstants64 = This->NumConstants;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants64);
		Visitor.Visit(This->Constants, This->Constants + This->NumConstants);
		Visitor.EndArray();

		This->SaveOpCodes(Visitor);
	}
}

} // namespace Verse

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
