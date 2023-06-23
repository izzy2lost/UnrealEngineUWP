// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "RuntimePartition.generated.h"

UCLASS(Abstract, CollapseCategories)
class URuntimePartition : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~ Begin UObject Interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.

	struct FCellDesc
	{
		FName Name;
		FBox Bounds = FBox(ForceInit);
		bool bIsSpatiallyLoaded;
		bool bBlockOnSlowStreaming;
		bool bClientOnlyVisible;
		FGuid ContentBundleID;
		int32 Priority;

		/** Optional level value that can be used to filter debug display */
		int32 Level;

		TArray<IStreamingGenerationContext::FActorInstance> ActorInstances;
	};

	virtual bool SupportsHLODs() const PURE_VIRTUAL(URuntimePartition::SupportsHLODs, return false;);
	virtual bool IsValidGrid(FName GridName) const PURE_VIRTUAL(URuntimePartition::IsValidGrid, return false;);
	virtual bool GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs) PURE_VIRTUAL(URuntimePartition::GenerateStreaming, return false;);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName Name;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "!bIsHLODSetup", EditConditionHides, HideEditConditionToggle))
	bool bBlockOnSlowStreaming;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "!bIsHLODSetup", EditConditionHides, HideEditConditionToggle))
	bool bClientOnlyVisible;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "!bIsHLODSetup", EditConditionHides, HideEditConditionToggle))
	int32 Priority;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	int32 LoadingRange;

	UPROPERTY()
	bool bIsHLODSetup;
#endif

protected:
#if WITH_EDITOR
	bool PopulateCellActorInstances(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, bool bIsMainWorldPartition, bool bIsCellAlwaysLoaded, TArray<IStreamingGenerationContext::FActorInstance>& OutCellActorInstances);
#endif
};