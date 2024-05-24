// Copyright Epic Games, Inc. All Rights Reserved.
#if 0
#pragma once

#include "PlainPropsBuild.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace PlainProps
{

union FBuiltValue
{
	uint64			Leaf;
	FBuiltStruct*	Struct;
	FBuiltRange*	Range;
};

struct FBuiltMember
{
	FBuiltMember() = delete;
	FBuiltMember(const FBuiltMember&) = delete;
	FBuiltMember(FBuiltMember&& O);
	FBuiltMember(FMemberId Name, FUnpackedLeafType Leaf, FOptionalEnumSchemaId Schema, uint64 Value);
	//FBuiltMember(FMemberId Name, FEnumSchemaId Schema, ELeafWidth Width,  uint64 Value);
	FBuiltMember(FMemberId Name, FTypedRange&& Range);
	FBuiltMember(FMemberId Name, FStructSchemaId Schema, TUniquePtr<FBuiltStruct>&& Value);
	static FBuiltMember MakeSuper(FStructSchemaId Schema, TUniquePtr<FBuiltStruct>&& Value);
	~FBuiltMember(); // Deletes Value

	FBuiltMember& operator=(const FBuiltMember&) = delete;
	FBuiltMember& operator=(FBuiltMember&&);

	FOptionalMemberId		Name;
	FMemberSchema			Schema;
	FBuiltValue				Value;

private:
	FBuiltMember(FOptionalMemberId N, FMemberSchema&& S, FBuiltValue V) : Name(N), Schema(MoveTemp(S)), Value(V) {}
};

struct FBuiltStruct
{
	~FBuiltStruct();

	uint16				NumMembers;
	FBuiltMember		Members[0];
};

struct FBuiltRange
{
	~FBuiltRange() = delete;
	[[nodiscard]] static FBuiltRange*					Create(uint64 NumItems, SIZE_T ItemSize);
	static uint64										Delete(FBuiltRange* Range, FOptionalSchemaId InnerSchema, TConstArrayView<FMemberType> InnerTypes);

	uint64												Num;
	uint8												Data[0];
	
	TConstArrayView64<const FBuiltRange*>				AsRanges() const { return { reinterpret_cast<FBuiltRange const* const*>(Data), IntCastChecked<int64>(Num) }; }
	TConstArrayView64<TUniquePtr<const FBuiltStruct>>	AsStructs() const { return { reinterpret_cast<const TUniquePtr<const FBuiltStruct>*>(Data), IntCastChecked<int64>(Num) }; }
};

//////////////////////////////////////////////////////////////////////////

inline void WriteData(TArray64<uint8>& Out, const void* Data, int64 Size)
{
	Out.Append(static_cast<const uint8*>(Data), Size);
}

template<typename ArrayType>
void WriteArray(TArray64<uint8>& Out, const ArrayType& In)
{
	static_assert(TIsContiguousContainer<ArrayType>::Value);
	WriteData(Out, In.GetData(), sizeof(typename ArrayType::ElementType) * In.Num());
}

template<typename T>
void WriteAlignmentPadding(TArray64<uint8>& Out)
{
	Out.AddZeroed(Align(Out.Num(), alignof(T)) - Out.Num());
}	

template<typename T>
void WriteAlignedArray(TArray64<uint8>& Out, TArrayView<T> In)
{
	WriteAlignmentPadding<T>(Out);
	WriteArray(Out, In);
}

template<typename T>
inline void WriteInt(TArray64<uint8>& Out, T Number)
{
	static_assert(std::is_integral_v<T>);
	static_assert(PLATFORM_LITTLE_ENDIAN);
	WriteData(Out, &Number, sizeof(T));
}

inline void WriteU32(TArray64<uint8>& Out, uint32 Int) { WriteInt(Out, Int); }
inline void WriteU64(TArray64<uint8>& Out, uint64 Int) { WriteInt(Out, Int); }

PLAINPROPS_API uint64 WriteSkippableSlice(TArray64<uint8>& Out, TConstArrayView64<uint8> Slice);


} // namespace PlainProps
#endif