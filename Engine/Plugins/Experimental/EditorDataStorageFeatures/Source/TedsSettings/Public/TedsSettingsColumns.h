// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TedsSettingsColumns.generated.h"

USTRUCT(meta = (DisplayName = "Settings Container"))
struct FSettingsContainerColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName ContainerName;
};

USTRUCT(meta = (DisplayName = "Settings Category"))
struct FSettingsCategoryColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName CategoryName;
};

USTRUCT(meta = (DisplayName = "Settings Section"))
struct FSettingsSectionColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (Searchable))
	FName SectionName;
};

USTRUCT(meta = (DisplayName = "Settings"))
struct FSettingsTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};
