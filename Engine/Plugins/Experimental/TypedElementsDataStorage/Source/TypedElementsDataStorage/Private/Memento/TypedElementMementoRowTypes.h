// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "TypedElementMementoRowTypes.generated.h"

/**
 * @file MementoRowTypes.h
 * Column/Tags used internally for the memento system
 */

/**
 * MementoTag denotes that the row is a memento
 */
USTRUCT()
struct FTypedElementMementoTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

/**
 * Column for a memento row indicating that the row is populated with memento data
 */
USTRUCT()
struct FTypedElementMementoPopulated : public FTypedElementDataStorageColumn
{
	// Frame number that the memento row became populated
	GENERATED_BODY()
	uint64 FrameNumber;
};