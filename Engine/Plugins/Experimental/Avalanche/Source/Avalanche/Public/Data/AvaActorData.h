// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaComponentData.h"
#include "AvaDataDefines.h"
#include "AvaObjectData.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/SoftObjectPath.h"
#include "AvaActorData.generated.h"

USTRUCT()
struct FAvaActorData : public FAvaObjectData
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString ActorLabel;

	UPROPERTY()
	bool bEditorVisibility = false;
#endif
	
	UPROPERTY()
	FSoftClassPath ActorClass;

	/** Additional Info for Components */
	UPROPERTY()
	TMap<FAvaObjectIndex, FAvaComponentData> ComponentData;

	/** Indices to all Non-Component SubObjects */
	UPROPERTY()
	TArray<FAvaObjectIndex> OwnedSubobjects;
};
