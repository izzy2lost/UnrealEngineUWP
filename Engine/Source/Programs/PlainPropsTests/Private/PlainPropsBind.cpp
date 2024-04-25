// Copyright Epic Games, Inc. All Rights Reserved.

#include  "PlainPropsBind.h"
//#include  "PlainPropsInternalFormat.h"
//#include  "PlainPropsInternalRead.h"
//#include <type_traits>

namespace PlainProps
{

static_assert(sizeof(ELeafBindType) == 1);
static_assert((uint8)ELeafType::Bool		== (uint8)ELeafBindType::Bool);
static_assert((uint8)ELeafType::IntS		== (uint8)ELeafBindType::IntS);
static_assert((uint8)ELeafType::IntU		== (uint8)ELeafBindType::IntU);
static_assert((uint8)ELeafType::Float		== (uint8)ELeafBindType::Float);
static_assert((uint8)ELeafType::Hex			== (uint8)ELeafBindType::Hex);
static_assert((uint8)ELeafType::Enum		== (uint8)ELeafBindType::Enum);
static_assert((uint8)ELeafType::Unicode		== (uint8)ELeafBindType::Unicode);

FMemberVisitor::FMemberVisitor(const FStructSchemaBinding& InSchema)
: Schema(InSchema)
, NumMembers(InSchema.NumMembers)
{}

EMemberKind FMemberVisitor::PeekKind() const
{
	return PeekType().GetKind();
}

FMemberBindType	FMemberVisitor::PeekType() const
{
	check(HasMore());
	return Schema.Members[MemberIdx];
}

uint64 FMemberVisitor::GrabMemberOffset()
{
	return Schema.GetOffsets()[MemberIdx++];
}

FLeafMemberBinding FMemberVisitor::GrabLeaf()
{
	FUnpackedLeafBindType Leaf = PeekType().AsLeaf();
	FOptionalEnumSchemaId Enum = Leaf.Type == ELeafBindType::Enum ? ToOptional(GrabEnumSchema()) : NoId;
	uint64 Offset = GrabMemberOffset();

	return {Leaf, Enum, Offset};
}

FStructMemberBinding FMemberVisitor::GrabStruct()
{
	checkf(!PeekType().AsStruct().IsDynamic, TEXT("Bound structs can't be dynamic"));
	return { static_cast<FStructSchemaId>(GrabInnerSchema()), GrabMemberOffset() };
}

static bool HasSchema(FMemberBindType Type)
{
	checkf(!Type.IsStruct() || !Type.AsStruct().IsDynamic, TEXT("Bound structs can't be dynamic"));
	return Type.IsStruct() || Type.AsLeaf().Bind.Type == ELeafBindType::Enum;
}

TConstArrayView<FMemberBindType> FMemberVisitor::GrabInnerTypes()
{
	const int32 Idx = InnerRangeIdx;
	const TConstArrayView<FMemberBindType> All(Schema.GetInnerRangeTypes(), Schema.NumInnerRanges);
	while (All[InnerRangeIdx++].IsRange()) {}
	return All.Slice(Idx, InnerRangeIdx - Idx);
}

FRangeMemberBinding FMemberVisitor::GrabRange()
{
	ERangeSizeType MaxSize = PeekType().AsRange().MaxSize;
	const FRangeBinding* RangeBindings = Schema.GetRangeBindings() + InnerRangeIdx;
	check(MaxSize == RangeBindings[0].GetSizeType());
	FMemberBindTypeRange InnerTypes = GrabInnerTypes();
	FOptionalSchemaId InnermostSchema = HasSchema(InnerTypes.Last()) ? ToOptional(GrabInnerSchema()) : NoId; 
	uint64 Offset = GrabMemberOffset();
		
	return { &InnerTypes[0], RangeBindings, InnermostSchema, Offset};
}

FSchemaId FMemberVisitor::GrabInnerSchema()
{
	check(InnerSchemaIdx < Schema.NumInnerSchemas);
	return Schema.GetInnerSchemas()[InnerSchemaIdx++];
}

FRangeBinding::FRangeBinding(const IRangeBinding& Binding, ERangeSizeType SizeType)
: Handle(uint64(&Binding) | uint8(SizeType))
{
	check(&Binding == &GetBinding());
	check(SizeType == GetSizeType());
}


////////////////////////////////////////////////////////////////////////////////////////////////

FStructBindings::~FStructBindings()
{
	for (FStructBinding Binding : Bindings)
	{
		if (FStructSchemaBinding* Schema = Binding.TryGetSchema())
		{
			FMemory::Free(Schema);
		}
	}
}

void FStructBindings::BindStruct(FStructSchemaId Id, const ICustomStructBinding& Custom)
{
	Bind(Id, FStructBinding(Custom));
}

static uint16 CountInnerSchemas(TConstArrayView<FMemberBinding> Members)
{
	uint32 Out = 0;
	for (const FMemberBinding& Member : Members)
	{
		Out += !!Member.InnermostSchema;
	}
	return IntCastChecked<uint16>(Out);
}

static uint16 CountRanges(TConstArrayView<FMemberBinding> Members)
{
	int32 Out = 0;
	for (const FMemberBinding& Member : Members)
	{
		Out += Member.RangeBindings.Num();
	}
	return IntCastChecked<uint16>(Out);
}

void FStructBindings::BindStruct(FStructSchemaId Id, TConstArrayView<FMemberBinding> Members)
{
	FStructSchemaBinding Header = { IntCastChecked<uint16>(Members.Num()), CountInnerSchemas(Members), CountRanges(Members) };

	// Calculate size
	uint64 Size = sizeof(Header);
	Size += (Members.Num() + Header.NumInnerRanges) * sizeof(FMemberBindType);
	Size = Align(Size, sizeof(uint32));
	Size += Members.Num() * sizeof(uint32);
	Size = Align(Size, sizeof(FSchemaId));
	Size += Header.NumInnerSchemas * sizeof(FSchemaId);
	Size = Align(Size, sizeof(FRangeBinding));
	Size += Header.NumInnerRanges * sizeof(FRangeBinding);

	// Allocate and copy header
	FStructSchemaBinding* Schema = new (FMemory::MallocZeroed(Size)) FStructSchemaBinding {Header};

	// Copy footer
	FMemberBindType* MemberIt		= Schema->Members;
	FMemberBindType* RangeTypeIt	= const_cast<FMemberBindType*>(Schema->GetInnerRangeTypes());
	uint32* OffsetIt				= const_cast<uint32*>(Schema->GetOffsets());
	FSchemaId* InnerSchemaIt		= const_cast<FSchemaId*>(Schema->GetInnerSchemas());
	FRangeBinding* RangeBindingIt	= const_cast<FRangeBinding*>(Schema->GetRangeBindings());
	for (const FMemberBinding& Member : Members)
	{
		if (TConstArrayView<FRangeBinding> Ranges = Member.RangeBindings; Ranges.IsEmpty())
		{
			*MemberIt++ = Member.InnermostType;
		}
		else
		{
			*MemberIt++ = FMemberBindType(Ranges[0].GetSizeType());

			for (FRangeBinding Range : Ranges.RightChop(1))
			{
				*RangeTypeIt++ = FMemberBindType(Range.GetSizeType());
			}
			*RangeTypeIt++ = Member.InnermostType;

			FMemory::Memcpy(RangeBindingIt, Ranges.GetData(), Ranges.Num() * Ranges.GetTypeSize());
			RangeBindingIt += Ranges.Num();
		}

		if (Member.InnermostSchema)
		{
			*InnerSchemaIt++ = Member.InnermostSchema.Get();
		}

		*OffsetIt++ = IntCastChecked<uint32>(Member.Offset);
	}

	// Validate copying
	check(MemberIt == Schema->GetInnerRangeTypes());
	check(RangeTypeIt == (const void*)Schema->GetOffsets());
	check(OffsetIt == (const void*)Schema->GetInnerSchemas());
	check(InnerSchemaIt == (const void*)Schema->GetRangeBindings());
	check(Header.NumInnerRanges == RangeBindingIt - Schema->GetRangeBindings());

	Bind(Id, FStructBinding(*Schema));
}

void FStructBindings::Bind(FStructSchemaId Id, FStructBinding Binding)
{
	if (Id.Idx >= static_cast<uint32>(Bindings.Num()))
	{
		Bindings.SetNumZeroed(Id.Idx + 1);
	}
	check(!Bindings[Id.Idx].IsBound());

	Bindings[Id.Idx] = Binding;
}

FStructBinding FStructBindings::Get(FStructSchemaId Id) const
{
	FStructBinding Out = Bindings[Id.Idx];
	check(Out.IsBound());
	return Out;
}

//void FStructBindings::DropStruct(FStructSchemaId Id);

} // namespace PlainProps