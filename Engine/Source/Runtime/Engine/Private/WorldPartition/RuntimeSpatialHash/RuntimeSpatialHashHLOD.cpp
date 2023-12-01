// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"

#if WITH_EDITOR
#include "HAL/PlatformFile.h"
#include "WorldPartition/RuntimeSpatialHash/RuntimeSpatialHashGridHelper.h"

#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODModifier.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/ContentBundle/ContentBundleActivationScope.h"

#include "UObject/GCObjectScopeGuard.h"
#include "UObject/SavePackage.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionRuntimeSpatialHashHLOD, Log, All);

static void SavePackage(UPackage* Package, ISourceControlHelper* SourceControlHelper)
{
	if (SourceControlHelper)
	{
		SourceControlHelper->Save(Package);
	}
	else
	{
		Package->MarkAsFullyLoaded();

		FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(Package->GetName());
		const FString PackageFileName = PackagePath.GetLocalFullPath();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
		{
			UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Error, TEXT("Error saving package %s."), *Package->GetName());
			check(0);
		}
	}
}

static void DeletePackage(const FString& PackageName, ISourceControlHelper* SourceControlHelper)
{
	FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(PackageName);
	const FString PackageFileName = PackagePath.GetLocalFullPath();

	if (SourceControlHelper)
	{
		SourceControlHelper->Delete(PackageFileName);
	}
	else
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PackageFileName);
	}
}

static void DeletePackage(UPackage* Package, ISourceControlHelper* SourceControlHelper)
{
	if (SourceControlHelper)
	{
		SourceControlHelper->Delete(Package);
	}
	else
	{
		DeletePackage(Package->GetName(), SourceControlHelper);
	}
}

static void DeletePackage(UWorldPartition* WorldPartition, FWorldPartitionActorDesc* ActorDesc, ISourceControlHelper* SourceControlHelper)
{
	if (ActorDesc->IsLoaded())
	{
		DeletePackage(ActorDesc->GetActor()->GetPackage(), SourceControlHelper);
		WorldPartition->OnPackageDeleted(ActorDesc->GetActor()->GetPackage());
	}
	else
	{
		DeletePackage(ActorDesc->GetActorPackage().ToString(), SourceControlHelper);
		WorldPartition->RemoveActor(ActorDesc->GetGuid());
	}
}

static void GameTick(UWorld* InWorld)
{
	static int32 TickRendering = 0;
	static const int32 FlushRenderingFrequency = 256;

	// Perform a GC when memory usage exceeds a given threshold
	if (FWorldPartitionHelpers::ShouldCollectGarbage())
	{
		FWorldPartitionHelpers::DoCollectGarbage();
	}

	// When running with -AllowCommandletRendering we want to flush
	if (((++TickRendering % FlushRenderingFrequency) == 0) && IsAllowCommandletRendering())
	{
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	}
}

