// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "ImportFilePath.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS  // For bForceReimport
USTRUCT()
struct FChaosClothAssetImportFilePath
{
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Import File Path")
	FString FilePath;

	UE_DEPRECATED(5.5, "Use delegate instead.")
	UPROPERTY()
	bool bForceReimport = false;

	FChaosClothAssetImportFilePath() = default;

	explicit FChaosClothAssetImportFilePath(FSimpleDelegate&& InDelegate) : Delegate(MoveTemp(InDelegate)) {}

	void Execute() const
	{
		Delegate.ExecuteIfBound();
	}

private:
	FSimpleDelegate Delegate;
};
