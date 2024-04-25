// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/MathFwd.h"

class UMaterialInterface;

class FDMMaterialShapshotLibrary
{
public:
	static bool SnapshotMaterial(UMaterialInterface* InMaterial, const FIntPoint& InTextureSize, const FString& InSavePath);
};
