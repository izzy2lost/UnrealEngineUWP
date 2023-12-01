// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionHelpers.h"

#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODModifier.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"

#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"

#include "UObject/SavePackage.h"
#include "HAL/PlatformFileManager.h"

#if WITH_EDITOR
class FHLODStreamingGenerationContext : public IStreamingGenerationContext
{
	using FActorSetInstanceList = TArray<IStreamingGenerationContext::FActorSetInstance>;

public:
	FHLODStreamingGenerationContext()
		: WorldBounds(ForceInit)
	{}

	virtual FBox GetWorldBounds() const override
	{
		return WorldBounds;
	}

	virtual const FActorSetContainer* GetMainWorldContainer() const override
	{
		return &ActorSetContainer;
	}

	virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const override
	{
		for (const FActorSetInstance& ActorSetInstance : ActorSetInstanceList)
		{
			Func(ActorSetInstance);
		}
	}

	virtual void ForEachActorSetContainer(TFunctionRef<void(const FActorSetContainer&)> Func) const override
	{
		Func(ActorSetContainer);
	}

	FBox WorldBounds;
	FActorSetContainer ActorSetContainer;
	FStreamingGenerationActorDescViewMap ActorDescViewMap;
	FActorSetInstanceList ActorSetInstanceList;
};

namespace PrivateUtils
{
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
				UE_LOG(LogWorldPartition, Error, TEXT("Error saving package %s."), *Package->GetName());
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
}

