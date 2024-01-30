// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheWriter.h"
#include "Data/AvalancheObjectData.h"

FAvalancheWriter::FAvalancheWriter(FAvalancheWorldData& InWorldData, FAvalancheObjectData& InObjectData, UObject* InObject)
	: Super(InWorldData, InObjectData, InObject, false)
{
	InObject->Serialize(*this);
	InObjectData.ObjectFlags = InObject->GetFlags();
}
