// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TedsSettingsColumns.generated.h"

USTRUCT(meta = (DisplayName = "Settings Container"))
struct FSettingsContainerColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	FName ContainerName;
};

USTRUCT(meta = (DisplayName = "Settings Category"))
struct FSettingsCategoryColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	FName CategoryName;
};

USTRUCT(meta = (DisplayName = "Settings Section"))
struct FSettingsSectionColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	FName SectionName;
};