bool UWorldPartitionRuntimeHashSet::SupportsHLODs() const
{
	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		if (RuntimePartitionDesc.MainLayer)
		{
			if (RuntimePartitionDesc.MainLayer->SupportsHLODs())
			{
				return true;
			}
		}
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::SetupHLODActors(const IStreamingGenerationContext* StreamingGenerationContext, const UWorldPartition::FSetupHLODActorsParams& Params) const
{
	IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
	check(WPHLODUtilities);

	UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();
	IStreamingGenerationContext::FActorSetContainer* MainActorSetContainer = const_cast<IStreamingGenerationContext::FActorSetContainer*>(StreamingGenerationContext->GetMainWorldContainer());
	const FStreamingGenerationActorDescCollection* MainContainerCollection = MainActorSetContainer->ActorDescCollection;

	// Create the HLOD creation context
	FHLODCreationContext HLODCreationContext;
	for (FActorDescList::TConstIterator<AWorldPartitionHLOD> HLODIterator(MainContainerCollection->GetMainActorDescContainer()); HLODIterator; ++HLODIterator)
	{
		FWorldPartitionHandle HLODActorHandle(WorldPartition, HLODIterator->GetGuid());
		HLODCreationContext.HLODActorDescs.Emplace(HLODIterator->GetActorName(), MoveTemp(HLODActorHandle));
	}

	TUniquePtr<IStreamingGenerationContext> CurrentHLODStreamingGenerationContext = MakeUnique<FStreamingGenerationContextProxy>(StreamingGenerationContext);

	int32 HLODLevel = 0;
	while (CurrentHLODStreamingGenerationContext)
	{
		TMap<URuntimePartition*, TArray<URuntimePartition::FCellDescInstance>> RuntimePartitionsStreamingDescs;
		GenerateRuntimePartitionsStreamingDescs(CurrentHLODStreamingGenerationContext.Get(), RuntimePartitionsStreamingDescs);

		int32 NumNextLayerHLODActors = 0;
		TArray<FGuid> HLODActorGuids;
		for (auto& [RuntimePartition, CellDescInstances] : RuntimePartitionsStreamingDescs)
		{
			int32 CellDescInstanceIndex = 0;
			for (URuntimePartition::FCellDescInstance& CellDescInstance : CellDescInstances)
			{
				const FCellUniqueId CellUniqueId = GetCellUniqueId(CellDescInstance);

				UE_LOG(LogWorldPartition, Display, TEXT("[%d / %d] Processing cell %s..."), ++CellDescInstanceIndex, CellDescInstances.Num(), *CellUniqueId.Name);

				TArray<IStreamingGenerationContext::FActorInstance> ActorInstances;
				for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : CellDescInstance.ActorSetInstances)
				{
					ActorSetInstance->ForEachActor([this, ActorSetInstance, &ActorInstances](const FGuid& ActorGuid)
					{
						ActorInstances.Emplace(ActorGuid, ActorSetInstance);
					});
				}

				// Fake tick
				PrivateUtils::GameTick(WorldPartition->GetWorld());

				TArray<FName> MainPartitionTokens;
				TArray<FName> HLODPartitionTokens;
				verify(ParseGridName(ActorInstances[0].ActorSetInstance->RuntimeGrid, MainPartitionTokens, HLODPartitionTokens));

				if (MainPartitionTokens[0].IsNone())
				{
					MainPartitionTokens[0] = RuntimePartitions[0].Name;
				}
					
				FHLODCreationParams HLODCreationParams;
				HLODCreationParams.WorldPartition = WorldPartition;
				HLODCreationParams.CellName = CellUniqueId.Name;
				HLODCreationParams.CellGuid = CellUniqueId.Guid;			
				HLODCreationParams.CellBounds = CellDescInstance.Bounds;
				HLODCreationParams.GetRuntimeGrid = [&MainPartitionTokens](const UHLODLayer* InHLODLayer) { return FName(*FString::Printf(TEXT("%s:%s"), *FString::JoinBy(MainPartitionTokens, TEXT("."), [](const FName Token) { return Token.ToString(); }), *InHLODLayer->GetName())); };
				HLODCreationParams.HLODLevel = HLODLevel;
				HLODCreationParams.MinVisibleDistance = RuntimePartition->LoadingRange;
				HLODCreationParams.ContentBundleGuid = CellDescInstance.ContentBundleID;
				HLODCreationParams.DataLayerInstances = CellDescInstance.DataLayerInstances;

				TArray<AWorldPartitionHLOD*> CellHLODActors = WPHLODUtilities->CreateHLODActors(HLODCreationContext, HLODCreationParams, ActorInstances);

				if (!CellHLODActors.IsEmpty())
				{
					for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
					{
						FGuid ActorGuid = CellHLODActor->GetActorGuid();

						UPackage* CellHLODActorPackage = CellHLODActor->GetPackage();
						if (CellHLODActorPackage->HasAnyPackageFlags(PKG_NewlyCreated))
						{
							// Get a reference to newly create actors so they get unloaded when we release the references
							HLODCreationContext.ActorReferences.Emplace(WorldPartition, CellHLODActor->GetActorGuid());
						}

						HLODActorGuids.Add(ActorGuid);
						NumNextLayerHLODActors += CellHLODActor->GetHLODLayer() ? 1 : 0;
					}

					if (!Params.bReportOnly)
					{
						for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
						{
							if (CellHLODActor->GetPackage()->IsDirty())
							{
								PrivateUtils::SavePackage(CellHLODActor->GetPackage(), Params.SourceControlHelper);
							}
						}
					}
				}

				// Unload actors
				HLODCreationContext.ActorReferences.Empty();
			}
		}

		CurrentHLODStreamingGenerationContext.Reset();

		// Build the next HLOD generation context
		if (NumNextLayerHLODActors)
		{
			CurrentHLODStreamingGenerationContext = MakeUnique<FHLODStreamingGenerationContext>();
			FHLODStreamingGenerationContext* HLODStreamingGenerationContext = (FHLODStreamingGenerationContext*)CurrentHLODStreamingGenerationContext.Get();

			HLODStreamingGenerationContext->ActorSetInstanceList.Reserve(HLODActorGuids.Num());
			HLODStreamingGenerationContext->ActorSetContainer.ActorDescViewMap = &HLODStreamingGenerationContext->ActorDescViewMap;

			UE_LOG(LogWorldPartition, Log, TEXT("Creating HLOD context:"));
			for (const FGuid& HLODActorGuid : HLODActorGuids)
			{
				FWorldPartitionActorDesc* HLODActorDesc = WorldPartition->GetActorDesc(HLODActorGuid);
				check(HLODActorDesc);

				FStreamingGenerationActorDescView* HLODActorDescView = HLODStreamingGenerationContext->ActorDescViewMap.Emplace(HLODActorDesc);
				HLODStreamingGenerationContext->WorldBounds += HLODActorDescView->GetRuntimeBounds();
			
				// Create actor set instances
				IStreamingGenerationContext::FActorSet* ActorSet = HLODStreamingGenerationContext->ActorSetContainer.ActorSets.Emplace_GetRef(MakeUnique<IStreamingGenerationContext::FActorSet>()).Get();
				ActorSet->Actors.Add(HLODActorDescView->GetGuid());

				IStreamingGenerationContext::FActorSetInstance& ActorSetInstance = HLODStreamingGenerationContext->ActorSetInstanceList.Emplace_GetRef();

				ActorSetInstance.Bounds = HLODActorDescView->GetRuntimeBounds();
				ActorSetInstance.RuntimeGrid = HLODActorDescView->GetRuntimeGrid();
				ActorSetInstance.bIsSpatiallyLoaded = HLODActorDescView->GetIsSpatiallyLoaded();
				ActorSetInstance.ContentBundleID = MainContainerCollection->GetContentBundleGuid();
				ActorSetInstance.ContainerInstance = &HLODStreamingGenerationContext->ActorSetContainer;
				ActorSetInstance.ActorSet = ActorSet;

				TArray<const FWorldPartitionActorDescView*> WorldDataLayerViews;
				Algo::Transform(MainActorSetContainer->ActorDescViewMap->FindByExactNativeClass<AWorldDataLayers>(), WorldDataLayerViews, [](const FStreamingGenerationActorDescView* ActorDescView) { return ActorDescView; });

				TArray<FName> RuntimeDataLayerInstanceNames;
				if (FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(DataLayerManager, *HLODActorDescView, WorldDataLayerViews, RuntimeDataLayerInstanceNames))
				{
					HLODActorDescView->SetRuntimeDataLayerInstanceNames(RuntimeDataLayerInstanceNames);
					ActorSetInstance.DataLayers = DataLayerManager->GetRuntimeDataLayerInstances(RuntimeDataLayerInstanceNames);
				}

				UE_LOG(LogWorldPartition, Log, TEXT("\t- %s"), *HLODActorDesc->ToString());
			}

			HLODLevel++;
		}
	}

	// Destroy all unreferenced HLOD actors
	if (!Params.bReportOnly)
	{
		for (const auto& HLODActorPair : HLODCreationContext.HLODActorDescs)
		{
			FWorldPartitionActorDesc* HLODActorDesc = HLODActorPair.Value.Get();
			check(HLODActorDesc);

			PrivateUtils::DeletePackage(WorldPartition, HLODActorDesc, Params.SourceControlHelper);
		}
	}

	return false;
}
#endif
