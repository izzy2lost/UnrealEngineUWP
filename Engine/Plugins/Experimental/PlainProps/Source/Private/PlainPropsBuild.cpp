// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsBuild.h"
#include "PlainPropsIndex.h"
#include "PlainPropsInternalBuild.h"
#include "PlainPropsInternalFormat.h"
#include "Serialization/VarInt.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Hash/xxhash.h"

namespace PlainProps
{

void FBuiltStructDeleter::operator()(FBuiltStruct* Ptr) const
{
	delete Ptr;
}

FBuiltRange* FBuiltRange::Create(uint64 NumItems, SIZE_T ItemSize)
{
	check(NumItems > 0);
	FBuiltRange* Out = new (FMemory::Malloc(sizeof(FBuiltRange) + NumItems * ItemSize)) FBuiltRange;
	Out->Num = NumItems;
	return Out;
}

//////////////////////////////////////////////////////////////////////////

FMemberSchema MakeNestedRangeSchema(ERangeSizeType SizeType, const FMemberSchema& InnerRangeSchema)
{
	FMemberSchema Out = { FMemberType(SizeType), InnerRangeSchema.InnerSchema };
	Out.InnerRangeTypes.Reserve(1 + InnerRangeSchema.InnerRangeTypes.Num());
	Out.InnerRangeTypes.Add(InnerRangeSchema.Type);
	Out.InnerRangeTypes.Append(InnerRangeSchema.InnerRangeTypes);
	return Out;
}

//////////////////////////////////////////////////////////////////////////

template<typename BuiltType>
FBuiltRange* BuildStructuralRangeImpl(/* in-out */ TArrayView64<BuiltType> Values)
{
	static_assert(alignof(FBuiltRange) >= alignof(BuiltType));
	FBuiltRange* Out = FBuiltRange::Create(Values.Num(), sizeof(FBuiltRange));
	BuiltType* DataIt = reinterpret_cast<BuiltType*>(Out->Data);
	for (BuiltType& Value : Values)
	{
		new (DataIt++) BuiltType(MoveTemp(Value));
	}

	return Out;
}

namespace Private
{
	FBuiltRange* BuildStructuralRange(TArrayView64<FBuiltRange*> Values)
	{
		return PlainProps::BuildStructuralRangeImpl(Values);
	}

	FBuiltRange* BuildStructuralRange(TArrayView64<FBuiltStructPtr> Values)
	{
		return PlainProps::BuildStructuralRangeImpl(Values);
	}

	FBuiltRange* BuildLeafRange(FUnpackedLeafType Leaf, uint64 Num, FMemoryView Values)
	{
		check(Values.GetSize() == Num * SizeOf(Leaf.Width));
		FBuiltRange* Out = FBuiltRange::Create(Num, SizeOf(Leaf.Width));
		FMemory::Memcpy(Out->Data, Values.GetData(), Values.GetSize());
		return Out;
	}
	
	template<typename FloatType>
	void NormalizeFloats(FloatType* Values, uint64 Num)
	{
		for (uint64 Idx = 0; Idx < Num; ++Idx)
		{
			// Reject NaN / INF and ignore negative zero for now
			checkf(FMath::IsFinite(Values[Idx]), TEXT("Saving NaN or INF isn't supported"));
		}
	}

