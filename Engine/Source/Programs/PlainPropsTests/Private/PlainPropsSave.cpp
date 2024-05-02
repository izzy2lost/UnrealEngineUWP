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

//FBuiltRange* SaveLeafRange(const void* Range, const IRangeBinding& Binding, FUnpackedLeafType Leaf)
//{
//	FSaveRangeContext Ctx = { { Range } };
//
//	Binding.ReadItems(Ctx);
//
//	if (const uint64 NumTotal = Ctx.Items.NumTotal)
//	{
//		FBuiltRange* Out = new (FMemory::Malloc(sizeof(FBuiltRange) + NumTotal * SizeOf(Leaf.Width))) FBuiltRange;
//		Out->Num = NumTotal;
//
//		for (uint64 NumRead = 0; true; NumRead += Ctx.Items.Slice.Num)
//		{
//			check(Ctx.Items.Slice.Num > 0);
//
//			FMemory::Memcpy(Out->Data + NumRead, Ctx.Items.Slice.Data,  Ctx.Items.Slice.Num * SizeOf(Leaf.Width));
//
//			if (NumRead >= NumTotal)
//			{
//				check(NumRead == NumTotal);	
//				break;
//			}
//
//			Binding.ReadItems(Ctx);	
//		}
//
//		return Out;
//	}
//	
//	return nullptr;
//}
//
//FBuiltRange* SaveStructRange(const void* Range, const IRangeBinding& Binding, FStructType Type, FStructSchemaId Id, const FSaveContext& OuterCtx)
//{
//	FSaveRangeContext Ctx = { { Range } };
//
//	Binding.ReadItems(Ctx);
//
//	if (const uint64 NumTotal = Ctx.Items.NumTotal)
//	{
//		TArray64<TUniquePtr<FBuiltStruct>> Structs;
//		Structs.Reserve(NumTotal);
//
//		for (uint64 NumRead = 0; true; NumRead += Ctx.Items.Slice.Num)
//		{
//			check(Ctx.Items.Slice.Num > 0);
//
//			for (uint64 Idx = 0, Num = Ctx.Items.Slice.Num; Idx < Num; ++Idx)
//			{
//				Structs.Emplace(SaveStruct(Ctx.Items.Slice.At(Idx, Ctx.Items.Stride), Id, OuterCtx));
//			}
//
//			if (NumRead >= NumTotal)
//			{
//				check(NumRead == NumTotal);	
//				break;
//			}
//
//			Binding.ReadItems(Ctx);	
//		}
//
//		return Private::BuildStructRange(/* ownership xfer */ Structs);
//	}
//	
//	return nullptr;
//}



//
//static FBuiltRange* SaveNestedRange(const void* Range, const IRangeBinding& Binding, FRangeType Type, FRangeMemberBinding InnerBinding, const FSaveContext& OuterCtx)
//{
//	FSaveRangeContext Ctx = { { Range } };
//
//	Binding.ReadItems(Ctx);
//
//	if (const uint64 NumTotal = Ctx.Items.NumTotal)
//	{
//		TArray64<FBuiltRange*> Ranges;
//		Ranges.Reserve(NumTotal);
//
//		for (uint64 NumRead = 0; true; NumRead += Ctx.Items.Slice.Num)
//		{
//			check(Ctx.Items.Slice.Num > 0);
//
//			for (uint64 Idx = 0, Num = Ctx.Items.Slice.Num; Idx < Num; ++Idx)
//			{
//				Ranges.Emplace(SaveRange(Ctx.Items.Slice.At(Idx, Ctx.Items.Stride), InnerBinding, OuterCtx));
//			}
//
//			if (NumRead >= NumTotal)
//			{
//				check(NumRead == NumTotal);	
//				break;
//			}
//
//			Binding.ReadItems(Ctx);	
//		}
//
//		return Private::BuildNestedRange(/* ownership xfer */ Ranges);
//	}
//	
//	return nullptr;
//}


