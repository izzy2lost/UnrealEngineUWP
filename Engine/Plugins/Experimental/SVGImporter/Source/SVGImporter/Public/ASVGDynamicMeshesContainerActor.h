// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ASVGDynamicMeshesContainerActor.generated.h"

class UDynamicMeshComponent;

UCLASS(Abstract)
class SVGIMPORTER_API ASVGDynamicMeshesContainerActor : public AActor
{
	GENERATED_BODY()
public:
	virtual TArray<UDynamicMeshComponent*> GetSVGDynamicMeshes()
	PURE_VIRTUAL(ASVGDynamicMeshesContainerActor::GetSVGDynamicMeshes, return {};)
};
