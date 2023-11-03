// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"

#include "PCGWorldPartitionBuilder.generated.h"

class UWorld;

/**
* Builder that triggers generation on PCG components.
* 
* Example command line:
*   ProjectName MapName -Unattended -AllowCommandletRendering -run=WorldPartitionBuilderCommandlet -Builder=PCGWorldPartitionBuilder  -IncludeGraphNames=PCG_GraphA;PCG_GraphB
*/
UCLASS()
class UPCGWorldPartitionBuilder : public UWorldPartitionBuilder
{
	GENERATED_UCLASS_BODY()

public:
	// Whether to require initialization of rendering, for now default to true. Requires a GPU present.
	// Get Texture Data benefits from rendering - means the GPU will be available as a fallback sampling method so
	// that the "CPU sampling" option does not have to be enabled on the texture. This may expand to other nodes if
	// we use GPU resources or compute more in the future.
	virtual bool RequiresCommandletRendering() const override { return true; }

	// In the future we may load a world in stages, but for now load everything to make sure we have the complete level.
	virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::EntireWorld; }

protected:
	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool PostRun(UWorld* World, FPackageSourceControlHelper& PackageHelper, const bool bInRunSuccess) override;
	virtual bool CanProcessNonPartitionedWorlds() const { return true; }

private:
	/** The packages dirtied while generating all components. */
	UPROPERTY(Transient)
	TMap<FGuid, UPackage*> PendingDirtyPackages;

	/** Include components which have editing mode set to Normal. */
	bool bGenerateEditingModeNormalComponents = false;

	/** If non empty, components with each graph name will be generated (if editing mode is included), in order. */
	TArray<FName> IncludeGraphNames;

	/** Call generate on each component and wait until completion and any async processes before generating the next. */
	bool bOneComponentAtATime = false;

	/** If non empty, only components on actors with given ID(s) will be generated. */
	TArray<FString> IncludeActorIDs;
};
