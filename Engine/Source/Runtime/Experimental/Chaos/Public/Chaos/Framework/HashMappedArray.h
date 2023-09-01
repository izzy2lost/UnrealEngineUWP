// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "Containers/HashTable.h"

namespace Chaos::Private
{
	// Default ID traits for use with THashMappedArray that works for all types that can be cast to int32
	template<typename TIDType>
	struct THashMappedArrayIDTraits
	{
		using FIDType = TIDType;

		// Hash the ID to a 32 bit unsigned int for use with FHashTable
		static uint32 HashID(const FIDType& ID)
		{
			return MurmurFinalize32(uint32(ID));
		}
	};

	// Default Element traits for THashMappedArray that works for all types that have an member variable ID of type TIDType
	template<typename TElementType, typename TIDType>
	struct THashMappedArrayElementTraits
	{
		using FIDType = TIDType;
		using FElementType = TElementType;

		// Return true if the element is the one with the specified ID
		static bool Is(const FElementType& Element, const FIDType& ID)
		{
			return Element.ID == ID;
		}
	};

	/**
	* A HashMap using FHashTable to index an array of elements of type TElementType, whcih should be uniquely identified by an object of type TIDType.
	* 
	* E.g.,
	*	using FMyDataID = int32;
	*	struct FMyData
	*	{
	*		FMyDataID ID;	// Every FMyData will require a unique ID if using the default THashMappedArrayElementTraits
	*		float MyValue;
	*	};
	* 
	*	const int32 HashTableSize = 128;				// Must be power of 2
	*	THashMapedArray<FMyDataID, FMyData> MyDataMap(HashTableSize);
	* 
	*	MyDataMap.Add(1, { 1, 1.0 });					// NOTE: ID passed twice. Once for the hash map and once to construct FMyData
	*	MyDataMap.Emplace(2, 2, 2.0);					// NOTE: ID passed twice. Once for the hash map and once for forwarding args to FMyData
	* 
	*	const FMyData* MyData2 = MyDataMap.Find(2);		// MyData2->MyValue == 2.0
	* 
	*/
	template<typename TIDType, typename TElementType>
	class THashMappedArray
	{
	public:
		using FIDType = TIDType;
		using FElementType = TElementType;
		using FIDTraits = THashMappedArrayIDTraits<FIDType>;
		using FElementTraits = THashMappedArrayElementTraits<FElementType, FIDType>;
		using FHashType = uint32;
		using FType = THashMappedArray<FIDType, FElementType>;

		// Initialize the hash table. InHashSize must be a power of two (asserted)
		THashMappedArray(const int32 InHashSize)
			: HashTable(InHashSize)
		{
		}

		// Clear the hash map and reserve space for the specified number of elements (will not shrink)
		void Reset(const int32 InReserveElements)
		{
			HashTable.Clear();
			HashTable.Resize(InReserveElements);
			Elements.Reset(InReserveElements);
		}

		// Add an element with the specified ID to the map. 
		FORCEINLINE void Add(const FIDType ID, const FElementType& Element)
		{
			checkSlow(Find(ID) == nullptr);

			const int32 Index = Elements.Add(Element);
			const FHashType Key = HashMapKey(ID);

			HashTable.Add(Key, Index);
		}

		// Add an element with the specified ID to the map. 
		// NOTE: since your element type will also need to contain the ID, you usually have to pass the ID twice
		// to emplace, which is a little annoying, but shouldn't affect much.
		template <typename... ArgsType>
		FORCEINLINE void Emplace(const FIDType ID, ArgsType&&... Args)
		{
			checkSlow(Find(ID) == nullptr);

			const int32 Index = Elements.Emplace(Forward<ArgsType>(Args)...);
			const FHashType Key = FIDTraits::HashID(ID);

			HashTable.Add(Key, Index);
		}

		// Find the element with the specified ID. Roughly O(Max(1,N/M)) for N elements with a hash table of size M
		const FElementType* Find(const FIDType ID) const
		{
			return const_cast<FType*>(this)->Find(ID);
		}

		// Find the element with the specified ID. Roughly O(Max(1,N/M)) for N elements with a hash table of size M
		FElementType* Find(const FIDType ID)
		{
			const FHashType Key = FIDTraits::HashID(ID);
			for (uint32 Index = HashTable.First(Key); HashTable.IsValid(Index); Index = HashTable.Next(Index))
			{
				if (FElementTraits::Is(Elements[Index], ID))
				{
					return &Elements[Index];
				}
			}
			return nullptr;
		}

	private:
		FHashTable HashTable;
		TArray<FElementType> Elements;
	};

}