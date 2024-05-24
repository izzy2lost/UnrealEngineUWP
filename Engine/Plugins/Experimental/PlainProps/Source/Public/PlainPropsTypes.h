// Copyright Epic Games, Inc. All Rights Reserved.
#if 0
#pragma once

#include "Containers/ArrayView.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include <type_traits>

namespace PlainProps
{

enum class EMemberKind : uint8 { Leaf, Struct, Range };
enum class ELeafType : uint8 { Bool, IntS, IntU, Float, Hex, Enum, Unicode };
enum class ELeafWidth : uint8 { B8, B16, B32, B64 };
enum class ERangeSizeType : uint8 { Uni, S8, U8, S16, U16, S32, U32, S64, U64 };

inline constexpr SIZE_T SizeOf(ELeafWidth Width)
{
	return SIZE_T(1) << static_cast<uint32>(Width);
}

inline SIZE_T SizeOf(ERangeSizeType Width)
{
	check(Width != ERangeSizeType::Uni);
    return SIZE_T(1) << ((uint8)Width - 1) / 2; 
}

inline constexpr uint64 Max(ERangeSizeType Width)
{
    const uint8_t LeadingZeroes[] = {63, 57, 56, 49, 48, 33, 32, 1, 0};
    return ~uint64_t(0) >> LeadingZeroes[(uint8)Width]; 
}

template<SIZE_T Size>
constexpr ELeafWidth IllegalLeafWidth()
{
	static_assert(Size == 1);
	return ELeafWidth::B8;
}

template<SIZE_T Size> constexpr ELeafWidth LeafWidth = IllegalLeafWidth<Size>();
template<> inline constexpr ELeafWidth LeafWidth<1> = ELeafWidth::B8;
template<> inline constexpr ELeafWidth LeafWidth<2> = ELeafWidth::B16;
template<> inline constexpr ELeafWidth LeafWidth<4> = ELeafWidth::B32;
template<> inline constexpr ELeafWidth LeafWidth<8> = ELeafWidth::B64;

//////////////////////////////////////////////////////////////////////////

struct FLeafType
{
    EMemberKind		_ : 2;
    ELeafWidth		Width : 2;
    ELeafType		Type : 3;
};

struct FRangeType
{
    EMemberKind		_ : 2;
    ERangeSizeType	MaxSize : 4;
};

struct FStructType
{
    EMemberKind		_ : 2;
	uint8			IsDynamic : 1;	// Which schema stored in value stream, different instances in same batch have different types
	uint8			IsSuper : 1;	// Inherited members from some super struct, can only appear as first member
//	uint8			IsDense : 1;	// Potential optimization to avoid accessing schema
};

union FMemberType
{
	FMemberType() = default; // Uninitialized
	constexpr explicit FMemberType(FLeafType InLeaf) : Leaf(InLeaf) {}
	constexpr explicit FMemberType(ELeafType Type, ELeafWidth Width) : Leaf({EMemberKind::Leaf, Width, Type}) {}
	constexpr explicit FMemberType(FRangeType InRange) : Range(InRange) {}
	constexpr explicit FMemberType(ERangeSizeType MaxSize) : Range({EMemberKind::Range, MaxSize}) {}
	constexpr explicit FMemberType(FStructType InStruct) : Struct(InStruct) {}

	bool			IsLeaf() const		{ return Kind == EMemberKind::Leaf; }
	bool			IsRange() const		{ return Kind == EMemberKind::Range; }
	bool			IsStruct() const	{ return Kind == EMemberKind::Struct; }
	EMemberKind     GetKind() const		{ return Kind; }

	FLeafType		AsLeaf() const		{ check(IsLeaf());		return Leaf; }
	FRangeType		AsRange() const		{ check(IsRange());		return Range; }
	FStructType		AsStruct() const	{ check(IsStruct());	return Struct; }
	uint8			AsByte() const		{ return reinterpret_cast<const uint8&>(*this); }

	friend inline bool operator==(FMemberType A, FMemberType B) { return A.AsByte() == B.AsByte(); }
private:
    EMemberKind     Kind : 2;
    FLeafType	    Leaf;
    FRangeType		Range;
    FStructType		Struct;
};

//////////////////////////////////////////////////////////////////////////

struct FNameId
{
	uint32 Idx = ~0u;