	void NormalizeLeafRange(FUnpackedLeafType Leaf, FBuiltRange& Out)
	{
		check(Leaf.Type == ELeafType::Float);
		if (Leaf.Width == ELeafWidth::B32)
		{
			NormalizeFloats(reinterpret_cast<float*>(Out.Data), Out.Num);
		}
		else
		{
			check(Leaf.Width == ELeafWidth::B64);
			NormalizeFloats(reinterpret_cast<double*>(Out.Data), Out.Num);
		}
	}
} // namespace Private

//////////////////////////////////////////////////////////////////////////

FMemberBuilder::FMemberBuilder() {}
FMemberBuilder::~FMemberBuilder() {}

void FMemberBuilder::AddStruct(FMemberId Name, FStructSchemaId Schema, FBuiltStructPtr&& Struct)
{
	Members.Emplace(Name, Schema, MoveTemp(Struct));
}

void FMemberBuilder::AddLeaf(FMemberId Name, FUnpackedLeafType Leaf, FOptionalEnumSchemaId Enum, uint64 Value)
{
	Members.Emplace(Name, Leaf, Enum, Value);
}

void FMemberBuilder::AddRange(FMemberId Name, FTypedRange&& Range)
{
	Members.Emplace(Name, MoveTemp(Range));
}

TSet<FBuiltStruct*> GLiveStructsFoo;

void FMemberBuilder::BuildSuperStruct(const FStructDeclaration& Super, const FDebugIds& Debug)
{
	// If we need to support EMemberPresence::RequireAll for sub structs,
	// we need access to struct declaration here or create an empty super
	// structs that we throw away in BuildAndReset.
	if (Members.IsEmpty() || (Members.Num() == 1 && IsSuper(Members[0].Schema.Type)))
	{
		return;
	}
	
	FBuiltStructPtr OnlyMember = BuildAndReset(Super, Debug);
	Members.Emplace(FBuiltMember::MakeSuper(Super.Id, MoveTemp(OnlyMember)));
	check(Members[0].Schema.Type.AsStruct().IsSuper);
}

FBuiltStructPtr FMemberBuilder::BuildAndReset(const FStructDeclaration& Declared, const FDebugIds& Debug)
{
	checkf(!(Declared.Super && Declared.Occupancy == EMemberPresence::RequireAll),
		TEXT("Requiring sub structs to be dense isn't implemented"));
#if DO_CHECK
	int32 OrderIdx = 0;
	TConstArrayView<FMemberId> Order = Declared.GetMemberOrder();
	for (FBuiltMember& Member : Members)
	{
		if (Declared.Super && IsSuper(Member.Schema.Type) && &Member == &Members[0])
		{
			continue; // Skip undeclared generated super struct
		}

		for (; OrderIdx < Order.Num() && Order[OrderIdx] != Member.Name; ++OrderIdx)
		{}	
		checkf(OrderIdx < Order.Num(), TEXT("Member '%s' in '%s' %s"), *Debug.Print(Member.Name), *Debug.Print(Declared.Type),
				Order.Contains(Member.Name) ? TEXT("appeared in non-declared order") : TEXT("is undeclared"));
		++OrderIdx;
	}
#endif

	uint32 Num = static_cast<uint32>(Members.Num());
	SIZE_T NumBytes = sizeof(FBuiltStruct) + Num * sizeof(FBuiltMember);
	FBuiltStruct* Out = reinterpret_cast<FBuiltStruct*>(FMemory::MallocZeroed(NumBytes, alignof(FBuiltStruct)));
	Out->NumMembers = IntCastChecked<uint16>(Num);
	for (FBuiltMember& Member : Members)
	{
		Out->Members[&Member - &Members[0]] = MoveTemp(Member);
	}

	Members.Reset();

	//GLiveStructsFoo.Add(Out);

	return FBuiltStructPtr(Out);
}

template<typename IntType, typename FloatType>
inline IntType CheckFiniteBitCast(FloatType Value)
{
	// Reject NaN / INF and ignore negative zero for now
	checkf(FMath::IsFinite(Value), TEXT("Saving NaN or INF isn't supported"));
	return BitCast<IntType>(Value);
}

uint64 ValueCast(float Value)
{
	return CheckFiniteBitCast<uint32>(Value);
}

uint64 ValueCast(double Value)
{ 
	return CheckFiniteBitCast<uint64>(Value);
}


//////////////////////////////////////////////////////////////////////////

FBuiltMember::FBuiltMember(FBuiltMember&& O)
: Name(O.Name)
, Schema(MoveTemp(O.Schema))
, Value(O.Value)
{
	O.Name = NoId;
	O.Schema.Type = FMemberType(ELeafType::Bool, ELeafWidth::B8);
	O.Value.Leaf = 0;
}

FBuiltMember::FBuiltMember(FMemberId Name, FUnpackedLeafType Leaf, FOptionalEnumSchemaId Enum, uint64 Value)
: FBuiltMember(Name, {Leaf.Pack(), FOptionalSchemaId(Enum)}, { .Leaf = Value})
{}

//FBuiltMember::FBuiltMember(FMemberId Name, FEnumSchemaId Schema, ELeafWidth Width, uint64 Value)
//: FBuiltMember(Name, MakeMemberSchema(Width, Schema), { .Leaf = Value })
//{}

FBuiltMember::FBuiltMember(FMemberId Name, FTypedRange&& Range)
: FBuiltMember(Name, MoveTemp(Range.Schema), { .Range = Range.Values })
{}

FBuiltMember::FBuiltMember(FMemberId Name, FStructSchemaId Schema, FBuiltStructPtr&& Value)
: FBuiltMember(Name, {DefaultStructType, FOptionalSchemaId(Schema)}, { .Struct = Value.Release() })
{}

FBuiltMember FBuiltMember::MakeSuper(FStructSchemaId Schema, FBuiltStructPtr&& Value)
{
	return FBuiltMember(NoId, {SuperStructType, FOptionalSchemaId(Schema)}, { .Struct = Value.Release() });
}

FBuiltMember::~FBuiltMember()
{
	if (Schema.Type.GetKind() == EMemberKind::Struct)
	{
		delete Value.Struct;
	}
	else if (Schema.Type.GetKind() == EMemberKind::Range)
	{
		FBuiltRange::Delete(Value.Range, Schema.InnerSchema, Schema.InnerRangeTypes);
	}
}

FBuiltMember& FBuiltMember::operator=(FBuiltMember&& O)
{
	this->~FBuiltMember();
	new (this) FBuiltMember(MoveTemp(O));
	return *this;
}

//////////////////////////////////////////////////////////////////////////

uint64 FBuiltRange::Delete(FBuiltRange* Range, FOptionalSchemaId InnerSchema, TConstArrayView<FMemberType> InnerTypes)
{ 
	// TODO: Handle struct and nested ranges

	switch (InnerTypes[0].GetKind()) 
	{
	case EMemberKind::Struct:
		//DeleteStructs(InnerTypes[0].AsStruct(), static_cast<FStructSchemaId>(Schema.InnerSchema.Get()), Range);
		break;
	case EMemberKind::Range:
		//for (FNestedRangeIterator It(Range); It;)
		//{
		//	FBuiltRange::Delete(&*It, InnerSchema, InnerTypes.RightChop(1));
		//	It.Advance();
		//}
	case EMemberKind::Leaf:		break;
	}

	FMemory::Free(Range);

	return 0; // todo: return number of bytes if needed?
}

//////////////////////////////////////////////////////////////////////////

FBuiltStruct::~FBuiltStruct()
{ 
	//check(GLiveStructsFoo.Remove(this) == 1);

	for (const FBuiltMember& Member : MakeArrayView(Members, NumMembers))
	{
		Member.~FBuiltMember();
	}
}

//////////////////////////////////////////////////////////////////////////

FTypedRange FStructRangeBuilder::BuildAndReset(const FStructDeclaration& Declared, const FDebugIds& Debug)
{
	TArray64<FBuiltStructPtr> BuiltStructs;
	BuiltStructs.Reserve(Structs.Num());
	for (FMemberBuilder& Struct : Structs)
	{
		BuiltStructs.Emplace(Struct.BuildAndReset(Declared, Debug));
	}

	return BuildStructRange(Declared.Id, SizeType, /* move */ BuiltStructs);
}

//////////////////////////////////////////////////////////////////////////

FNestedRangeBuilder::~FNestedRangeBuilder()
{
	checkf(Ranges.IsEmpty(), TEXT("Half-built range, forgot to call BuildAndReset() before destruction?"));
}

FTypedRange FNestedRangeBuilder::BuildAndReset(ERangeSizeType SizeType)
{
	FTypedRange Out = { MakeNestedRangeSchema(SizeType, Schema), 
						Ranges.IsEmpty() ? nullptr : Private::BuildStructuralRange(Ranges) };
	Ranges.Reset();
	return Out;
}

} // namespace PlainProps