static TArray<FGuid> GenerateHLODActorsForGrid(UWorldPartition* WorldPartition, const IStreamingGenerationContext* StreamingGenerationContext, const UWorldPartition::FSetupHLODActorsParams& Params, const FSpatialHashRuntimeGrid& RuntimeGrid, uint32 HLODLevel, FHLODCreationContext& Context, ISourceControlHelper* SourceControlHelper, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FName>& NewActors, TArray<FName>& DirtyActors)
{
	const IStreamingGenerationContext::FActorSetContainer* MainActorSetContainer = StreamingGenerationContext->GetMainWorldContainer();
	const FBox WorldBounds = StreamingGenerationContext->GetWorldBounds();

	const FSquare2DGridHelper PartitionedActors = GetPartitionedActors(WorldBounds, RuntimeGrid, ActorSetInstances);
	const FSquare2DGridHelper::FGridLevel::FGridCell& AlwaysLoadedCell = PartitionedActors.GetAlwaysLoadedCell();

	auto ShouldGenerateHLODActors = [&AlwaysLoadedCell](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell, const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk)
	{
		const bool bIsCellAlwaysLoaded = &GridCell == &AlwaysLoadedCell;
		const bool bChunkHasActors = !GridCellDataChunk.GetActorSetInstances().IsEmpty();

		const bool bShouldGenerateHLODActors = !bIsCellAlwaysLoaded && bChunkHasActors;
		return bShouldGenerateHLODActors;
	};

	UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();

	// Quick pass to compute the number of cells we'll have to process, to provide a meaningful progress display
	int32 NbCellsToProcess = 0;
	PartitionedActors.ForEachCells([&](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell)
	{
		for (const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk : GridCell.GetDataChunks())
		{
			const bool bShouldGenerateHLODActors = ShouldGenerateHLODActors(GridCell, GridCellDataChunk);
			if (bShouldGenerateHLODActors)
			{
				NbCellsToProcess++;
			}
		}
	});

	UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Verbose, TEXT("Building HLODs for grid %s..."), *RuntimeGrid.GridName.ToString());

	FScopedSlowTask SlowTask(NbCellsToProcess, FText::FromString(FString::Printf(TEXT("Building HLODs for grid %s..."), *RuntimeGrid.GridName.ToString())));
	SlowTask.MakeDialog();

	TArray<FGuid> GridHLODActors;
	PartitionedActors.ForEachCells([&](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell)
	{
		const FGridCellCoord CellCoord = GridCell.GetCoords();

		FBox2D CellBounds2D;
		PartitionedActors.GetCellBounds(CellCoord, CellBounds2D);

		FGridCellCoord CellGlobalCoord;
		verify(PartitionedActors.GetCellGlobalCoords(CellCoord, CellGlobalCoord));

		for (const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk : GridCell.GetDataChunks())
		{
			// Fake tick
			GameTick(WorldPartition->GetWorld());

			const bool bShouldGenerateHLODActors = ShouldGenerateHLODActors(GridCell, GridCellDataChunk);
			if (bShouldGenerateHLODActors)
			{
				SlowTask.EnterProgressFrame(1);

				const FGuid CellGuid = UWorldPartitionRuntimeSpatialHash::GetCellGuid(RuntimeGrid.GridName, RuntimeGrid.CellSize, CellGlobalCoord, GridCellDataChunk.GetDataLayersID(), GridCellDataChunk.GetContentBundleID());
				const FString CellName = UWorldPartitionRuntimeSpatialHash::GetCellNameString(OuterWorld, RuntimeGrid.GridName, CellGlobalCoord, GridCellDataChunk.GetDataLayersID(), GridCellDataChunk.GetContentBundleID());

				UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Verbose, TEXT("Creating HLOD for cell %s at %s..."), *CellName, *CellCoord.ToString());

				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*CellCoord.ToString());

				UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Display, TEXT("[%d / %d] Processing cell %s..."), (int32)SlowTask.CompletedWork + 1, (int32)SlowTask.TotalAmountOfWork, *CellName);

				// We know the cell's 2D bound but must figure out the bounds in Z
				FBox CellBounds = FBox(FVector(CellBounds2D.Min, MAX_dbl), FVector(CellBounds2D.Max, -MAX_dbl));

				TArray<IStreamingGenerationContext::FActorInstance> ActorInstances;
				for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : GridCellDataChunk.GetActorSetInstances())
				{
					ActorSetInstance->ForEachActor([&ActorInstances, ActorSetInstance, &CellBounds](const FGuid& ActorGuid)
					{
						const IStreamingGenerationContext::FActorInstance& ActorInstance = ActorInstances.Emplace_GetRef(ActorGuid, ActorSetInstance);
						const FBox RuntimeBounds = ActorInstance.GetActorDescView().GetRuntimeBounds();
						check(RuntimeBounds.IsValid);
						CellBounds.Min.Z = FMath::Min(CellBounds.Min.Z, RuntimeBounds.Min.Z);
						CellBounds.Max.Z = FMath::Max(CellBounds.Max.Z, RuntimeBounds.Max.Z);
					});
				}

				// Ensure the Z bounds are valid
				check(CellBounds.Min.Z <= CellBounds.Max.Z);

				FHLODCreationParams CreationParams;
				CreationParams.WorldPartition = WorldPartition;
				CreationParams.CellGuid = CellGuid;
				CreationParams.CellName = CellName;
				CreationParams.CellBounds = CellBounds;
				CreationParams.GetRuntimeGrid = [HLODLevel](const UHLODLayer* HLODLayer) { return HLODLayer->GetRuntimeGrid(HLODLevel); };
				CreationParams.HLODLevel = HLODLevel;
				CreationParams.MinVisibleDistance = RuntimeGrid.LoadingRange;
				CreationParams.ContentBundleGuid = GridCellDataChunk.GetContentBundleID();
				CreationParams.DataLayerInstances = GridCellDataChunk.GetDataLayers();

				IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
				TArray<AWorldPartitionHLOD*> CellHLODActors = WPHLODUtilities->CreateHLODActors(Context, CreationParams, ActorInstances);
				if (!CellHLODActors.IsEmpty())
				{
					TArray<AWorldPartitionHLOD*> NewCellHLODActors;

					for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
					{
						FGuid ActorGuid = CellHLODActor->GetActorGuid();
						GridHLODActors.Add(ActorGuid);

						UPackage* CellHLODActorPackage = CellHLODActor->GetPackage();
						if (CellHLODActorPackage->HasAnyPackageFlags(PKG_NewlyCreated))
						{
							NewCellHLODActors.Add(CellHLODActor);
							NewActors.Add(CellHLODActor->GetFName());
						}
						else if (CellHLODActorPackage->IsDirty())
						{
							DirtyActors.Add(CellHLODActor->GetFName());
						}
					}

					if (!Params.bReportOnly)
					{
						for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
						{
							if (CellHLODActor->GetPackage()->IsDirty())
							{
								SavePackage(CellHLODActor->GetPackage(), SourceControlHelper);
							}
						}
					}

					// Update newly created HLOD actors
					for (AWorldPartitionHLOD* CellHLODActor : NewCellHLODActors)
					{
						Context.ActorReferences.Emplace(WorldPartition, CellHLODActor->GetActorGuid());
					}
				}

				// Unload actors
				Context.ActorReferences.Empty();
			}
		}
	});

	// Ensure all async packages writes are completed before we start processing another HLOD level or grid
	UPackage::WaitForAsyncFileWrites();

	// Need to collect garbage here since some HLOD actors have been marked pending kill when destroying them
	// and they may be loaded when generating the next HLOD layer.
	FWorldPartitionHelpers::DoCollectGarbage();

	return GridHLODActors;
}

