// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaWriter.h"
#include "Data/AvaObjectData.h"

FAvaWriter::FAvaWriter(FAvaWorldData& InWorldData, FAvaObjectData& InObjectData, UObject* InObject)
	: Super(InWorldData, InObjectData, InObject, false)
{
	InObject->Serialize(*this);
	InObjectData.ObjectFlags = InObject->GetFlags();
}
