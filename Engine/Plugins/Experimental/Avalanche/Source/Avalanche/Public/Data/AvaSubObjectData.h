// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDataDefines.h"
#include "AvaObjectData.h"
#include "UObject/SoftObjectPath.h"
#include "AvaSubObjectData.generated.h"

USTRUCT()
struct FAvaSubObjectData : public FAvaObjectData
{
	GENERATED_BODY()

	static FAvaSubObjectData MakeSkippedSubObjectData()
	{
		FAvaSubObjectData OutData;
		OutData.bWasSkippedClass = true;
		return OutData;
	}

	/** Index to FAvaWorldData::SerializedObjectReferences */
	UPROPERTY()
	FAvaObjectIndex OuterIndex;

	UPROPERTY()
	FSoftClassPath Class;

	/** Whether this class was marked as unsupported when the snapshot was taken */
	UPROPERTY()
	bool bWasSkippedClass = false;
};