// Find all referenced HLODLayer assets
static TMap<UHLODLayer*, int32> GatherHLODLayers(const IStreamingGenerationContext* StreamingGenerationContext, const UWorldPartition* WorldPartition)
{
	// Gather up all HLODLayers referenced by the actors, along with the HLOD level at which it was used
	TMap<UHLODLayer*, int32> HLODLayersLevel;

	StreamingGenerationContext->ForEachActorSetContainer([&HLODLayersLevel, WorldPartition](const IStreamingGenerationContext::FActorSetContainer& ActorSetContainer)
	{
		const FStreamingGenerationActorDescViewMap* ActorDescViewMap = ActorSetContainer.ActorDescViewMap;
		ActorDescViewMap->ForEachActorDescView([&HLODLayersLevel, WorldPartition](const FStreamingGenerationActorDescView& ActorDescView)
		{
			if (!ActorDescView.GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>())
			{
				if (ActorDescView.GetActorIsHLODRelevant())
				{
					UHLODLayer* HLODLayer = Cast<UHLODLayer>(ActorDescView.GetHLODLayer().TryLoad());
		
					// If layer was already encountered, no need to process it again
					if (!HLODLayersLevel.Contains(HLODLayer))
					{
						// Walk up the parent HLOD layers, keep track of HLOD level
						int32 CurrentHLODLevel = 0;
						while (HLODLayer != nullptr)
						{
							int32& HLODLevel = HLODLayersLevel.FindOrAdd(HLODLayer);
							HLODLevel = FMath::Max(HLODLevel, CurrentHLODLevel);
		
							HLODLayer = HLODLayer->GetParentLayer();
							CurrentHLODLevel++;
						}
					}
				}
			}
		});
	});

	return HLODLayersLevel;
}

