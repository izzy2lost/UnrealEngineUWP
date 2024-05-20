// Copyright Epic Games, Inc. All Rights Reserved.

#include  "PlainPropsSave.h"
#include  "PlainPropsBind.h"
#include  "PlainPropsInternalBuild.h"
#include  "PlainPropsInternalFormat.h"
#include <type_traits>

namespace PlainProps
{

static uint64 GetBit(uint8 Byte, uint8 BitIdx)
{
	return (Byte >> BitIdx) & 1;
}

static uint64 SaveLeaf(const uint8* Member, FUnpackedLeafBindType Leaf)
{
#if DO_CHECK
	if (Leaf.Type == ELeafBindType::Float)
	{
		switch (Leaf.Width)
		{
			case ELeafWidth::B32: return ValueCast(*reinterpret_cast<const float*>(Member));
			case ELeafWidth::B64: return ValueCast(*reinterpret_cast<const double*>(Member));
			default: check(false); return 0;
		}

	}
#endif

	if (Leaf.Type == ELeafBindType::BitfieldBool)
	{
		return GetBit(*Member, Leaf.BitfieldIdx);
	}
	else
	{
		// Undefined behavior. If problematic, use memcpy or switch(Leaf.Type) and ValueCast(reinterpret_cast<...>(Member))  
		switch (Leaf.Width)
		{
			case ELeafWidth::B8:	return *Member;
			case ELeafWidth::B16:	return *reinterpret_cast<const uint16*>(Member);
			case ELeafWidth::B32:	return *reinterpret_cast<const uint32*>(Member);
			case ELeafWidth::B64:	return *reinterpret_cast<const uint64*>(Member);
		}
	}
	check(false);
	return uint64(0);
}

struct FLeafRangeSaver
{
	FBuiltRange* Out;
	uint8* OutIt;

	FLeafRangeSaver(uint64 Num, SIZE_T LeafSize)
	: Out(FBuiltRange::Create(Num, LeafSize))
	, OutIt(Out->Data)
	{}

	void Append(FExistingItemSlice Slice, uint32 Stride, SIZE_T LeafSize, const FSaveContext&)
	{
		check(Stride == LeafSize);
		FMemory::Memcpy(OutIt, Slice.Data, Slice.Num * LeafSize);
		OutIt += Slice.Num * LeafSize;
	}

	FBuiltRange* Finish()
	{
		return Out;
	}
};

//////////////////////////////////////////////////////////////////////////

template<typename BuiltItemType, typename ItemSchemaType>
struct TStructuralRangeSaver
{
	TArray64<BuiltItemType> Items;

	TStructuralRangeSaver(uint64 Num, ItemSchemaType)
	{ 
		Items.Reserve(Num);
	}

	void Append(FExistingItemSlice Slice, uint32 Stride, ItemSchemaType Schema, const FSaveContext& OuterCtx)
	{
		for (uint64 Idx = 0; Idx < Slice.Num; ++Idx)
		{
			Items.Emplace(SaveRangeItem(Slice.At(Idx, Stride), Schema, OuterCtx));
		}
	}

	[[nodiscard]] FBuiltRange* Finish()
	{
		return Private::BuildStructuralRange(/* ownership xfer */ Items);
	}
};

using FNestedRangeSaver = TStructuralRangeSaver<FBuiltRange*, FRangeMemberBinding>;
using FStructRangeSaver = TStructuralRangeSaver<TUniquePtr<FBuiltStruct>, FStructSchemaId>;

//////////////////////////////////////////////////////////////////////////

template<class SaverType, typename InnerContextType>
[[nodiscard]] FBuiltRange* SaveRange(const void* Range, const IItemRangeBinding& Binding, const FSaveContext& OuterCtx, InnerContextType InnerCtx)
{
	FSaveRangeContext ReadCtx = { { Range } };
	Binding.ReadItems(ReadCtx);

	if (const uint64 NumTotal = ReadCtx.Items.NumTotal)
	{
		SaverType Saver(NumTotal, InnerCtx);
		while (true)
		{
			check(ReadCtx.Items.Slice.Num > 0);
			Saver.Append(ReadCtx.Items.Slice, ReadCtx.Items.Stride, InnerCtx, OuterCtx);
		
			ReadCtx.Request.NumRead += ReadCtx.Items.Slice.Num;
			if (ReadCtx.Request.NumRead >= NumTotal)
			{
				check(ReadCtx.Request.NumRead == NumTotal);	
				return Saver.Finish();
			}

			Binding.ReadItems(ReadCtx);	
		}
	}
	
	return nullptr;
}

[[nodiscard]] static FRangeMemberBinding GetInnerRange(FRangeMemberBinding Member)
{
	check(Member.NumRanges > 1);
	check(Member.InnerTypes[0].IsRange());
	return { Member.InnerTypes + 1, Member.RangeBindings + 1, Member.NumRanges - 1, Member.InnermostSchema };
}

