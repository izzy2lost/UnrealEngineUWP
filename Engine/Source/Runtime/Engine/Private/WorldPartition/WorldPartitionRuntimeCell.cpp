// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "UObject/Package.h"
#include "Engine/Level.h"
#include "Misc/HierarchicalLogArchive.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeCell)

UWorldPartitionRuntimeCell::UWorldPartitionRuntimeCell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsAlwaysLoaded(false)
#if !UE_BUILD_SHIPPING
	, DebugStreamingPriority(-1.f)
#endif
	, RuntimeCellData(nullptr)
{
	EffectiveWantedState = EDataLayerRuntimeState::Unloaded;
	EffectiveWantedStateEpoch = MAX_int32;
}

UWorld* UWorldPartitionRuntimeCell::GetOwningWorld() const
{
	if (URuntimeHashExternalStreamingObjectBase* StreamingObjectOuter = GetTypedOuter<URuntimeHashExternalStreamingObjectBase>())
	{
		return StreamingObjectOuter->GetOwningWorld();
	}
	return UObject::GetTypedOuter<UWorldPartition>()->GetWorld();
}

UWorld* UWorldPartitionRuntimeCell::GetOuterWorld() const
{
	if (URuntimeHashExternalStreamingObjectBase* StreamingObjectOuter = GetTypedOuter<URuntimeHashExternalStreamingObjectBase>())
	{
		return StreamingObjectOuter->GetOuterWorld();
	}
	return UObject::GetTypedOuter<UWorld>();
}

#if WITH_EDITOR
void UWorldPartitionRuntimeCell::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (UnsavedActorsContainer)
	{
		// Make sure actor container isn't under PIE World so those template actors will never be considered part of the world.
		UnsavedActorsContainer->Rename(nullptr, GetPackage());

		for (auto& [ActorName, Actor] : UnsavedActorsContainer->Actors)
		{
			if (ensure(Actor))
			{
				const FName FolderPath = Actor->GetFolderPath();
				
				// Don't use AActor::Rename here since the actor is not part of the world, it's only a duplication template.	
				Actor->UObject::Rename(nullptr, UnsavedActorsContainer);
				
				// Mimic what AActor::Rename does under the hood to convert ActorFolders when changing the outer
				const bool bBroadcastChange = false;
				FSetActorFolderPath SetFolderPath(Actor, FolderPath, bBroadcastChange);
			}
		}
	}
}

bool UWorldPartitionRuntimeCell::NeedsActorToCellRemapping() const
{
	// When cooking, always loaded cells content is moved to persistent level (see PopulateGeneratorPackageForCook)
	return !(IsAlwaysLoaded() && IsRunningCookCommandlet());
}

void UWorldPartitionRuntimeCell::SetDataLayers(const TArray<const UDataLayerInstance*>& InDataLayerInstances)
{
	check(DataLayers.IsEmpty());
	DataLayers.Reserve(InDataLayerInstances.Num());
	for (const UDataLayerInstance* DataLayerInstance : InDataLayerInstances)
	{
		check(DataLayerInstance->IsRuntime());
		DataLayers.Add(DataLayerInstance->GetDataLayerFName());
	}
	DataLayers.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
}

void UWorldPartitionRuntimeCell::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	Ar.Printf(TEXT("Actor Count: %d"), GetActorCount());
}
#endif

int32 UWorldPartitionRuntimeCell::SortCompare(const UWorldPartitionRuntimeCell* Other) const
{
	return RuntimeCellData->SortCompare(Other->RuntimeCellData);
}

bool UWorldPartitionRuntimeCell::IsDebugShown() const
{
	return FWorldPartitionDebugHelper::IsDebugStreamingStatusShown(GetStreamingStatus()) &&
	       FWorldPartitionDebugHelper::AreDebugDataLayersShown(DataLayers) &&
		   (FWorldPartitionDebugHelper::CanDrawContentBundles() || !ContentBundleID.IsValid()) &&
			RuntimeCellData->IsDebugShown();
}

