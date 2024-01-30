// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheComponentData.h"
#include "AvalancheDataDefines.h"
#include "AvalancheObjectData.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/SoftObjectPath.h"
#include "AvalancheActorData.generated.h"

USTRUCT()
struct FAvalancheActorData : public FAvalancheObjectData
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
	TMap<FAvaObjectIndex, FAvalancheComponentData> ComponentData;

	/** Indices to all Non-Component SubObjects */
	UPROPERTY()
	TArray<FAvaObjectIndex> OwnedSubobjects;
};
