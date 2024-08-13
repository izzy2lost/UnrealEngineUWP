// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeInfoMap.h"
#include "Engine/World.h"
#include "LandscapeInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeInfoMap)

ULandscapeInfoMap::ULandscapeInfoMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

ULandscapeInfoMap& ULandscapeInfoMap::GetLandscapeInfoMap(const UWorld* World)
{
	ULandscapeInfoMap *FoundObject = nullptr;
	World->PerModuleDataObjects.FindItemByClass(&FoundObject);
	checkf(FoundObject, TEXT("ULandscapInfoMap object was not created for this UWorld."));
	return *FoundObject;
}

ULandscapeInfoMap* ULandscapeInfoMap::FindLandscapeInfoMap(const UWorld* World)
{
	ULandscapeInfoMap* FoundObject = nullptr;
	World->PerModuleDataObjects.FindItemByClass(&FoundObject);
	return FoundObject;
}