	bool operator==(FNameId O) const { return Idx == O.Idx; }
	friend uint32 GetTypeHash(FNameId Id) { return Id.Idx; };
};

struct FMemberId
{
	FNameId Id;

	bool operator==(FMemberId O) const { return Id == O.Id; }
	friend uint32 GetTypeHash(FMemberId Member) { return Member.Id.Idx; };
};

//////////////////////////////////////////////////////////////////////////

struct FSchemaId
{
	uint32 Idx = ~0u;
	
	// TODO: Consider moving these to FEnumSchemaId/FStructSchemaId.
	// They're not comparable in the building stage when the overlap.
	// After remap they are comparable during reading.
	bool operator==(FSchemaId O) const { return Idx == O.Idx; }
	friend uint32 GetTypeHash(FSchemaId Schema) { return GetTypeHash(Schema.Idx); };
};
struct FStructSchemaId : FSchemaId {};
struct FEnumSchemaId : FSchemaId {};

//////////////////////////////////////////////////////////////////////////

struct FNestedScopeId { uint32 Idx; };
struct FFlatScopeId { FNameId Name; };

class FScopeId
{
	static constexpr uint32 NestedBit = 0x80000000u;
	uint32 Handle;
public:
	explicit FScopeId(FFlatScopeId Flat) : Handle(Flat.Name.Idx) 				{ check(AsFlat().Name == Flat.Name); }
	explicit FScopeId(FNestedScopeId Nested) : Handle(Nested.Idx | NestedBit) 	{ check(AsNested().Idx == Nested.Idx); }

	bool							IsFlat() const 					{ return  !(Handle & NestedBit); }
	bool							IsNested() const 				{ return !!(Handle & NestedBit); }
	FFlatScopeId					AsFlat() const 					{ check(IsFlat()); 		return {FNameId{Handle}}; }
	FNestedScopeId					AsNested() const				{ check(IsNested()); 	return {Handle & ~NestedBit}; }
	uint32							AsInt() const					{ return Handle; }
	bool							operator==(FScopeId O) const	{ return Handle == O.Handle; }
};

//////////////////////////////////////////////////////////////////////////

struct FConcreteTypenameId
{
	FNameId Id;
	bool operator==(FConcreteTypenameId O) const { return Id == O.Id; }
};

struct FBaseTypenameId
{
	FBaseTypenameId(uint8 InNumParameters, uint32 InIdx)
	: NumParameters(InNumParameters)
	, Idx(InIdx)
	{
		check(Idx == InIdx);
	}

	uint32 NumParameters : 8;
	uint32 Idx : 24;

	uint32 AsInt() const { return (Idx << 8) + NumParameters; }
	static FBaseTypenameId FromInt(uint32 Int) { return FBaseTypenameId(static_cast<uint8>(Int), Int >> 8); }
	bool operator==(FBaseTypenameId O) const { return AsInt() == O.AsInt(); }
};

struct FParametricTypeId : FBaseTypenameId
{
	using FBaseTypenameId::FBaseTypenameId;
	static FParametricTypeId FromInt(uint32 Int) { return static_cast<FParametricTypeId&&>(FBaseTypenameId::FromInt(Int)); }
};

struct FTypenameId : FBaseTypenameId
{
	explicit FTypenameId(FParametricTypeId Parametric) : FBaseTypenameId(Parametric) { check(AsParametric().AsInt() == Parametric.AsInt()); }
	explicit FTypenameId(FConcreteTypenameId Concrete) : FBaseTypenameId(0, Concrete.Id.Idx) { check(AsConcrete().Id == Concrete.Id); }

	bool							IsConcrete() const 		{ return NumParameters == 0; }
	bool							IsParametric() const 	{ return !IsConcrete(); }
	FConcreteTypenameId				AsConcrete() const 		{ check(IsConcrete());		return { FNameId{Idx} };  }
	FParametricTypeId				AsParametric() const 	{ check(IsParametric());	return FParametricTypeId(NumParameters, Idx); }
};

//////////////////////////////////////////////////////////////////////////

struct FTypeId
{
	FScopeId 		Scope;
	FTypenameId		Name;