static TMap<FName, FSpatialHashRuntimeGrid> CreateHLODGrids(TMap<UHLODLayer*, int32>& InHLODLayersLevel)
{
	TMap<FName, FSpatialHashRuntimeGrid> HLODGrids;

	// Create HLOD runtime grids
	for (const TPair<UHLODLayer*, int32>& HLODLayerLevel : InHLODLayersLevel)
	{
		UHLODLayer* HLODLayer = HLODLayerLevel.Key;

		// No need to create a runtime grid if the HLOD layer is set to be non spatially loaded
		if (HLODLayer->IsSpatiallyLoaded())
		{
			int32 HLODLevel = HLODLayerLevel.Value;
			FName GridName = HLODLayer->GetRuntimeGrid(HLODLevel);

			FSpatialHashRuntimeGrid& HLODGrid = HLODGrids.FindOrAdd(GridName);
			if (HLODGrid.GridName.IsNone())
			{
				HLODGrid.CellSize = HLODLayer->GetCellSize();
				HLODGrid.LoadingRange = HLODLayer->GetLoadingRange();
				HLODGrid.GridName = GridName;
				HLODGrid.HLODLayer = HLODLayer;
				HLODGrid.bClientOnlyVisible = true;

				// HLOD grid have a lower priority than standard grids
				HLODGrid.Priority = 100 + HLODLevel;

				// Setup grid debug color to match HLODColorationColors
				const int32 LastLODColorationColorIdx = GEngine->HLODColorationColors.Num() - 1;
				const int32 HLODColorIdx = FMath::Clamp(HLODLevel + 2, 0U, (int32)LastLODColorationColorIdx);
				HLODGrid.DebugColor = GEngine->HLODColorationColors[HLODColorIdx];
			}

			// Temp solution to have replication working for hlod layers that have a modifier
			// Modifiers are used for example for destructible HLODs, that need to have a server side component
			// The real solution would probably be to check if we have any actors that are
			// returning true for NeedsLoadForServer()
			HLODGrid.bClientOnlyVisible &= HLODLayer->GetHLODModifierClass() == nullptr;
		}
	}

	HLODGrids.KeySort(FNameLexicalLess());

	return HLODGrids;
}

// Create/destroy HLOD grid actors
static void UpdateHLODGridsActors(UWorld* World, const UActorDescContainer* Container, const TMap<FName, FSpatialHashRuntimeGrid>& HLODGrids, ISourceControlHelper* SourceControlHelper)
{
	static const FName HLODGridTag = TEXT("HLOD");
	static const uint32 HLODGridTagLen = HLODGridTag.ToString().Len();

	// Gather all existing HLOD grid actors, see if some are unused and needs to be deleted
	TMap<FName, ASpatialHashRuntimeGridInfo*> ExistingGridActors;
	for (FActorDescList::TConstIterator<> ItActorDesc(Container); ItActorDesc; ++ItActorDesc)
	{
		if(ItActorDesc->GetActorNativeClass()->IsChildOf<ASpatialHashRuntimeGridInfo>())
		{
			const FWorldPartitionActorDesc* GridActorDesc = *ItActorDesc;
			ASpatialHashRuntimeGridInfo* GridActor = CastChecked<ASpatialHashRuntimeGridInfo>(GridActorDesc->GetActor());
			if (GridActor->ActorHasTag(HLODGridTag))
			{
				const FSpatialHashRuntimeGrid* HLODGrid = HLODGrids.Find(GridActor->GridSettings.GridName);
				if (HLODGrid && GridActor->GridSettings.Priority && GridActor->GridSettings.HLODLayer)
				{
					check(GridActor->GetContentBundleGuid() == Container->GetContentBundleGuid());

					ExistingGridActors.Emplace(GridActor->GridSettings.GridName, GridActor);
				}
				else
				{
					World->DestroyActor(GridActor);

					DeletePackage(GridActor->GetPackage(), SourceControlHelper);
				}
			}
		}
	}

	// Create missing HLOD grid actors or update existing actors
	for (const auto& HLODGridEntry : HLODGrids)
	{
		FName GridName = HLODGridEntry.Key;
		const FSpatialHashRuntimeGrid& GridSettings = HLODGridEntry.Value;

		bool bDirty = false;

		ASpatialHashRuntimeGridInfo* GridActor = ExistingGridActors.FindRef(GridName);
		if (!GridActor)
		{
			FContentBundleActivationScope ContentBndleScope(Container->GetContentBundleGuid());

			FActorSpawnParameters SpawnParams;
			SpawnParams.bCreateActorPackage = true;
			GridActor = World->SpawnActor<ASpatialHashRuntimeGridInfo>(SpawnParams);
			GridActor->Tags.Add(HLODGridTag);
			bDirty = true;

			check(GridActor->GetContentBundleGuid() == Container->GetContentBundleGuid());
		}

		const FString ActorLabel = GridSettings.GridName.ToString();
		if (GridActor->GetActorLabel() != ActorLabel)
		{
			GridActor->SetActorLabel(ActorLabel);
			bDirty = true;
		}

		if (GridActor->GridSettings != GridSettings)
		{
			GridActor->GridSettings = GridSettings;
			bDirty = true;
		}			

		if (bDirty)
		{
			SavePackage(GridActor->GetPackage(), SourceControlHelper);
		}
	}
}

