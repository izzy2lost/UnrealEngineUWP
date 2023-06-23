// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionLevelStreaming.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
bool URuntimePartitionLevelStreaming::SupportsHLODs() const
{
	return false;
}

bool URuntimePartitionLevelStreaming::IsValidGrid(FName GridName) const
{
	const TArray<FName> GridNameList = UWorldPartitionRuntimeHashSet::ParseGridName(GridName);
	return GridNameList.Num() && (GridNameList.Num() <= 2);
}

bool URuntimePartitionLevelStreaming::GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	TArray<IStreamingGenerationContext::FActorInstance> CellActorInstances;
	if (PopulateCellActorInstances(ActorSetInstances, bIsMainWorldPartition, false, CellActorInstances))
	{
		TMap<FName, TArray<IStreamingGenerationContext::FActorInstance>> SubLevelsActorInstances;
		for (const IStreamingGenerationContext::FActorInstance& ActorInstance : CellActorInstances)
		{
			TArray<FName> ActorSetGridNameList;
					
			if (!ActorInstance.ActorSetInstance->RuntimeGrid.IsNone())
			{
				ActorSetGridNameList = UWorldPartitionRuntimeHashSet::ParseGridName(ActorInstance.ActorSetInstance->RuntimeGrid);
			}
			else
			{
				ActorSetGridNameList.Add(NAME_Default);
			}

			if (bOneLevelPerActorContainer && !ActorInstance.GetContainerID().IsMainContainer())
			{
				ActorSetGridNameList.Add(*ActorInstance.GetContainerID().ToString());
			}

			TStringBuilder<512> StringBuilder;
			for (FName GridName : ActorSetGridNameList)
			{
				StringBuilder += GridName.ToString();
				StringBuilder += TEXT("_");
			}
			StringBuilder.RemoveSuffix(1);

			FName SubLevelName = *StringBuilder;

			SubLevelsActorInstances.FindOrAdd(SubLevelName).Add(ActorInstance);
		}

		for (auto& [SubLevelName, SubLevelActorSetInstances] : SubLevelsActorInstances)
		{
			FCellDesc& CellDesc = OutRuntimeCellDescs.Emplace_GetRef();

			CellDesc.Name = SubLevelName;
			CellDesc.bIsSpatiallyLoaded = true;
			CellDesc.ContentBundleID = SubLevelActorSetInstances[0].ActorSetInstance->ContentBundleID;
			CellDesc.bBlockOnSlowStreaming = bBlockOnSlowStreaming;
			CellDesc.bClientOnlyVisible = bClientOnlyVisible;
			CellDesc.Priority = Priority;
			CellDesc.Level = 0;
			CellDesc.ActorInstances = SubLevelActorSetInstances;

			for (const IStreamingGenerationContext::FActorInstance& ActorInstance : CellDesc.ActorInstances)
			{
				const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();
				const FBox RuntimeBounds = ActorDescView.GetRuntimeBounds();
				if (RuntimeBounds.IsValid)
				{
					CellDesc.Bounds += RuntimeBounds.TransformBy(ActorInstance.GetTransform());
				}
			}
		}
	}

	return true;
}
#endif