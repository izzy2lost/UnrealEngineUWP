// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/HLOD/HLODLayer.h"

#if WITH_EDITOR
UWorldPartitionRuntimeHashSet* UWorldPartitionRuntimeHashSet::CreateFrom(const UWorldPartitionRuntimeHash* SrcHash)
{
	const UWorldPartitionRuntimeSpatialHash* SpatialHash = CastChecked<UWorldPartitionRuntimeSpatialHash>(SrcHash);
	UWorldPartitionRuntimeHashSet* HashSet = NewObject<UWorldPartitionRuntimeHashSet>(SrcHash->GetOuter(), NAME_None, RF_Transactional);

	UWorldPartition* WorldPartition = SrcHash->GetTypedOuter<UWorldPartition>();
	check(WorldPartition);

	UWorldPartition::FGenerateStreamingParams Params = UWorldPartition::FGenerateStreamingParams()
		.SetActorDescContainer(WorldPartition->GetActorDescContainer());

	UWorldPartition::FGenerateStreamingContext Context;
	TUniquePtr<IStreamingGenerationContext> StreamingGenerationContext = WorldPartition->GenerateStreamingGenerationContext(Params, Context);
	if (StreamingGenerationContext.IsValid())
	{
		// Gather all HLOD layers into their corresponding grids
		TMap<FName, TMap<FName, TSet<const UHLODLayer*>>> GridHLODLayersMap;
		const UHLODLayer* DefaultHLODLayer = WorldPartition->GetDefaultHLODLayer();

		StreamingGenerationContext->ForEachActorSetInstance([&GridHLODLayersMap, SpatialHash, DefaultHLODLayer](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
		{
			ActorSetInstance.ForEachActor([&GridHLODLayersMap, SpatialHash, DefaultHLODLayer, &ActorSetInstance](const FGuid& ActorGuid)
			{
				const FWorldPartitionActorDescView& ActorDescView = ActorSetInstance.ContainerInstance->ActorDescViewMap->FindByGuidChecked(ActorGuid);

				if (const UHLODLayer* HLODLayer = ActorDescView.GetHLODLayer().IsValid() ? Cast<UHLODLayer>(ActorDescView.GetHLODLayer().TryLoad()) : DefaultHLODLayer)
				{
					uint32 HLODLIndex = 0;
					while (HLODLayer)
					{
						const FName GridName = ActorDescView.GetRuntimeGrid().IsNone() ? SpatialHash->Grids[0].GridName : ActorDescView.GetRuntimeGrid();
						const FName HLODGridName = HLODLayer->GetRuntimeGrid(HLODLIndex++);
						GridHLODLayersMap.FindOrAdd(GridName).FindOrAdd(HLODGridName).Add(HLODLayer);
						HLODLayer = HLODLayer->GetParentLayer();
					}
				}
			});
		});

		for (const FSpatialHashRuntimeGrid& Grid : SpatialHash->Grids)
		{
			if (!Grid.HLODLayer)
			{
				FRuntimePartitionDesc& RuntimePartitionDesc = HashSet->RuntimePartitions.AddDefaulted_GetRef();
				RuntimePartitionDesc.Class = URuntimePartitionLHGrid::StaticClass();
				RuntimePartitionDesc.Name = Grid.GridName;

				URuntimePartitionLHGrid* LHGrid = NewObject<URuntimePartitionLHGrid>(HashSet, NAME_None);
				LHGrid->Name = RuntimePartitionDesc.Name;
				LHGrid->CellSize = Grid.CellSize;
				LHGrid->bBlockOnSlowStreaming = Grid.bBlockOnSlowStreaming;
				LHGrid->bClientOnlyVisible = Grid.bClientOnlyVisible;
				LHGrid->Priority = Grid.Priority;
				LHGrid->LoadingRange = Grid.LoadingRange;
				LHGrid->HLODIndex = INDEX_NONE;

				RuntimePartitionDesc.MainLayer = LHGrid;

				for (const auto& [HLODGridName, HLODLayers] : GridHLODLayersMap.FindChecked(Grid.GridName))
				{
					const int32 HLODIndex = RuntimePartitionDesc.HLODSetups.Num();
					FRuntimePartitionHLODSetup& HLODSetup = RuntimePartitionDesc.HLODSetups.AddDefaulted_GetRef();

					HLODSetup.Name = HLODGridName.IsNone() ? NAME_PersistentLevel : HLODGridName;
					HLODSetup.bIsSpatiallyLoaded = !HLODGridName.IsNone();
					HLODSetup.HLODLayers = HLODLayers.Array();

					if (HLODSetup.bIsSpatiallyLoaded)
					{
						URuntimePartitionLHGrid* HLODLHGrid = NewObject<URuntimePartitionLHGrid>(HashSet, NAME_None);
						HLODLHGrid->CellSize = HLODSetup.HLODLayers[0]->GetCellSize();
						HLODLHGrid->LoadingRange = HLODSetup.HLODLayers[0]->GetLoadingRange();
						HLODSetup.PartitionLayer = HLODLHGrid;
					}
					else
					{
						HLODSetup.PartitionLayer = NewObject<URuntimePartitionPersistent>(HashSet, NAME_None);;
					}

					HLODSetup.PartitionLayer->Name = HLODSetup.Name;
					HLODSetup.PartitionLayer->bBlockOnSlowStreaming = false;
					HLODSetup.PartitionLayer->bClientOnlyVisible = true;
					HLODSetup.PartitionLayer->Priority = 0;
					HLODSetup.PartitionLayer->LoadingRange = 0;
					HLODSetup.PartitionLayer->HLODIndex = HLODIndex;
				}
			}
		}

		WorldPartition->FlushStreaming();
	}

	return HashSet;
}
#endif