 ELeafWidth GetArithmeticWidth(FLeafBindType Leaf)
 {
	checkf(Leaf.Bind.Type != ELeafBindType::BitfieldBool, TEXT("Arrays of bitfields is not a thing"));
	return Leaf.Arithmetic.Width;
 }

[[nodiscard]] static FBuiltRange* SaveLeafRange(const uint8* Range, const ILeafRangeBinding& Binding, FUnpackedLeafType Leaf)
{
	FLeafRangeAllocator Allocator(Leaf);
	Binding.SaveLeaves(Range, Allocator);
	return Allocator.GetAllocatedRange();
}

[[nodiscard]] static FBuiltRange* SaveRange(const uint8* Range, FRangeMemberBinding Member, const FSaveContext& Ctx)
{
	FRangeBinding Binding = Member.RangeBindings[0];
	FMemberBindType InnerType = Member.InnerTypes[0];

	if (Binding.IsLeafBinding())
	{
		return SaveLeafRange(Range, Binding.AsLeafBinding(), UnpackNonBitfield(InnerType.AsLeaf()));
	}

	const IItemRangeBinding& ItemBinding = Binding.AsItemBinding();
	switch (InnerType.GetKind())
	{
	case EMemberKind::Leaf:		return SaveRange<FLeafRangeSaver>(  Range, ItemBinding, Ctx, SizeOf(GetArithmeticWidth(InnerType.AsLeaf())));
	case EMemberKind::Range:	return SaveRange<FNestedRangeSaver>(Range, ItemBinding, Ctx, GetInnerRange(Member));
	case EMemberKind::Struct:	return SaveRange<FStructRangeSaver>(Range, ItemBinding, Ctx, static_cast<FStructSchemaId>(Member.InnermostSchema.Get()));
	}

	check(false);
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////

[[nodiscard]] static FBuiltRange* SaveRangeItem(const uint8* Range, FRangeMemberBinding Member, const FSaveContext& Ctx)
{ 
	return SaveRange(Range, Member, Ctx);
}

[[nodiscard]] static TUniquePtr<FBuiltStruct> SaveRangeItem(const uint8* Struct, FStructSchemaId Id, const FSaveContext& Ctx)
{
	return SaveStruct(Struct, Id, Ctx);
}

[[nodiscard]] static FMemberType ToMemberType(FMemberBindType In)
{
	switch (In.GetKind())
	{
	case EMemberKind::Leaf:		return FMemberType(ToLeafType(In.AsLeaf()));
	case EMemberKind::Range:	return FMemberType(In.AsRange());
	default:					return FMemberType(In.AsStruct());
	}
}

[[nodiscard]] static TArray<FMemberType> ToMemberTypes(const FMemberBindType* InnerTypes)
{
	TArray<FMemberType> Out;
	for (const FMemberBindType* It = InnerTypes; true; ++It)
	{
		Out.Add(ToMemberType(*It));
		if (!It->IsRange())
		{
			return Out;
		}
	} 
}

[[nodiscard]] static FMemberSchema MakeSchema(FRangeMemberBinding Member)
{
	return { FMemberType(Member.RangeBindings[0].GetSizeType()), Member.InnermostSchema, ToMemberTypes(Member.InnerTypes) };
}

static void SaveMember(FMemberBuilder& Out, const uint8* Struct, FMemberId Name, const FSaveContext& Ctx, FLeafMemberBinding Member)
{
	FUnpackedLeafType Type = { ToLeafType(Member.Leaf.Type), Member.Leaf.Width };
	Out.AddLeaf(Name, Type, Member.Enum, SaveLeaf(Struct + Member.Offset, Member.Leaf));
}

static void SaveMember(FMemberBuilder& Out, const uint8* Struct, FMemberId Name, const FSaveContext& Ctx, FRangeMemberBinding Member)
{
	Out.AddRange(Name, { MakeSchema(Member), SaveRange(Struct + Member.Offset, Member, Ctx) });
}

static void SaveMember(FMemberBuilder& Out, const uint8* Struct, FMemberId Name, const FSaveContext& Ctx, FStructMemberBinding Member)
{
	Out.AddStruct(Name, Member.Id, SaveStruct(Struct + Member.Offset, Member.Id, Ctx));
}

TUniquePtr<FBuiltStruct> SaveStruct(const uint8* Struct, FStructSchemaId Id, const FSaveContext& Ctx)
{
	const FStructDeclaration& Declaration = Ctx.Declarations.Get(Id);

	FMemberBuilder Out;
	if (ICustomBinding* Custom = Ctx.Customs.FindStruct(Id))
	{
		Custom->SaveStruct(Out, Struct, nullptr, Ctx.Declarations.GetDebug());
	}
	else for (FMemberVisitor It(Ctx.Schemas.GetStruct(Id)); It.HasMore(); )
	{
		FMemberId Name = Declaration.GetMemberOrder()[It.GetIndex()];
		switch (It.PeekKind())
		{
			case EMemberKind::Leaf:		SaveMember(Out, Struct, Name, Ctx, It.GrabLeaf());		break;
			case EMemberKind::Range:	SaveMember(Out, Struct, Name, Ctx, It.GrabRange());		break;
			case EMemberKind::Struct:	SaveMember(Out, Struct, Name, Ctx, It.GrabStruct());	break;
		}
	}
	
	return Out.BuildAndReset(Declaration, Ctx.Declarations.GetDebug());
}

} // namespace PlainProps