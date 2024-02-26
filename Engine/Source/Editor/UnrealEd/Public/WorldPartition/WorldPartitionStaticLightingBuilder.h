// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Misc/EnumClassFlags.h"
#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartition/WorldPartitionBuilderHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h"

#include "WorldPartitionStaticLightingBuilder.generated.h"


class AMapBuildDataActor;

enum class EWPStaticLightingBuildStep : uint8
{
	None		= 0,
	WPSL_Build = 1 << 1,						// Build the static lighting by iterating over the map and associates the data actors with the map actors already present	
	WPSL_Submit = 1 <<2,						// Optionally, submit results to source control
};
ENUM_CLASS_FLAGS(EWPStaticLightingBuildStep);

UCLASS()
class UWorldPartitionStaticLightingBuilder : public UWorldPartitionBuilder
{
	GENERATED_UCLASS_BODY()
public:
	// UWorldPartitionBuilder interface begin
	virtual bool RequiresCommandletRendering() const override;
	virtual ELoadingMode GetLoadingMode() const override;
	virtual bool PreWorldInitialization(UWorld* World, FPackageSourceControlHelper& PackageHelper) override;

protected:
	virtual bool PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;

	// UWorldPartitionBuilder interface end

	bool ValidateParams() const;
	bool ShouldRunStep(const EWPStaticLightingBuildStep BuildStep) const;

	bool RunForVLM(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper);

	bool Submit(UWorld* World, FPackageSourceControlHelper& PackageHelper);
	

private:
	class UWorldPartition* WorldPartition;
	TUniquePtr<FSourceControlHelper> SourceControlHelper;
	 
	// Options --
	EWPStaticLightingBuildStep BuildOptions;	
	bool bBuildVLMOnly;
	ELightingBuildQuality QualityLevel;

	FBuilderModifiedFiles ModifiedFiles;
};
