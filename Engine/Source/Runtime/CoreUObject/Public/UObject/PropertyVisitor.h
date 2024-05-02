// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class FProperty;

enum class EPropertyVisitorControlFlow : uint8
{
	Stop, // Stop the visit
	StepOver, // Skip over to the next property or item
	StepOut, // Stop iteration at this level and continue on the outer on the next property or item
	StepInto, // Introspect the inner properties if any
};

enum class EPropertyVisitorInfoType : uint8
{
	None, // Property is not inside a container
	StaticArrayIndex, // Property is a static array and has a valid index
	ContainerIndex, // Property is inside a container and has a valid index
	MapKey, // Property represents a key of the map container and has a valid index
	MapValue, // Property represents a value of the map container and has a valid index
};

struct FPropertyVisitorInfo
{
	explicit FPropertyVisitorInfo( const FProperty* InProperty, int32 InIndex = INDEX_NONE, EPropertyVisitorInfoType InPropertyInfo = EPropertyVisitorInfoType::None)
		: Property(InProperty)
		, Index(InIndex)
		, PropertyInfo(InPropertyInfo)
		, bContainsInnerProperties(false)
	{}

	FPropertyVisitorInfo(const FPropertyVisitorInfo&) = default;
	FPropertyVisitorInfo(FPropertyVisitorInfo&&) = default;

	FPropertyVisitorInfo& operator=(const FPropertyVisitorInfo&) = default;
	FPropertyVisitorInfo& operator=(FPropertyVisitorInfo&&) = default;

	void SetIndex(int32 InIndex, EPropertyVisitorInfoType InPropertyInfo)
	{
		Index = InIndex;
		PropertyInfo = InPropertyInfo;
	}

	/** The property currently being visited */
	const FProperty* Property;

	/**
	 * The parent struct that provided the property being iterated, if iterating a sub-property within a struct.
	 * @note This is slighty different than Property->GetOwnerStruct() as you might be iterating a FDerived instance 
	 *       but processing a FBase struct property. In this case this will be set to FDerived rather than FBase.
	 */
	const UStruct* ParentStructType = nullptr;

	/**
     * Index of the element being visited in the container, otherwise INDEX_NONE.
     * For maps and sets it indicates the logical index. */
	int32 Index;

	/** Whether this property is inside a container and if it is key or a value of a map */
	EPropertyVisitorInfoType PropertyInfo;

	/* Indicate that this property contains inner properties */
	bool bContainsInnerProperties;
};

struct FPropertyVisitorPath
{
public:
	FPropertyVisitorPath() = default;
	
	FPropertyVisitorPath(const FPropertyVisitorPath&) = default;
	FPropertyVisitorPath(FPropertyVisitorPath&&) = default;

	FPropertyVisitorPath& operator=(const FPropertyVisitorPath&) = default;
	FPropertyVisitorPath& operator=(FPropertyVisitorPath&&) = default;
	
	explicit FPropertyVisitorPath(const FPropertyVisitorInfo& Info)
	{
		Push(Info);
	}

	void Push(const FPropertyVisitorInfo& Info)
	{
		Path.Push(Info);
	}

	void Pop()
	{
		Path.Pop(EAllowShrinking::No);
	}

	int32 Num() const
	{
		return Path.Num();
	}

	FPropertyVisitorInfo& Top()
	{
		return Path.Top();
	}

	const FPropertyVisitorInfo& Top() const
	{
		return Path.Top();
	}

	const TArray<FPropertyVisitorInfo>& GetPath() const
	{
		return Path;
	}

	COREUOBJECT_API FString ToString(const TCHAR* Separator = TEXT(".")) const;

	/**
	 * Is this property path contained in the specified one
	 * @param OtherPath property path to check if it is contianed in
	 * @param bIsEqual optional parameter to know if it fully match
	 * @return true if it is contained in the specified path
	 */
	COREUOBJECT_API bool Contained(const FPropertyVisitorPath& OtherPath, bool* bIsEqual) const;

	/**
	 * Retrieves the data using the specified root object
	 * @param Object to search into
	 * @return 
	 */
	COREUOBJECT_API void* GetPropertyDataPtr(UObject* Object) const;

	TArray<FPropertyVisitorInfo>::TConstIterator GetRootIterator() const
	{
		return Path.CreateConstIterator();
	}

protected:
	TArray<FPropertyVisitorInfo> Path;
};

struct FPropertyVisitorScope
{
public:
	FPropertyVisitorScope(FPropertyVisitorPath& InPath, const FPropertyVisitorInfo& Info)
		: Path(InPath)
	{
		Path.Push(Info);
	}

	~FPropertyVisitorScope()
	{
		Path.Pop();
	}

protected:
	FPropertyVisitorPath& Path;
};