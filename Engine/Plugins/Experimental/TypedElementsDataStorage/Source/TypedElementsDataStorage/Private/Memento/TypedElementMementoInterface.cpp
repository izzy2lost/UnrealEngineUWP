// Copyright Epic Games, Inc. All Rights Reserved.

#include "Memento/TypedElementMementoInterface.h"

#include "TypedElementMementoSystem.h"

TypedElementTableHandle UTypedElementMementoInterface::GetUnpopulatedMementoTable()
{
	const UTypedElementMementoSystemFactory* System = GetDefault<UTypedElementMementoSystemFactory>();
	return System->GetUnpopulatedMementoTable();
}