	bool operator==(FTypeId O) const { return Scope.AsInt() == O.Scope.AsInt() && Name.AsInt() == O.Name.AsInt(); }
	friend uint32 GetTypeHash(FTypeId Type) { return HashCombineFast(Type.Scope.AsInt(), Type.Name.Idx); };
};

//////////////////////////////////////////////////////////////////////////

struct FNoId {};
inline constexpr FNoId NoId;


inline uint32 ToIdx(FNameId Id) { return Id.Idx; }
inline uint32 ToIdx(FMemberId Name) { return Name.Id.Idx; }
inline uint32 ToIdx(FSchemaId Id) { return Id.Idx; }
inline uint32 ToIdx(FNestedScopeId Id) { return Id.Idx; }
inline uint32 ToIdx(FParametricTypeId Id) { return Id.AsInt(); }

template<class IdType>
IdType FromIdx(uint32 Idx) { return {Idx}; }

template<>
inline FMemberId FromIdx(uint32 Idx) { return {{Idx}}; }

template<>
inline FParametricTypeId FromIdx(uint32 Idx) { return FParametricTypeId::FromInt(Idx); }

class FOptionalId
{
protected:
    uint32 Idx = ~0u; 
public:
    explicit operator bool() const { return Idx != ~0u; }
	friend uint32 GetTypeHash(FOptionalId Id) { return Id.Idx; };
};

template<class T>
class TOptionalId : public FOptionalId
{
public:
	constexpr TOptionalId() = default;
	constexpr TOptionalId(FNoId) {}
    constexpr TOptionalId(T Id) { Idx = ToIdx(Id); }
    
	template<class U>
    explicit operator TOptionalId<U>() const
    {
		static_assert(std::is_convertible_v<U, T> || std::is_convertible_v<T, U>);
		return static_cast<const TOptionalId<U>&>(static_cast<const FOptionalId&>(*this));
    }

    T Get() const
	{
		check(*this);
		return FromIdx<T>(Idx);
	}

	friend bool operator==(TOptionalId A, TOptionalId B) { return A.Idx == B.Idx;}
	friend bool operator!=(TOptionalId A, TOptionalId B) { return A.Idx != B.Idx;}
};


using FOptionalNameId = TOptionalId<FNameId>;
using FOptionalMemberId = TOptionalId<FMemberId>;
using FOptionalSchemaId = TOptionalId<FSchemaId>;
using FOptionalStructSchemaId = TOptionalId<FStructSchemaId>;
using FOptionalEnumSchemaId = TOptionalId<FEnumSchemaId>;
using FOptionalNestedScopeId = TOptionalId<FNestedScopeId>;
using FOptionalParametricTypeId = TOptionalId<FParametricTypeId>;

template<class IdType>
inline constexpr TOptionalId<IdType> ToOptional(IdType Id) { return Id; }

//////////////////////////////////////////////////////////////////////////

// Resolved FNestedScopeId
struct FNestedScope
{
	FScopeId		Outer;
	FFlatScopeId 	Inner;

	bool operator==(FNestedScope O) const { return Outer.AsInt() == O.Outer.AsInt() && Inner.Name == O.Inner.Name; }
	friend uint32 GetTypeHash(FNestedScope Scope) { return HashCombineFast(Scope.Outer.AsInt(), Scope.Inner.Name.Idx); };
};


struct FParameterIndexRange : FBaseTypenameId { using FBaseTypenameId::FBaseTypenameId; };

// Name-resolved FParametricTypeId
struct FParametricType
{
	FConcreteTypenameId		Name;
	FParameterIndexRange	Parameters;

	bool operator==(FParametricType O) const { return Name == O.Name && Parameters.AsInt() == O.Parameters.AsInt(); }
};

// Fully resolved FParametricTypeId
struct FParametricTypeView
{
	FParametricTypeView(FConcreteTypenameId InName, uint8 NumParams, const FTypeId* Params) : Name(InName), NumParameters(NumParams), Parameters(Params) {}
	FParametricTypeView(FConcreteTypenameId InName, TConstArrayView<FTypeId> Params);

