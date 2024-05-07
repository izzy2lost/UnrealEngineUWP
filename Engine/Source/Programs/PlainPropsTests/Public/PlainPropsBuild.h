// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsDeclare.h"
#include  "PlainPropsTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Memory/MemoryView.h"
#include "Templates/UniquePtr.h"

namespace PlainProps
{

struct FBuiltMember;
struct FBuiltStruct;
struct FBuiltRange;
class FDebugIds;
struct FUnpackedLeafType;

//////////////////////////////////////////////////////////////////////////

struct FMemberSchema
{
	FMemberType				Type;
	FOptionalSchemaId		InnerSchema;
	TArray<FMemberType>		InnerRangeTypes;

	FMemberType GetInnermostType() const { return Type.IsRange() ? InnerRangeTypes.Last() : Type; }
};

inline bool operator==(const FMemberSchema& A, const FMemberSchema& B)
{
	return A.Type == B.Type && A.InnerSchema == B.InnerSchema && A.InnerRangeTypes == B.InnerRangeTypes;
}
//////////////////////////////////////////////////////////////////////////

inline uint64 ValueCast(bool Value)			{ return static_cast<uint64>(Value); }
inline uint64 ValueCast(int8 Value)			{ return static_cast<uint8>(Value); }
inline uint64 ValueCast(int16 Value)		{ return static_cast<uint16>(Value); }
inline uint64 ValueCast(int32 Value)		{ return static_cast<uint32>(Value); }
inline uint64 ValueCast(int64 Value)		{ return static_cast<uint64>(Value); }
inline uint64 ValueCast(uint8 Value)		{ return Value; }
inline uint64 ValueCast(uint16 Value)		{ return Value; }
inline uint64 ValueCast(uint32 Value)		{ return Value; }
inline uint64 ValueCast(uint64 Value)		{ return Value; }
uint64 ValueCast(float Value);
uint64 ValueCast(double Value);
inline uint64 ValueCast(char8_t Value)		{ return static_cast<uint8>(Value); }
inline uint64 ValueCast(char16_t Value)		{ return static_cast<uint16>(Value); }
inline uint64 ValueCast(char32_t Value)		{ return static_cast<uint32>(Value); }
	
inline constexpr ERangeSizeType RangeSizeOf(bool)	{ return ERangeSizeType::Uni; }
inline constexpr ERangeSizeType RangeSizeOf(int8)	{ return ERangeSizeType::S8; }
inline constexpr ERangeSizeType RangeSizeOf(int16)	{ return ERangeSizeType::S16; }
inline constexpr ERangeSizeType RangeSizeOf(int32)	{ return ERangeSizeType::S32; }
inline constexpr ERangeSizeType RangeSizeOf(int64)	{ return ERangeSizeType::S64; }
inline constexpr ERangeSizeType RangeSizeOf(uint8)	{ return ERangeSizeType::U8; }
inline constexpr ERangeSizeType RangeSizeOf(uint16)	{ return ERangeSizeType::U16; }
inline constexpr ERangeSizeType RangeSizeOf(uint32)	{ return ERangeSizeType::U32; }
inline constexpr ERangeSizeType RangeSizeOf(uint64)	{ return ERangeSizeType::U64; }

//////////////////////////////////////////////////////////////////////////

// Todo: Turn into smart pointer where dtor call FBuiltRange::Delete if Values not detached to FBuiltValue
struct FTypedRange
{
	FMemberSchema Schema;
	FBuiltRange* Values = nullptr;
};


template<typename LeafType, typename SizeType>
FMemberSchema MakeLeafRangeSchema()
{
	check(std::is_arithmetic_v<LeafType>);
	return { FMemberType(RangeSizeOf(SizeType{})), NoId, { ReflectLeaf<LeafType>.Pack() } };
}

template<typename EnumType, typename SizeType>
FMemberSchema MakeEnumRangeSchema(FEnumSchemaId Schema)
{
	check(std::is_enum_v<EnumType>);
	return { FMemberType(RangeSizeOf(SizeType{})), Schema, { ReflectLeaf<EnumType>.Pack() } };
}

inline constexpr FMemberType DefaultStructType = FMemberType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 0});
inline constexpr FMemberType SuperStructType =	 FMemberType(FStructType{EMemberKind::Struct, /* IsDynamic */ 0, /* IsSuper */ 1});


inline FMemberSchema MakeStructRangeSchema(ERangeSizeType SizeType, FStructSchemaId Schema)
{
	return { FMemberType(SizeType), Schema, { DefaultStructType } };
}

PP_API FMemberSchema MakeNestedRangeSchema(ERangeSizeType SizeType, const FMemberSchema& InnerRangeSchema);

namespace Private
{
	PP_API [[nodiscard]] FBuiltRange* BuildStructuralRange(/* in-out */ TArrayView64<FBuiltRange*> Structs);
	PP_API [[nodiscard]] FBuiltRange* BuildStructuralRange(/* in-out */ TArrayView64<TUniquePtr<FBuiltStruct>> Structs);
	PP_API [[nodiscard]] FBuiltRange* BuildLeafRange(FUnpackedLeafType Leaf, uint64 Num, FMemoryView Values);
	PP_API void						NormalizeLeafRange(FUnpackedLeafType Leaf, FBuiltRange& Out);

	template<typename LeafType, typename SizeType>
	[[nodiscard]] FBuiltRange* BuildLeafRange(const LeafType* Values, SizeType InNum)
	{
		static_assert(SizeOf(ReflectLeaf<LeafType>.Width) == sizeof(LeafType));

		uint64 Num = IntCastChecked<uint64>(InNum);
		FBuiltRange* Range = Num > 0 ? Private::BuildLeafRange(ReflectLeaf<LeafType>, Num, MakeMemoryView(Values, Num * sizeof(LeafType))) : nullptr;
	
		if (Range && std::is_floating_point_v<LeafType>)
		{
			NormalizeLeafRange(ReflectLeaf<LeafType>, *Range);
		}
	
		return Range;
	}
}

