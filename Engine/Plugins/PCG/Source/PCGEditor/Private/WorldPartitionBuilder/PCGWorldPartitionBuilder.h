// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"

#include "EditorBuildUtils.h"

#include "PCGWorldPartitionBuilder.generated.h"

class UWorld;
class UPCGBuilderSettings;
class UPCGGraphInterface;

struct FPCGWorldPartitionCommandlineArgs
{
	/** Include components which have editing mode set to LoadAsPreview. */
	TOptional<bool> bGenerateEditingModeLoadAsPreviewComponents;

	/** Include components which have editing mode set to Normal. */
	TOptional<bool> bGenerateEditingModeNormalComponents;

	/** Include components which have editing mode set to Preview. */
	TOptional<bool> bGenerateEditingModePreviewComponents;
		
	/** If non empty, components with each graph name will be generated (if editing mode is included), in order. */
	TOptional<TArray<FString>> IncludeGraphNames;

	/** Call generate on each component and wait until completion and any async processes before generating the next. */
	TOptional<bool> bOneComponentAtATime;

	/** If non empty, only components on actors with given ID(s) will be generated. */
	TOptional<TArray<FString>> IncludeActorIDs;

	/** Submit dirty files even if errors occurred during generation. */
	TOptional<bool> bIgnoreGenerationErrors;
		
	/** Path to builder settings asset */
	TOptional<TSoftObjectPtr<UPCGBuilderSettings>> SettingAsset;
};

struct FPCGWorldPartitionBuilderArgs
{
	/** Include components which have editing mode set to LoadAsPreview. */
	bool bGenerateEditingModeLoadAsPreviewComponents = true;

	/** Include components which have editing mode set to Normal. */
	bool bGenerateEditingModeNormalComponents = false;

	/** Include components which have editing mode set to Preview. */
	bool bGenerateEditingModePreviewComponents = false;

	/** If non empty, components with each graph name will be generated (if editing mode is included), in order. */
	TArray<FString> IncludeGraphNames;

	/** If non empty, components with each graph asset will be generated (if editing mode is included), in order */
	TArray<TSoftObjectPtr<UPCGGraphInterface>> IncludeGraphs;

	/** Call generate on each component and wait until completion and any async processes before generating the next. */
	bool bOneComponentAtATime = false;

	/** If non empty, only components on actors with given ID(s) will be generated. */
	TArray<FString> IncludeActorIDs;

	/** Submit dirty files even if errors occurred during generation. */
	bool bIgnoreGenerationErrors = false;

	static FPCGWorldPartitionBuilderArgs InitializeFrom(const FPCGWorldPartitionCommandlineArgs& CommandlineArgs);
};

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
	virtual bool PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool CanProcessNonPartitionedWorlds() const override { return true; }

	/** Save all the pending dirty and deleted packages. */
	virtual bool SaveDirtyPackages(UWorld* World, FPackageSourceControlHelper& PackageHelper);

	friend class FPCGEditorModule;
	static bool CanBuild(const UWorld* InWorld, FName InBuildOption);
	static EEditorBuildResult Build(UWorld* InWorld, FName InBuildOption);
private:
	/** The packages dirtied while generating all components. */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UPackage>> PendingDirtyPackages;

	/** Packages that were logged as deleted through the OnActorDeleted event. UPROP to prevent GC. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPackage>> DeletedActorPackages;

	FPCGWorldPartitionCommandlineArgs CommandlineArgs;
	FPCGWorldPartitionBuilderArgs Args;

	/** Flag to register if any error occurred during generation. */
	bool bErrorOccurredWhileGenerating = false;
};