	FConcreteTypenameId			Name;
	uint8						NumParameters;
	const FTypeId*				Parameters;

	TConstArrayView<FTypeId>	GetParameters() const { return MakeArrayView(Parameters, NumParameters); }
};

//////////////////////////////////////////////////////////////////////////

struct FUnpackedLeafType
{
	ELeafType Type;
	ELeafWidth Width;

	constexpr FUnpackedLeafType(ELeafType InType, ELeafWidth InWidth) : Type(InType), Width(InWidth) {}
	constexpr FUnpackedLeafType(FLeafType In) : Type(In.Type), Width(In.Width) {}

	constexpr bool operator==(FUnpackedLeafType O) const { return Type == O.Type && Width == O.Width; }
	FMemberType Pack() const { return FMemberType(Type, Width); }
};

template<typename T>
constexpr FUnpackedLeafType ReflectEnum()
{
	static_assert(std::is_enum_v<T>);
	return FUnpackedLeafType(ELeafType::Enum, LeafWidth<sizeof(T)>);
}

template<typename T>
constexpr FUnpackedLeafType ReflectLeaf = ReflectEnum<T>();

template<> inline constexpr FUnpackedLeafType ReflectLeaf<bool>		= { ELeafType::Bool,	ELeafWidth::B8 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<int8>		= { ELeafType::IntS,	ELeafWidth::B8 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<int16>	= { ELeafType::IntS,	ELeafWidth::B16 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<int32>	= { ELeafType::IntS,	ELeafWidth::B32 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<int64>	= { ELeafType::IntS,	ELeafWidth::B64 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<uint8>	= { ELeafType::IntU,	ELeafWidth::B8 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<uint16>	= { ELeafType::IntU,	ELeafWidth::B16 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<uint32>	= { ELeafType::IntU,	ELeafWidth::B32 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<uint64>	= { ELeafType::IntU,	ELeafWidth::B64 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<float>	= { ELeafType::Float,	ELeafWidth::B32 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<double>	= { ELeafType::Float,	ELeafWidth::B64 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<char>		= { ELeafType::Unicode,	ELeafWidth::B8 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<char8_t>	= { ELeafType::Unicode,	ELeafWidth::B8 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<char16_t>	= { ELeafType::Unicode,	ELeafWidth::B16 };
template<> inline constexpr FUnpackedLeafType ReflectLeaf<char32_t>	= { ELeafType::Unicode,	ELeafWidth::B32 };

//////////////////////////////////////////////////////////////////////////

template<typename T>
const T* AlignPtr(const void* Ptr)
{
	return static_cast<const T*>(Align(Ptr, alignof(T)));
}

//////////////////////////////////////////////////////////////////////////

class FDebugIds
{
public:
	virtual ~FDebugIds() {}

	virtual FParametricTypeView			Resolve(FParametricTypeId Id) const = 0;
	virtual FNestedScope				Resolve(FNestedScopeId Id) const = 0;
	virtual FTypeId						Resolve(FEnumSchemaId Id) const = 0;
	virtual FTypeId						Resolve(FStructSchemaId Id) const = 0;

	virtual void						AppendDebugString(FString& Out, FNameId Name) const = 0;
	virtual void						AppendDebugString(FString& Out, FScopeId Scope) const;
	virtual void						AppendDebugString(FString& Out, FTypenameId Typename) const;
	virtual void						AppendDebugString(FString& Out, FTypeId Type) const;
	virtual void						AppendDebugString(FString& Out, FEnumSchemaId Name) const;
	virtual void						AppendDebugString(FString& Out, FStructSchemaId Name) const;

	PLAINPROPS_API FString				Print(FNameId Name) const;
	PLAINPROPS_API FString				Print(FMemberId Name) const;
	PLAINPROPS_API FString				Print(FOptionalMemberId Name) const;
	PLAINPROPS_API FString				Print(FTypeId Type) const;
	PLAINPROPS_API FString				Print(FEnumSchemaId Name) const;
	PLAINPROPS_API FString				Print(FStructSchemaId Name) const;
};

} // namespace PlainProps
#endif