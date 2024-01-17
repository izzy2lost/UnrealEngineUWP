// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"

// assumes EnumType is an enum 
// where EnumType::Num is a member
// and corresponds to the number of
// enumerated values.
template<typename EnumType, typename ValueType> 
struct TEnumArray
{
public:
	using FValueType = ValueType;
	using EEnumType = EnumType;
	virtual ~TEnumArray() {}

	// TODO: doesn't compile
	//template <typename ItArrayType, typename ItValueType>
	//struct TBaseIterator
	//{
	//public:

	//	FORCEINLINE TBaseIterator(ItArrayType& InIterArray, int Index)
	//		: IterArray(InIterArray)
	//		, IterIndex(Index)
	//	{}
	//	
	//	FORCEINLINE ItValueType& operator*() const { return IterArray[IterIndex]; }
	//	FORCEINLINE ItValueType* operator->() const { return &IterArray[IterIndex]; }

	//	FORCEINLINE bool operator==(const TBaseIterator& Other) const { return IterIndex == Other.IterIndex; }
	//	FORCEINLINE bool operator!=(const TBaseIterator& Other) const { return IterIndex != Other.IterIndex; }

	//	FORCEINLINE int Index() const { return IterIndex; }
	//	FORCEINLINE EnumType Key() const { return (EnumType)IterIndex; }

	//	FORCEINLINE TBaseIterator& operator++() { ++IterIndex; return *this; }

	//private:

	//	ItArrayType& IterArray;
	//	int IterIndex = 0;
	//};

	//typedef TBaseIterator<TEnumArray, ValueType> TIterator;
	//typedef TBaseIterator<const TEnumArray, const ValueType> TConstIterator;

	//FORCEINLINE TIterator begin() { return TIterator(*this, 0); }
	//FORCEINLINE TIterator end() { return TIterator(*this, Num()); }
	//FORCEINLINE TConstIterator begin() const { return TConstIterator(*this, 0); }
	//FORCEINLINE TConstIterator end() const { return TConstIterator(*this, Num()); }

	ValueType& operator[](EnumType e)
	{
		int Index = (int)e;
		check(IsValidIndex(Index));
		return Array[Index];
	}

	const ValueType& operator[](EnumType e) const
	{
		int Index = (int)e;
		check(IsValidIndex(Index));
		return Array[Index];
	}

	ValueType& operator[](int Index)
	{
		check(IsValidIndex(Index));
		return Array[Index];
	}

	const ValueType& operator[](int Index) const
	{
		check(IsValidIndex(Index));
		return Array[Index];
	}

	static constexpr int Num()
	{
		return (int)EnumType::Num;
	}

	bool IsValidIndex(int Index) const
	{
		return Index >= 0 && Index < Num();
	}

	TEnumArray& operator=(const TEnumArray& Other)
	{
		for (int idx = 0; idx < Num(); ++idx)
		{
			Array[idx] = Other.Array[idx];
		}
		return *this;
	}
protected:

	ValueType& Get(EnumType e)
	{
		int Index = (int)e;
		check(IsValidIndex(Index));
		return Array[Index];
	}

private:

	ValueType Array[Num()];
};


/**********
* 
* usage 
* 
* 
* EStructIndex
* {
*	Zero,
*	One,
*	Num
* }
* 
* FStruct
* {
*	int Data = 0;
* }
* 
* // allocates an array of structs with size EStructIndex::Num
* TEnumArray<EStructIndex, FStruct> EnumStruct;
* 
* // access statically allocated struct by reference
* FStruct& ZeroRef = EnumStruct[EStructIndex::Zero]
* 
* 
***********/

// assumes EnumType::Num is a valid value and is the correct MAX number in the Enum
#define IMPLEMENT_ENUM_ARRAY(ArrayName, EnumType, StructType)											\
	using FValueType = StructType;																		\
	using FArrayType = StructType ## Array;																\
	using EEnumType = EnumType;																			\
																										\
	const StructType& operator[](int Index) const														\
	{																									\
		check(IsValidIndex(Index));																		\
		return ArrayName[Index];																		\
	}																									\
																										\
	StructType& operator[](int Index)																	\
	{																									\
		check(IsValidIndex(Index));																		\
		return ArrayName[Index];																		\
	}																									\
																										\
	const StructType& operator[](EnumType Index) const													\
	{																									\
		check(IsValidIndex(static_cast<int>(Index)))													\
		return ArrayName[static_cast<int>(Index)];														\
	}																									\
																										\
	StructType& operator[](EnumType Index)																\
	{																									\
		check(IsValidIndex(static_cast<int>(Index)));													\
		return ArrayName[static_cast<int>(Index)];														\
	}																									\
																										\
	static constexpr int Num()																			\
	{																									\
		return static_cast<int>(EnumType::Num);															\
	}																									\
																										\
	bool IsValidIndex(int Index) const																	\
	{																									\
		return Index >= 0 && Index < Num();																\
	}																									\
																										\
	StructType ## Array() = default;																	\
																										\
	StructType ## Array(const FArrayType& Other) = default;												\
																										\
	FArrayType& operator=(const FArrayType& Other)														\
	{																									\
		for (int idx = 0; idx < Num(); ++idx)															\
		{																								\
			ArrayName[idx] = Other.GetArray()[idx];														\
		}																								\
		return *this;																					\
	}																									\
																										\
private:																								\
	const StructType* GetArray() const { return ArrayName; }											\
		  StructType* GetArray()       { return ArrayName; }											\
																										\
public:					

// Makes a property with the name "Name", for a TEnumArray or a Struct that uses IMPLEMENT_ENUM_ARRAY
// The property will then be a reference to the array element indexed by the enum value of the same name
// assumes "this" implements the ref by index operator (operator[])
#define ENUM_PROPERTY(Name) FValueType& Name = this->operator[](EEnumType::Name)