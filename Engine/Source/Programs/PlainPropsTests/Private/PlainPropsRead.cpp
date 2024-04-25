// Copyright Epic Games, Inc. All Rights Reserved.

#include  "PlainPropsRead.h"
#include  "PlainPropsInternalFormat.h"
#include  "PlainPropsInternalRead.h"
#include "Misc/Optional.h"

#include <atomic>
#include <type_traits>

namespace PlainProps
{

void FSchemaBatch::ValidateBounds(uint64 NumBytes) const
{
	static constexpr uint32 Alignment = FMath::Max(alignof(FStructSchema), alignof(FEnumSchema));
	check(IsAligned(this, Alignment));
	check(sizeof(FSchemaBatch) + NumSchemas * sizeof(uint32) <= NestedScopesOffset);
	check(NestedScopesOffset + NumNestedScopes * sizeof(FNestedScope) + NumParametricTypes * sizeof(FParametricType) <= NumBytes);

	for (uint32 SchemaOffset : MakeArrayView(SchemaOffsets, NumSchemas))
	{
		check(SchemaOffset < NestedScopesOffset);
		check(IsAligned(SchemaOffset, Alignment));
	}
		
	uint32 NumParameters = 0;
	for (FParametricType ParametricType : GetParametricTypes())
	{
		check(ParametricType.Parameters.Idx == NumParameters);
		check(ParametricType.Parameters.NumParameters > 0);
		NumParameters += ParametricType.Parameters.NumParameters;
	}

	const void* ExpectedEnd = GetFirstParameter() + NumParameters;
	const void* ActualEnd = reinterpret_cast<const uint8*>(this) + NumBytes;
	check(ExpectedEnd == ActualEnd);
}

template<class SchemaType>
const SchemaType& ResolveSchema(const FSchemaBatch& Batch, FSchemaId Id)
{
	check(Id.Idx < Batch.NumSchemas);
	return *reinterpret_cast<const SchemaType*>(reinterpret_cast<const uint8*>(&Batch) + Batch.SchemaOffsets[Id.Idx]);
}
	
static FNestedScope ResolveNestedScope(const FSchemaBatch& Batch, FNestedScopeId Id)
{
	return Batch.GetNestedScopes()[Id.Idx];
}
	
static FParametricTypeView ResolveParametricType(const FSchemaBatch& Batch, FParametricTypeId Id)
{
	FParametricType Type = Batch.GetParametricTypes()[Id.Idx];
	return {Type.Name, IntCastChecked<uint8>(Type.Parameters.NumParameters), Batch.GetFirstParameter() + Type.Parameters.Idx};
}


class FReadSchemaRegistry
{
	static constexpr uint32 Capacity = 1 << 16;
	std::atomic<const FSchemaBatch*> Slots[Capacity]{};
	std::atomic<uint32> Counter{};

public:
	FReadBatchId Mount(const FSchemaBatch* Batch)
	{
		const uint32 Count = Counter.fetch_add(1, std::memory_order_relaxed);
		for (uint32 BaseIdx = 0; BaseIdx < FReadSchemaRegistry::Capacity; ++BaseIdx)
		{
			uint32 Idx = (Count + BaseIdx) % FReadSchemaRegistry::Capacity;
			const FSchemaBatch* Null = nullptr;
			if (Slots[Idx].load(std::memory_order_relaxed) == nullptr &&
				Slots[Idx].compare_exchange_strong(Null, Batch, std::memory_order_acq_rel))
			{
				FReadBatchId Out;
				Out.Idx = static_cast<uint16>(Idx);
				return Out;
			}
		}

		checkf(false, TEXT("Exceeded fixed limit of 65536 simulatenous read batches"));
		while (true);
	}

	const FSchemaBatch* Unmount(FReadBatchId Id)
	{
		const FSchemaBatch* Batch = &Get(Id);
		verify(Slots[Id.Idx].compare_exchange_strong(Batch, nullptr, std::memory_order_acq_rel));
		return Batch;
	}