bool UWorldPartitionRuntimeSpatialHash::SetupHLODActors(const IStreamingGenerationContext* StreamingGenerationContext, const UWorldPartition::FSetupHLODActorsParams& Params) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionRuntimeSpatialHash::SetupHLODActors);

	ISourceControlHelper* SourceControlHelper = Params.SourceControlHelper;

	TArray<FName> NewActors;
	TArray<FName> DirtyActors;
	TArray<FName> DeletedActors;

	if (Params.bReportOnly)
	{
		LogWorldPartitionRuntimeSpatialHashHLOD.SetVerbosity(ELogVerbosity::Verbose);
	}

	if (!Grids.Num())
	{
		UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Error, TEXT("Invalid partition grids setup"));
		return false;
	}

	UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	IStreamingGenerationContext::FActorSetContainer* MainActorSetContainer = const_cast<IStreamingGenerationContext::FActorSetContainer*>(StreamingGenerationContext->GetMainWorldContainer());
	const FStreamingGenerationActorDescCollection* MainContainerCollection = MainActorSetContainer->ActorDescCollection;

	// Find all used HLOD layers
	TMap<UHLODLayer*, int32> HLODLayersLevels = GatherHLODLayers(StreamingGenerationContext, WorldPartition);
	TArray<UHLODLayer*> HLODLayers;
	HLODLayersLevels.GetKeys(HLODLayers);

	// Keep references to HLODLayers or they may be GC'd
	TGCObjectsScopeGuard<UHLODLayer> KeepHLODLayersAlive(HLODLayers);

	TMap<FName, FSpatialHashRuntimeGrid> HLODGrids = CreateHLODGrids(HLODLayersLevels);
	
	TMap<FName, int32> GridsMapping;
	GridsMapping.Add(NAME_None, 0);
	for (int32 i = 0; i < Grids.Num(); i++)
	{
		const FSpatialHashRuntimeGrid& Grid = Grids[i];
		check(!GridsMapping.Contains(Grid.GridName));
		GridsMapping.Add(Grid.GridName, i);
	}

	// HLOD creation context
	FHLODCreationContext Context;
	for (FActorDescList::TConstIterator<AWorldPartitionHLOD> HLODIterator(MainContainerCollection->GetMainActorDescContainer()); HLODIterator; ++HLODIterator)
	{
		FWorldPartitionHandle HLODActorHandle(WorldPartition, HLODIterator->GetGuid());
		Context.HLODActorDescs.Emplace(HLODIterator->GetActorName(), MoveTemp(HLODActorHandle));
	}

	TArray<TArray<const IStreamingGenerationContext::FActorSetInstance*>> GridActorSetInstances;
	GridActorSetInstances.InsertDefaulted(0, Grids.Num());

	StreamingGenerationContext->ForEachActorSetInstance([&GridsMapping, &GridActorSetInstances](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
	{
		int32* FoundIndex = GridsMapping.Find(ActorSetInstance.RuntimeGrid);
		if (!FoundIndex)
		{
			//@todo_ow should this be done upstream?
			UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Error, TEXT("Invalid partition grid '%s' referenced by actor cluster"), *ActorSetInstance.RuntimeGrid.ToString());
		}

		int32 GridIndex = FoundIndex ? *FoundIndex : 0;
		GridActorSetInstances[GridIndex].Add(&ActorSetInstance);
	});

	// Keep track of all valid HLOD actors, along with which runtime grid they live in
	TMap<FName, TArray<FGuid>> GridsHLODActors;
	TArray<TUniquePtr<FStreamingGenerationActorDescView>> ActorDescViews;

	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();

	auto GenerateHLODActors = [&GridsHLODActors, &ActorDescViews, &MainActorSetContainer, StreamingGenerationContext, &Params, WorldPartition, DataLayerManager, &Context, SourceControlHelper, &NewActors, &DirtyActors]
	(const FSpatialHashRuntimeGrid& RuntimeGrid, uint32 HLODLevel, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances)
	{
		// Generate HLODs for this grid
		TArray<FGuid> HLODActors = GenerateHLODActorsForGrid(WorldPartition, StreamingGenerationContext, Params, RuntimeGrid, HLODLevel, Context, SourceControlHelper, ActorSetInstances, NewActors, DirtyActors);

		for (const FGuid& HLODActorGuid : HLODActors)
		{
			if (!Params.bReportOnly)
			{
				FWorldPartitionActorDesc* HLODActorDesc = WorldPartition->GetActorDesc(HLODActorGuid);
				check(HLODActorDesc);
				FStreamingGenerationActorDescView* ActorDescView = ActorDescViews.Emplace_GetRef(MakeUnique<FStreamingGenerationActorDescView>(HLODActorDesc)).Get();
				
				TArray<const FWorldPartitionActorDescView*> WorldDataLayerViews;
				Algo::Transform(MainActorSetContainer->ActorDescViewMap->FindByExactNativeClass<AWorldDataLayers>(), WorldDataLayerViews, [](const FStreamingGenerationActorDescView* ActorDescView) { return ActorDescView; });

				TArray<FName> RuntimeDataLayerInstanceNames;
				if (FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(DataLayerManager, *ActorDescView, WorldDataLayerViews, RuntimeDataLayerInstanceNames))
				{
					ActorDescView->SetRuntimeDataLayerInstanceNames(RuntimeDataLayerInstanceNames);
				}
				FStreamingGenerationActorDescViewMap* NonConstActorDescViewMap = const_cast<FStreamingGenerationActorDescViewMap*>(MainActorSetContainer->ActorDescViewMap);			
				NonConstActorDescViewMap->Emplace(HLODActorGuid, *ActorDescView);

				const FStreamingGenerationActorDescView& HLODActorDescView = MainActorSetContainer->ActorDescViewMap->FindByGuidChecked(HLODActorGuid);
				GridsHLODActors.FindOrAdd(HLODActorDescView.GetRuntimeGrid()).Add(HLODActorGuid);
			}
		}
	};

	// Generate HLODs for the standard runtime grids (HLOD 0)
	for (int32 GridIndex = 0; GridIndex < Grids.Num(); GridIndex++)
	{
		// Generate HLODs for this grid - retrieve actors from the world partition
		GenerateHLODActors(Grids[GridIndex], 0, GridActorSetInstances[GridIndex]);
	}

	// Now, go on and create HLOD actors from HLOD grids (HLOD 1-N)
	for (auto It = HLODGrids.CreateIterator(); It; ++It)
	{
		const FName HLODGridName = It.Key();

		// No need to process empty grids
		if (!GridsHLODActors.Contains(HLODGridName))
		{
			It.RemoveCurrent(); // No need to keep this grid around, we have no actors in it
			continue;
		}

		// Create one actor cluster per HLOD actor
		TArray<IStreamingGenerationContext::FActorSetInstance> HLODActorSetInstances;
		TArray<const IStreamingGenerationContext::FActorSetInstance*> HLODActorSetInstancePtrs;
		HLODActorSetInstances.Reserve(GridsHLODActors[HLODGridName].Num());
		HLODActorSetInstancePtrs.Reserve(GridsHLODActors[HLODGridName].Num());
		MainActorSetContainer->ActorSets.Reserve(MainActorSetContainer->ActorSets.Num() + GridsHLODActors[HLODGridName].Num());

		for (const FGuid& HLODActorGuid : GridsHLODActors[HLODGridName])
		{
			const FStreamingGenerationActorDescView& HLODActorDescView = MainActorSetContainer->ActorDescViewMap->FindByGuidChecked(HLODActorGuid);
			
			IStreamingGenerationContext::FActorSetInstance& NewHLODActorSetInstance = HLODActorSetInstances.Emplace_GetRef();
			
			NewHLODActorSetInstance.Bounds = HLODActorDescView.GetRuntimeBounds();
			NewHLODActorSetInstance.RuntimeGrid = HLODActorDescView.GetRuntimeGrid();
			NewHLODActorSetInstance.bIsSpatiallyLoaded = HLODActorDescView.GetIsSpatiallyLoaded();
			if (DataLayerManager)
			{
				NewHLODActorSetInstance.DataLayers = DataLayerManager->GetRuntimeDataLayerInstances(HLODActorDescView.GetRuntimeDataLayerInstanceNames());
			}
			NewHLODActorSetInstance.ContentBundleID = MainContainerCollection->GetContentBundleGuid();
			NewHLODActorSetInstance.ContainerInstance = MainActorSetContainer;
			NewHLODActorSetInstance.ActorSet = MainActorSetContainer->ActorSets.Add_GetRef(MakeUnique<IStreamingGenerationContext::FActorSet>()).Get();
			const_cast<IStreamingGenerationContext::FActorSet*>(NewHLODActorSetInstance.ActorSet)->Actors.Add(HLODActorDescView.GetGuid());

			HLODActorSetInstancePtrs.Add(&NewHLODActorSetInstance);
		};

		// Generate HLODs for this grid - retrieve actors from our ValidHLODActors map
		// We can't rely on actor descs for newly created HLOD actors
		GenerateHLODActors(HLODGrids[HLODGridName], HLODLayersLevels[HLODGrids[HLODGridName].HLODLayer] + 1, HLODActorSetInstancePtrs);
	}

	auto DeleteHLODActor = [&SourceControlHelper, WorldPartition](FWorldPartitionHandle ActorHandle)
	{
		FWorldPartitionActorDesc* HLODActorDesc = ActorHandle.Get();
		check(HLODActorDesc);

		DeletePackage(WorldPartition, HLODActorDesc, SourceControlHelper);
	};

	// Destroy all unreferenced HLOD actors
	for (const auto& HLODActorPair : Context.HLODActorDescs)
	{
		DeletedActors.Add(HLODActorPair.Key);

		if (!Params.bReportOnly)
		{
			DeleteHLODActor(HLODActorPair.Value);
		}
	}

	if (!Params.bReportOnly)
	{
		// Create/destroy HLOD grid actors
		UpdateHLODGridsActors(GetWorld(), MainContainerCollection->GetMainActorDescContainer(), HLODGrids, SourceControlHelper);
	}

	auto DumpActorsStats = [](const TCHAR* StatType, const TArray<FName>& Actors)
	{
		UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Display, TEXT("\t%s actors (%d):"), StatType, Actors.Num());
		for (FName ActorName : Actors)
		{
			UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Display, TEXT("\t\t%s:"), *ActorName.ToString());
		}
	};

	UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Display, TEXT("SetupHLODActors stats:"));

	DumpActorsStats(TEXT("New"), NewActors);
	DumpActorsStats(TEXT("Dirty"), DirtyActors);
	DumpActorsStats(TEXT("Deleted"), DeletedActors);

	return true;
}

#endif // #if WITH_EDITOR
