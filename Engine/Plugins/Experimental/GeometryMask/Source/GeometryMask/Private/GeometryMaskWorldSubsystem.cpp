// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskWorldSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GeometryMaskSVE.h"
#include "SceneViewExtension.h"

void UGeometryMaskWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();

	GeometryMaskSceneViewExtension = FSceneViewExtensions::NewExtension<FGeometryMaskSceneViewExtension>(World);
}
