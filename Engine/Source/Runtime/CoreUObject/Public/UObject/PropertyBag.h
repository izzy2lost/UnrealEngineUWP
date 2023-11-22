// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Misc/TVariant.h"
#include "UObject/PropertyPathName.h"
#include "UObject/PropertyTag.h"

#define UE_API COREUOBJECT_API

enum EPropertyFlags : uint64;

namespace UE { class FPropertyBagElement; }

namespace UE
{

/**
 * A property bag is an unordered collection of properties and their values.
 */
class FPropertyBag
{
	struct FValue
	{
		~FValue();

		void AllocateAndInitializeValue();
		void Destroy();

		UE_API int32 GetSize() const;

		FPropertyTag Tag;
		void* Data = nullptr;
	};

	struct FNode;
	using FNodeMap = TMap<FName, TUniquePtr<FNode>>;

	struct FNode
	{
		FName Type;
		TVariant<TYPE_OF_NULLPTR, FValue, FNodeMap> ValueOrNodes;
	};

	FValue& FindOrCreateValue(const FPropertyPathName& Path);

public:
	UE_API FPropertyBag();
	UE_API ~FPropertyBag();

	UE_API FPropertyBag(FPropertyBag&&);
	UE_API FPropertyBag& operator=(FPropertyBag&&);

	FPropertyBag(const FPropertyBag&) = delete;
	FPropertyBag& operator=(const FPropertyBag&) = delete;

	/** True if the bag contains no properties. */
	inline bool IsEmpty() const { return Properties.IsEmpty(); }

	/**
	 * Remove every property from the property bag.
	 */
	UE_API void Empty();

	/**
	 * Add the property to the bag.
	 *
	 * Any property in the bag with the same path will be overwritten.
	 */
	UE_API void Add(const FPropertyPathName& Path, FProperty* Property, void* Data, int32 ArrayIndex = 0);

	/**
	 * Remove a property from the bag by path.
	 */
	UE_API void Remove(const FPropertyPathName& Path);

	/**
	 * Load a property from the slot into the bag based on the tag.
	 *
	 * If the property has a known type then it will be deserialized and accessible.
	 * If not, the serialized data will be retained to avoid losing the unrecognized property.
	 */
	UE_API void LoadPropertyByTag(const FPropertyPathName& Path, const FPropertyTag& Tag, FStructuredArchiveSlot& ValueSlot, const void* Defaults = nullptr);

	class FConstIterator
	{
	public:
		using FNodeIterator = typename FNodeMap::TConstIterator;

		UE_API explicit FConstIterator(const FNodeIterator& NodeIt);

		UE_API FConstIterator& operator++();

		inline explicit operator bool() const { return !!NodeIterators.Last(); }

		inline bool operator==(const FConstIterator& Rhs) const { return NodeIterators.Last() == Rhs.NodeIterators.Last(); }
		inline bool operator!=(const FConstIterator& Rhs) const { return NodeIterators.Last() != Rhs.NodeIterators.Last(); }

		inline const FPropertyPathName& GetPath() const { return CurrentPath; }
		inline const FProperty* GetProperty() const { return CurrentValue->Tag.Prop; }
		inline void* GetValue() const { return CurrentValue->Data; }
		inline int32 GetValueSize() const { return CurrentValue->GetSize(); }

	private:
		void EnterNode();

		TArray<FNodeIterator, TInlineAllocator<8>> NodeIterators;
		FPropertyPathName CurrentPath;
		FValue* CurrentValue = nullptr;
	};

	inline FConstIterator CreateConstIterator() const { return FConstIterator(Properties.CreateConstIterator()); }

private:
	FNodeMap Properties;
};

} // UE

#undef UE_API
