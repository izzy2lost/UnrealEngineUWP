// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheReader.h"
#include "AvaBlueprint_Serialize.h"

FAvalancheReader::FAvalancheReader(FAvalancheWorldData& InWorldData, FAvalancheObjectData& InObjectData, UObject* InObject)
	: Super(InWorldData, InObjectData, InObject, true)
{
	InObject->Serialize(*this);
}

UObject* FAvalancheReader::ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const
{
	return FAvaBlueprint_Serialize::ResolveObjectDependency(WorldData, ObjectIndex);
}
