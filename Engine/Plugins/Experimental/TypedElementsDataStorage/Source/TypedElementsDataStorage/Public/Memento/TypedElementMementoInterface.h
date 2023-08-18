// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

#include "TypedElementMementoInterface.generated.h"

/**
 * Add to a row to opt into behaviour to populate a memento
 * when the row is deleted
 */
USTRUCT()
struct FTypedElementMementoOnDelete : public FTypedElementDataStorageColumn
{
	// The memento row populated when the row owning this column is deleted
	GENERATED_BODY()
	TypedElementRowHandle Memento;
};

UCLASS()
class UTypedElementMementoInterface : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Gets the table type for creating an unpopulated memento row
	 */
	static TypedElementTableHandle GetUnpopulatedMementoTable();
};