template<typename LeafType, typename SizeType>
[[nodiscard]] FTypedRange BuildLeafRange(const LeafType* Values, SizeType Num)
{
	return { MakeLeafRangeSchema<LeafType, SizeType>(), Private::BuildLeafRange(Values, Num) };
}

template<typename LeafType, typename SizeType>
[[nodiscard]] FTypedRange BuildLeafRange(TConstArrayView<LeafType, SizeType> Values)
{
	return BuildLeafRange(Values.GetData(), Values.Num());
}

template<typename EnumType, typename SizeType>
[[nodiscard]] FTypedRange BuildEnumRange(FEnumSchemaId Enum, TConstArrayView<EnumType, SizeType> Values)
{
	return { MakeEnumRangeSchema<EnumType, SizeType>(Enum), Private::BuildLeafRange(Values.GetData(), Values.Num()) };
}

[[nodiscard]] inline FTypedRange BuildStructRange(FStructSchemaId Schema, ERangeSizeType SizeType, /* in-out */ TArrayView64<TUniquePtr<FBuiltStruct>> Values )
{
	return { MakeStructRangeSchema(SizeType, Schema), Values.Num() ? Private::BuildStructuralRange(/* ownership xfer */ Values) : nullptr };
}

// Builds an ordered list of properties to be saved
class FMemberBuilder
{
public:
	FMemberBuilder();
	~FMemberBuilder();

	template<typename LeafType>
	void Add(FMemberId Name, LeafType Value)
	{
		static_assert(std::is_arithmetic_v<LeafType>, "Illegal leaf type");
		AddLeaf(Name, ReflectLeaf<LeafType>, {}, ValueCast(Value));
	}

	template<typename EnumType>
	void AddEnum(FMemberId Name, FEnumSchemaId Schema, EnumType Value)
	{
		AddLeaf(Name, ReflectLeaf<EnumType>, ToOptional(Schema), ValueCast(Value));
	}
	
	void AddEnum8(FMemberId Name, FEnumSchemaId Schema, uint8 Value)	{ AddLeaf(Name, {ELeafType::Enum, ELeafWidth::B8},  ToOptional(Schema), Value); }
	void AddEnum16(FMemberId Name, FEnumSchemaId Schema, uint16 Value)	{ AddLeaf(Name, {ELeafType::Enum, ELeafWidth::B16}, ToOptional(Schema), Value); }
	void AddEnum32(FMemberId Name, FEnumSchemaId Schema, uint32 Value)	{ AddLeaf(Name, {ELeafType::Enum, ELeafWidth::B32}, ToOptional(Schema), Value); }
	void AddEnum64(FMemberId Name, FEnumSchemaId Schema, uint64 Value)	{ AddLeaf(Name, {ELeafType::Enum, ELeafWidth::B64}, ToOptional(Schema), Value); }

	PP_API void AddLeaf(FMemberId Name, FUnpackedLeafType Leaf, FOptionalEnumSchemaId Enum, uint64 Value);
	PP_API void AddStruct(FMemberId Name, FStructSchemaId Schema, TUniquePtr<FBuiltStruct>&& Struct);	
	PP_API void AddRange(FMemberId Name, FTypedRange&& Range);
	
	// Build members into a single nested super struct member, no-op if no non-super members has been added
	PP_API void BuildSuperStruct(const FStructDeclaration& Super, const FDebugIds& Debug);

	PP_API [[nodiscard]] TUniquePtr<FBuiltStruct> BuildAndReset(const FStructDeclaration& Declared, const FDebugIds& Debug);

private:
	TArray<FBuiltMember>	Members;
	
//	PP_API uint8* AllocateLeafRange(FMemberId Name, FUnpackedLeafType Leaf, ERangeSizeType RangeMax, uint64 Num);
	
	//template<typename T>
	//void NormalizeLeafRange(T*, uint64) {}
	//PP_API void NormalizeLeafRange(float*, uint64 Num);
	//PP_API void NormalizeLeafRange(double*, uint64 Num);
	
};

// Helper class for building struct ranges
class FStructRangeBuilder
{
public:
	FStructRangeBuilder(uint64 Num, ERangeSizeType InSizeType)
	: SizeType(InSizeType)
	{
		Structs.SetNum(Num);
	}

	template<typename IntType>
	explicit FStructRangeBuilder(IntType Num)
	: FStructRangeBuilder(static_cast<uint64>(Num), RangeSizeOf(Num))
	{}
	 
	FMemberBuilder& operator[](uint64 Idx) { return Structs[Idx]; }

	FTypedRange BuildAndReset(const FStructDeclaration& Declared, const FDebugIds& Debug);

private:
	TArray64<FMemberBuilder> Structs; 
	ERangeSizeType SizeType;
};


// Helper class for building nested ranges
class FNestedRangeBuilder
{
public:
	FNestedRangeBuilder(FMemberSchema InSchema, int64 InitialReserve)
	: Schema(InSchema)
	{
		Ranges.Reserve(InitialReserve);
	}

	~FNestedRangeBuilder();

	void Add(FTypedRange Range)
	{
		check(Range.Schema == Schema || Range.Values == nullptr);
		Ranges.Add(Range.Values);
	}

	[[nodiscard]] FTypedRange BuildAndReset(ERangeSizeType SizeType);

private:
	TArray64<FBuiltRange*> Ranges; 
	FMemberSchema Schema;
};

} // namespace PlainProps