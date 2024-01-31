// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaReader.h"
#include "AvaBlueprint_Serialize.h"

FAvaReader::FAvaReader(FAvaWorldData& InWorldData, FAvaObjectData& InObjectData, UObject* InObject)
	: Super(InWorldData, InObjectData, InObject, true)
{
	InObject->Serialize(*this);
}

UObject* FAvaReader::ResolveObjectDependency(FAvaObjectIndex ObjectIndex) const
{
	return FAvaBlueprint_Serialize::ResolveObjectDependency(WorldData, ObjectIndex);
}
