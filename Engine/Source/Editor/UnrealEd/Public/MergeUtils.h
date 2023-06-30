// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetDefinition.h"
#include "MergeUtils.generated.h"

namespace MergeUtils
{
	UNREALED_API EAssetCommandResult Merge(const FAssetAutomaticMergeArgs& MergeArgs);
	UNREALED_API EAssetCommandResult Merge(const FAssetManualMergeArgs& MergeArgs);
}

UCLASS()
class UUndoableResolveHandler : public UObject
{
public:
	GENERATED_BODY()
	void SetManagedObject(UObject* Object);
	void MarkResolved();

	virtual void PostEditUndo() override;

private:
	FString BaseRevisionNumber;
	FString CurrentRevisionNumber;
	FString BackupFilepath;
	TWeakObjectPtr<UObject> ManagedObject;
	TSharedPtr<class ISourceControlChangelist> CheckinIdentifier;
	
	UPROPERTY()
	bool bShouldBeResolved = false;
};