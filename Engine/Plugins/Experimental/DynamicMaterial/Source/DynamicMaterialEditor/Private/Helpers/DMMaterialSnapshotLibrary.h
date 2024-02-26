// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "Containers/UnrealString.h"

class UMaterialInterface;

class FDMMaterialShapshotLibrary
{
public:
	static bool SnapshotMaterial(UMaterialInterface* InMaterial, const FIntPoint& InTextureSize, const FString& InSavePath);
};