	const FSchemaBatch& Get(FReadBatchId Id) const
	{
		const FSchemaBatch* Batch = Slots[Id.Idx].load(std::memory_order_relaxed);
		check(Batch);
		return *Batch;
	}
};
static FReadSchemaRegistry GReadSchemas;

const FSchemaBatch*	ValidateSchemas(FMemoryView Schemas)
{
	const FSchemaBatch* Batch = reinterpret_cast<const FSchemaBatch*>(Schemas.GetData());
	Batch->ValidateBounds(Schemas.GetSize());
	return Batch;
}

FReadBatchId MountReadSchemas(const FSchemaBatch* Batch)
{
	return GReadSchemas.Mount(Batch);
}

const FSchemaBatch* UnmountReadSchemas(FReadBatchId Id)
{
	return GReadSchemas.Unmount(Id);
}

uint32 NumStructSchemas(FReadBatchId Batch)
{
	return GReadSchemas.Get(Batch).NumSchemas;
}

const FStructSchema& ResolveStructSchema(FReadBatchId Batch, FStructSchemaId Schema)
{
	return ResolveSchema<FStructSchema>(GReadSchemas.Get(Batch), Schema);
}

const FEnumSchema& ResolveEnumSchema(FReadBatchId Batch, FEnumSchemaId Schema)
{
	return ResolveSchema<FEnumSchema>(GReadSchemas.Get(Batch), Schema);
}

FNestedScope ResolveUntranslatedNestedScope(FReadBatchId Batch, FNestedScopeId Id)
{
	return ResolveNestedScope(GReadSchemas.Get(Batch), Id);
}

FParametricTypeView ResolveUntranslatedParametricType(FReadBatchId Batch, FParametricTypeId Id)
{
	return ResolveParametricType(GReadSchemas.Get(Batch), Id);
}

//////////////////////////////////////////////////////////////////////////

FLeafRangeView FRangeView::AsLeaves() const
{
	FUnpackedLeafType Leaf = Schema.ItemType.AsLeaf();

	FLeafRangeView Out;
	Out.Type = Leaf.Type;
	Out.Width = Leaf.Width;
	Out.Batch = Schema.Batch;
	Out.Enum = static_cast<FOptionalEnumSchemaId>(Schema.InnermostSchema);
	Out.NumItems = NumItems;
	Out.Values = static_cast<const uint8*>(Values.GetData());

	return Out;
}

FStructRangeView FRangeView::AsStructs() const
{
	check(IsStructRange());

	FStructRangeView Out;
	Out.NumItems = NumItems;
	Out.Data = Values;
	Out.Schema.Id = static_cast<FStructSchemaId>(Schema.InnermostSchema.Get());
	Out.Schema.Batch = Schema.Batch;

	return Out;
}

FNestedRangeView FRangeView::AsRanges() const
{
	check(IsNestedRange());

	FNestedRangeView Out;
	Out.NumItems = NumItems;
	Out.Data = Values;
	Out.Schema = Schema;
	return Out;
}

//////////////////////////////////////////////////////////////////////////

bool FBitCacheReader::GrabNext(FByteReader& Bytes)
{
	BitIt <<= 1; // Shift up til overflow

	if (BitIt == 0)
	{
		Bits = Bytes.GrabByte();
		BitIt = 1;
	}

	return !!(Bits & BitIt);
}

const FStructSchema& FStructSchemaHandle::ResolveSuper() const
{
	return ResolveStructSchema(Batch, Resolve().GetSuperSchema().Get());
}

//////////////////////////////////////////////////////////////////////////

static FMemoryView GrabRangeValues(uint64 Num, FMemberType InnerType, /* in-out */ FByteReader& ByteIt)
{
	if (Num == 0)
	{
		return FMemoryView();
	}

	uint64 NumBytes = InnerType.GetKind() == EMemberKind::Leaf ? GetLeafRangeSize(Num, InnerType.AsLeaf()) : ByteIt.GrabVarIntU();

	return ByteIt.GrabSlice(NumBytes);
}

FRangeView FNestedRangeIterator::operator*() const 
{
	FByteReader PeekBytes = ByteIt;
	FBitCacheReader PeekBits = BitIt;
	
	FRangeView Out;
	Out.Schema = { Schema.NestedItemTypes[0], Schema.Batch, Schema.InnermostSchema, /* Only valid for nested ranges */ Schema.NestedItemTypes + 1 };
	Out.NumItems = GrabRangeNum(Schema.ItemType.AsRange().MaxSize, /* in-out */ PeekBytes,  /* in-out */ PeekBits);
	Out.Values = GrabRangeValues(Out.NumItems, Schema.NestedItemTypes[0], /* in-out */ PeekBytes);

	return Out;
}

void FNestedRangeIterator::operator++()
{
	uint64 Num = GrabRangeNum(Schema.ItemType.AsRange().MaxSize, /* in-out */ ByteIt, /* in-out */ BitIt);
	(void)GrabRangeValues(Num, Schema.NestedItemTypes[0], /* in-out */ ByteIt);
}

//////////////////////////////////////////////////////////////////////////

const FMemberType*	FMemberReader::GetMemberTypes() const	{ return FStructSchema::GetMemberTypes(Footer); }
const FMemberType*	FMemberReader::GetRangeTypes() const	{ return FStructSchema::GetRangeTypes(Footer, NumMembers); }
const FSchemaId*	FMemberReader::GetInnerSchemas() const	{ return FStructSchema::GetInnerSchemas(Footer, NumMembers, NumRangeTypes, NumMembers - HasSuper); }
const FMemberId*	FMemberReader::GetMemberNames() const	{ return FStructSchema::GetMemberNames(Footer, NumMembers, NumRangeTypes); }

static bool SkipDeclaredSuperSchema(ESuper Inheritance)
{
	return Inheritance == ESuper::Unused || Inheritance == ESuper::Used;
}

FMemberReader::FMemberReader(const FStructSchema& Schema, FByteReader Values, FReadBatchId InBatch)
: Footer(Schema.Footer)
, Batch(InBatch)
, NumMembers(Schema.NumMembers)
, NumRangeTypes(Schema.NumRangeTypes)
, IsSparse(!Schema.IsDense)
, HasSuper(UsesSuper(Schema.Inheritance))
, InnerSchemaIdx(SkipDeclaredSuperSchema(Schema.Inheritance))
, ValueIt(Values)
#if DO_CHECK
, NumInnerSchemas(Schema.NumInnerSchemas)
#endif
{
	check(InnerSchemaIdx <= NumInnerSchemas);
	checkf(NumRangeTypes != 0xFFFFu, TEXT("GrabRangeTypes() doesn't check for wrap-around"));
	SkipMissingSparseMembers();
}

FOptionalMemberId FMemberReader::PeekName() const
{
	int32 MemberNameIdx = MemberIdx - HasSuper;
	if (MemberNameIdx == -1)
	{
		UE_DEBUG_BREAK();
	}

	return MemberNameIdx >= 0 ? ToOptional(GetMemberNames()[MemberNameIdx]) : NoId;
}

EMemberKind FMemberReader::PeekKind() const
{
	return PeekType().GetKind();
}

FMemberType	FMemberReader::PeekType() const
{
	check(HasMore());
	return GetMemberTypes()[MemberIdx];
}

void FMemberReader::AdvanceToNextMember()
{
	++MemberIdx;
	SkipMissingSparseMembers();
}

void FMemberReader::SkipMissingSparseMembers()
{
	if (IsSparse)
	{
		// Change code in LoadMembers() too
		while (MemberIdx < NumMembers && GrabBit())
		{
			FMemberType InnermostType = PeekType().IsRange() ? /* advances RangeTypeIdx */ GrabRangeTypes().Last() : PeekType();
			SkipSchema(InnermostType);
			++MemberIdx;
		}
	}	
}

void FMemberReader::SkipSchema(FMemberType InnermostType)
{
	if (InnermostType.IsStruct())
	{
		if (InnermostType.AsStruct().IsDynamic)
		{
			(void)ValueIt.Grab<uint32>();
		}
		else
		{
			++InnerSchemaIdx;
		}
	}
	else
	{
		InnerSchemaIdx += InnermostType.AsLeaf().Type == ELeafType::Enum;
	}

	check(InnerSchemaIdx <= NumInnerSchemas);
}

FSchemaId FMemberReader::GrabInnerSchema()
{
	check(InnerSchemaIdx < NumInnerSchemas);
	uint32 Idx = InnerSchemaIdx++;
	return GetInnerSchemas()[Idx];
}

FStructSchemaId FMemberReader::GrabStructSchema(FStructType Type)
{
	return Type.IsDynamic	? FStructSchemaId { ValueIt.Grab<uint32>() }
							: static_cast<FStructSchemaId&&>(GrabInnerSchema());
}

FOptionalSchemaId FMemberReader::GrabRangeSchema(FMemberType InnermostType)
{
	if (InnermostType.IsStruct())
	{
		return FOptionalSchemaId(GrabStructSchema(InnermostType.AsStruct()));
	}

	ELeafType LeafType = InnermostType.AsLeaf().Type;
	return LeafType == ELeafType::Enum ? FOptionalSchemaId(GrabEnumSchema()) : NoId;
}

FLeafView FMemberReader::GrabLeaf()
{
	check(HasMore());
	FUnpackedLeafType Leaf = PeekType().AsLeaf();
	FEnumSchemaId Enum = Leaf.Type == ELeafType::Enum ? GrabEnumSchema() : FEnumSchemaId{};
	
	FMemberValue Value;
	if (Leaf.Type == ELeafType::Bool)
	{
		Value.bValue = GrabBit();
	}
	else
	{
		Value.Ptr = ValueIt.GrabBytes(SizeOf(Leaf.Width));
	}

	AdvanceToNextMember();

	return { Leaf.Type, Leaf.Width, Batch, Enum, Value };
}

FStructView FMemberReader::GrabStruct()
{
	check(HasMore());
	FStructType Type = PeekType().AsStruct();
	FStructSchemaId Struct = Type.IsDynamic ? FStructSchemaId{ ValueIt.Grab<uint32>() } : GrabStructSchema(Type);
	FMemoryView Values = ValueIt.GrabSkippableSlice();

	AdvanceToNextMember();

	return { { Struct, Batch }, FByteReader(Values) };
}

TConstArrayView<FMemberType> FMemberReader::GrabRangeTypes()
{
	return GrabInnerRangeTypes(MakeArrayView(GetRangeTypes(), NumRangeTypes), /* in-out */ RangeTypeIdx);
}

FRangeView FMemberReader::GrabRange()
{
	check(HasMore());

	FMemberTypeRange RangeTypes = GrabRangeTypes();
	FRangeView Out;
	Out.Schema.Batch = Batch;
	Out.Schema.InnermostSchema = GrabRangeSchema(RangeTypes.Last());									// #1
	Out.Schema.ItemType = RangeTypes[0];
	Out.Schema.NestedItemTypes = RangeTypes.Num() > 1 ? &RangeTypes[1] : nullptr;
	Out.NumItems = GrabRangeNum(PeekType().AsRange().MaxSize, /* in-out */ ValueIt, /* in-out */ Bits); // #2
	Out.Values = GrabRangeValues(Out.NumItems, Out.Schema.ItemType, /* in-out */ ValueIt);				// #3
	
	AdvanceToNextMember();

	return Out;
}

//FAnyMemberView FMemberReader::GrabAnyMember()
//{
//	FMemberValue Value = GetValueAndAdvance(Type, /* in-out */ ValueIt);
//	
//	return {Type, Id, BatchId, Value};
//}

//////////////////////////////////////////////////////////////////////////

static TOptional<FStructView> TryGrabSuper(FMemberReader& Members)
{
	if (Members.HasMore() && IsSuper(Members.PeekType()))
	{
		return Members.GrabStruct();
	}

	return NullOpt;
}

FFlatMemberReader::FReader::FReader(FStructView Struct)
: FMemberReader(Struct)
, Owner(Struct.Schema.Resolve().Type)
{}


FFlatMemberReader::FFlatMemberReader(FStructView Struct)
{
	Lineage.Emplace(Struct);
	while (TOptional<FStructView> Super = TryGrabSuper(Lineage.Last()))
	{
		Lineage.Emplace(*Super);
	}
	It = &Lineage.Last();
}

} // namespace PlainProps