UDataLayerManager* UWorldPartitionRuntimeCell::GetDataLayerManager() const
{
	return GetOuterWorld()->GetWorldPartition()->GetDataLayerManager();
}

EDataLayerRuntimeState UWorldPartitionRuntimeCell::GetCellEffectiveWantedState() const
{
	if (!HasDataLayers())
	{
		EffectiveWantedState = EDataLayerRuntimeState::Activated;
	}
	else
	{
		const UWorld* OuterWorld = GetOuterWorld();
		const AWorldDataLayers* WorldDataLayers = OuterWorld->GetWorldDataLayers();
		check(WorldDataLayers);
		if (EffectiveWantedStateEpoch != WorldDataLayers->GetDataLayersStateEpoch())
		{
			EffectiveWantedState = EDataLayerRuntimeState::Unloaded;

			UWorldPartition* WorldPartition = OuterWorld->GetWorldPartition();
			if (const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager())
			{
				switch (WorldPartition->GetDataLayersLogicOperator())
				{
				case EWorldPartitionDataLayersLogicOperator::Or:
					if (DataLayerManager->IsAnyDataLayerInEffectiveRuntimeState(GetDataLayers(), EDataLayerRuntimeState::Activated))
					{
						EffectiveWantedState = EDataLayerRuntimeState::Activated;
					}
					else if (DataLayerManager->IsAnyDataLayerInEffectiveRuntimeState(GetDataLayers(), EDataLayerRuntimeState::Loaded))
					{
						EffectiveWantedState = EDataLayerRuntimeState::Loaded;
					}
					break;
				case EWorldPartitionDataLayersLogicOperator::And:
					if (DataLayerManager->IsAllDataLayerInEffectiveRuntimeState(GetDataLayers(), EDataLayerRuntimeState::Activated))
					{
						EffectiveWantedState = EDataLayerRuntimeState::Activated;
					}
					else if (DataLayerManager->IsAllDataLayerInEffectiveRuntimeState(GetDataLayers(), EDataLayerRuntimeState::Loaded))
					{
						EffectiveWantedState = EDataLayerRuntimeState::Loaded;
					}
					break;
				default:
					checkNoEntry();
				}
			}

			EffectiveWantedStateEpoch = WorldDataLayers->GetDataLayersStateEpoch();
		}
	}

	return EffectiveWantedState;
}

TArray<const UDataLayerInstance*> UWorldPartitionRuntimeCell::GetDataLayerInstances() const
{
	const UDataLayerManager* DataLayerManager = HasDataLayers() ? GetDataLayerManager() : nullptr;
	return DataLayerManager ? DataLayerManager->GetDataLayerInstances(GetDataLayers()) : TArray<const UDataLayerInstance*>();
}

bool UWorldPartitionRuntimeCell::ContainsDataLayer(const UDataLayerAsset* DataLayerAsset) const
{
	const UDataLayerManager* DataLayerManager = HasDataLayers() ? GetDataLayerManager() : nullptr;
	const UDataLayerInstance* DataLayerInstance = DataLayerManager ? DataLayerManager->GetDataLayerInstance(DataLayerAsset) : nullptr;
	return DataLayerInstance ? ContainsDataLayer(DataLayerInstance) : false;
}

bool UWorldPartitionRuntimeCell::HasContentBundle() const
{
	return GetContentBundleID().IsValid();
}

bool UWorldPartitionRuntimeCell::ContainsDataLayer(const UDataLayerInstance* DataLayerInstance) const
{
	return GetDataLayers().Contains(DataLayerInstance->GetDataLayerFName());
}

FName UWorldPartitionRuntimeCell::GetLevelPackageName() const
{ 
#if WITH_EDITOR
	return LevelPackageName;
#else
	return NAME_None;
#endif
}

FString UWorldPartitionRuntimeCell::GetDebugName() const
{
	return RuntimeCellData->GetDebugName();
}
