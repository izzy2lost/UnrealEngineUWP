// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CEClonerEffectorSettings.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

UCEClonerEffectorSettings::UCEClonerEffectorSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName = TEXT("Cloner & Effector");

	DefaultStaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(DefaultStaticMeshPath));
	DefaultMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultMaterialPath));
}