//struct FStructRangeSaver
//{
//	FStructSchemaId Id;
//	TArray64<TUniquePtr<FBuiltStruct>> Structs;
//
//	void Init(uint64 Num)
//	{ 
//		Ranges.Reserve(NumTotal);
//	}
//
//	void Append(FExistingItemSlice Slice, uint32 Stride, FSaveContext& OuterCtx)
//	{
//		for (uint64 Idx = 0; Idx < Slice.Num; ++Idx)
//		{
//			Ranges.Emplace(SaveRange(Slice.At(Idx, Stride), Id, OuterCtx));
//		}
//	}
//
//	FBuiltRange* Build()
//	{
//		Private::BuildStructRange(/* ownership xfer */ Structs);
//	}
//};

struct FLeafRangeSaver
{
	FBuiltRange* Out;
	uint8* OutIt;

	FLeafRangeSaver(uint64 Num, SIZE_T LeafSize)
	: Out(new (FMemory::Malloc(sizeof(FBuiltRange) + Num * LeafSize)) FBuiltRange)
	, OutIt(Out->Data)
	{ 
		Out->Num = Num;
	}

	void Append(FExistingItemSlice Slice, uint32, SIZE_T LeafSize, const FSaveContext&)
	{
		FMemory::Memcpy(OutIt, Slice.Data, Slice.Num * LeafSize);
		OutIt += Slice.Num * LeafSize;
	}

	FBuiltRange* Finish()
	{
		return Out;
	}
};

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

template<class SaverType, typename InnerContextType>
[[nodiscard]] FBuiltRange* SaveRange(const void* Range, const IRangeBinding& Binding, const FSaveContext& OuterCtx, InnerContextType InnerCtx)
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
		
			ReadCtx.Request.Index += ReadCtx.Items.Slice.Num;
			if (ReadCtx.Request.Index >= NumTotal)
			{
				check(ReadCtx.Request.Index == NumTotal);	
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

[[nodiscard]] static FBuiltRange* SaveRange(const uint8* Range, FRangeMemberBinding Member, const FSaveContext& Ctx)
{
	const IRangeBinding& Binding = Member.RangeBindings[0].GetBinding();
	FMemberBindType InnerType = Member.InnerTypes[0];
	switch (InnerType.GetKind())
	{
	case EMemberKind::Leaf:		return SaveRange<FLeafRangeSaver>(  Range, Binding, Ctx, SizeOf(GetArithmeticWidth(InnerType.AsLeaf())));
	case EMemberKind::Range:	return SaveRange<FNestedRangeSaver>(Range, Binding, Ctx, GetInnerRange(Member));
	case EMemberKind::Struct:	return SaveRange<FStructRangeSaver>(Range, Binding, Ctx, static_cast<FStructSchemaId>(Member.InnermostSchema.Get()));
	}

	check(false);
	return nullptr;
}

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
	FStructBinding Binding = Ctx.Bindings.Get(Id);
	const FStructDeclaration& Declaration = Ctx.Declarations.Get(Id);
	FMemberBuilder Out;

	if (Binding.IsCustom())
	{
		Binding.AsCustom().SaveStruct(Out, Struct, nullptr, Ctx.Debug);
	}
	else for (FMemberVisitor It(Binding.AsSchema()); It.HasMore(); )
	{
		FMemberId Name = Declaration.GetMemberOrder()[It.GetIndex()];
		switch (It.PeekKind())
		{
			case EMemberKind::Leaf:		SaveMember(Out, Struct, Name, Ctx, It.GrabLeaf());		break;
			case EMemberKind::Range:	SaveMember(Out, Struct, Name, Ctx, It.GrabRange());		break;
			case EMemberKind::Struct:	SaveMember(Out, Struct, Name, Ctx, It.GrabStruct());	break;
		}
	}
	
	return Out.BuildAndReset(Declaration, Ctx.Debug);
}

} // namespace PlainProps