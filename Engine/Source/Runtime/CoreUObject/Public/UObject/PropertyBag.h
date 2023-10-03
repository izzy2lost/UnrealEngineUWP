// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "UObject/PropertyTag.h"

#define UE_API COREUOBJECT_API

enum EPropertyFlags : uint64;

namespace UE { class FPropertyBagElement; }

namespace UE
{

/**
 * A property bag element is a single value from the bag.
 *
 * A fixed-size array consists of multiple elements. These are not guaranteed to be contiguous in
 * memory, and iteration of the property bag skips elements that do not have a value.
 */
class FPropertyBagElement
{
public:
	/** Name, which is always valid even when the property is null. */
	FName Name;

	/** Property, which can be null if the property type is not available. */
	const FProperty* Property = nullptr;

	/** Pointer to the start of the value. This is NOT a container pointer. */
	void* Value = nullptr;

	/** Array index for fixed-size arrays. Always 0 for scalar values. */
	int32 ArrayIndex = 0;

	/** Array dimension for fixed-size arrays. Always 1 for scalar values. */
	int32 ArrayDim = 0;
};

/**
 * A property bag is an unordered collection of properties and their values.
 */
class FPropertyBag
{
	struct FValue
	{
		~FValue();

		void Destroy();

		void EnsurePropertyData(int32 ArrayIndex);
		void EnsureArrayData(int32 ArrayIndex);

		struct FData
		{
			void* Memory = nullptr;
			int32 Size = 0;
		};

		FPropertyTag Tag;

		// A value is stored in one of several ways based on what we know of the property.
		//
		// A scalar is stored in Data.
		// An array is stored in Data if it is of known dimension.
		// An array is stored in Data if it is of unknown dimension and only index 0 is known.
		// An array is stored in ArrayData if it is of unknown dimension.
		//
		// Arrays use ArrayAllocationFlags to track which values are known.
		//
		// A value is stored in its native form if it has a known property or a property can be created dynamically.
		// A value is otherwise stored in its serialized form.

		FData Data;
		TMap<int32, FData>* ArrayData = nullptr;
		TBitArray<>* ArrayAllocationFlags = nullptr;
	};

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
	 * Copy every non-skipped property of the struct into the property bag.
	 *
	 * Any property in the bag with the same name will be overwritten.
	 */
	UE_API void Copy(const UStruct* Struct, void* Data, EPropertyFlags SkipFlags);

	/**
	 * Add the property to the bag.
	 *
	 * Any property in the bag with the same name will be overwritten.
	 */
	UE_API void Add(FProperty* Property, void* Data, int32 ArrayIndex = 0);

	/**
	 * Remove a property from the bag by name.
	 */
	UE_API void Remove(FName Name);

	/**
	 * Load a property from the slot into the bag based on the tag.
	 *
	 * If the property has a known type then it will be deserialized and accessible.
	 * If not, the serialized data will be retained to avoid losing the unrecognized property.
	 */
	UE_API void LoadPropertyByTag(const FPropertyTag& Tag, FStructuredArchiveSlot& ValueSlot, const void* Defaults = nullptr);

	template <bool bConst, bool bRangedFor>
	class TBaseIterator
	{
	public:
		using FPropertyIterator = std::conditional_t<bConst,
			std::conditional_t<bRangedFor, typename TMap<FName, FValue>::TRangedForConstIterator, typename TMap<FName, FValue>::TConstIterator>,
			std::conditional_t<bRangedFor, typename TMap<FName, FValue>::TRangedForIterator, typename TMap<FName, FValue>::TIterator>>;

		inline explicit TBaseIterator(const FPropertyIterator& InPropertyIt)
			: PropertyIt(InPropertyIt)
		{
			if (PropertyIt)
			{
				AssignElement(Element, PropertyIt.Key(), PropertyIt.Value());
			}
		}

		inline TBaseIterator& operator++()
		{
			if (UNLIKELY(++Element.ArrayIndex < Element.ArrayDim && AdvanceElement(Element, PropertyIt.Value())))
			{
			}
			else if (++PropertyIt)
			{
				AssignElement(Element, PropertyIt.Key(), PropertyIt.Value());
			}
			else
			{
				Element = {};
			}
			return *this;
		}

		inline explicit operator bool() const { return !!Element.ArrayDim; }

		inline bool operator==(const TBaseIterator& Rhs) const { return Element.Value == Rhs.Element.Value; }
		inline bool operator!=(const TBaseIterator& Rhs) const { return Element.Value != Rhs.Element.Value; }

		inline const FPropertyBagElement& operator*() const { return Element; }
		inline const FPropertyBagElement* operator->() const { return &Element; }

	private:
		FPropertyIterator PropertyIt;
		FPropertyBagElement Element;
	};

	using TIterator = TBaseIterator</*bConst*/ false, /*bRangedFor*/ false>;
	using TConstIterator = TBaseIterator</*bConst*/ true, /*bRangedFor*/ false>;

	inline TIterator CreateIterator() { return TIterator(Properties.CreateIterator()); }
	inline TConstIterator CreateConstIterator() const { return TConstIterator(Properties.CreateConstIterator()); }

private:
	using TRangedForIterator = TBaseIterator</*bConst*/ false, /*bRangedFor*/ true>;
	using TRangedForConstIterator = TBaseIterator</*bConst*/ true, /*bRangedFor*/ true>;

	friend inline TRangedForIterator begin(FPropertyBag& Bag) { return TRangedForIterator(Bag.Properties.begin()); }
	friend inline TRangedForIterator end(FPropertyBag& Bag) { return TRangedForIterator(Bag.Properties.end()); }

	friend inline TRangedForConstIterator begin(const FPropertyBag& Bag) { return TRangedForConstIterator(Bag.Properties.begin()); }
	friend inline TRangedForConstIterator end(const FPropertyBag& Bag) { return TRangedForConstIterator(Bag.Properties.end()); }

	UE_API static void AssignElement(FPropertyBagElement& Element, const FName& Name, const FValue& Value);
	UE_API static bool AdvanceElement(FPropertyBagElement& Element, const FValue& Value);

	TMap<FName, FValue> Properties;
};

} // UE

#undef UE_API
