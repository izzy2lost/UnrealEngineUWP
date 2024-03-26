// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterModularFeatureMediaInitializer.h"


/**
 * ShareMemory media source/output initializer for nDisplay
 */
class FSharedMemoryMediaInitializerFeature
	: public IDisplayClusterModularFeatureMediaInitializer
{
public:

	//~ Begin IDisplayClusterModularFeatureMediaInitializer
	virtual bool IsMediaSubjectSupported(const UObject* MediaSubject) override;
	virtual void InitializeMediaSubjectForTile(UObject* MediaSubject, const FString& OwnerName, uint8 OwnerUniqueIdx, const FIntPoint& TilePos) override;
	//~ End IDisplayClusterModularFeatureMediaInitializer

private:

	/** Generates unique name for SMM subjects */
	FString GenerateUniqueName(const FString& OwnerName, const FIntPoint& TilePos);
};
