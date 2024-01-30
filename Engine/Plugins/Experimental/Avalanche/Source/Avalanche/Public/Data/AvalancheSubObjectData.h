// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheDataDefines.h"
#include "AvalancheObjectData.h"
#include "UObject/SoftObjectPath.h"
#include "AvalancheSubObjectData.generated.h"

USTRUCT()
struct FAvalancheSubObjectData : public FAvalancheObjectData
{
	GENERATED_BODY()

	static FAvalancheSubObjectData MakeSkippedSubObjectData()
	{
		FAvalancheSubObjectData OutData;
		OutData.bWasSkippedClass = true;
		return OutData;
	}

	/** Index to FAvalancheWorldData::SerializedObjectReferences */
	UPROPERTY()
	FAvaObjectIndex OuterIndex;

	UPROPERTY()
	FSoftClassPath Class;

	/** Whether this class was marked as unsupported when the snapshot was taken */
	UPROPERTY()
	bool bWasSkippedClass = false;
};
