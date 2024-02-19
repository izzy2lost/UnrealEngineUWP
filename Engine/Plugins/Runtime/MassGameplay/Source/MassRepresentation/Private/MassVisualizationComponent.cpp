// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassVisualizationComponent.h"
#include "CoreGlobals.h"
#include "Logging/LogMacros.h"
#include "MassVisualizer.h"
#include "MassRepresentationTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/InstancedStaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "VisualLogger/VisualLogger.h"
#include "Rendering/NaniteResources.h"
#include "AI/NavigationSystemBase.h"


DECLARE_CYCLE_STAT(TEXT("Mass Visualization EndVisualChanges"), STAT_Mass_VisualizationComponent_EndVisualChanges, STATGROUP_Mass);
DECLARE_CYCLE_STAT(TEXT("Mass Visualization HandleIDs"), STAT_Mass_VisualizationComponent_HandleChangesWithExternalIDTracking, STATGROUP_Mass);

DECLARE_DWORD_COUNTER_STAT(TEXT("VisualizationComp Instances Removed"), STAT_Mass_VisualizationComponent_InstancesRemovedNum, STATGROUP_Mass);
DECLARE_DWORD_COUNTER_STAT(TEXT("VisualizationComp Instances Added"), STAT_Mass_VisualizationComponent_InstancesAddedNum, STATGROUP_Mass);


namespace UE::Mass::Private
{
	uint32 CalculateComponentHash(const TObjectPtr<UInstancedStaticMeshComponent>& ISMComponent)
	{
		constexpr bool bUseObjectPathHash = false;
		constexpr bool bUseAssetPathHash = false;

		if constexpr (bUseObjectPathHash)
		{
			// This approach can result in (desired) reuse of hash if a given component is being recreated for 
            // due to it or the owner being streamed in and out repeatedly. This approach however is fragile 
            // to potential outer level name changes as part of GC preparation (depending on settings and context 
            // might take place with WorldPartition approach)
			check(ISMComponent);
			const FString ISMCPath = ISMComponent->GetPathName();
			return GetTypeHash(ISMCPath);
		}
		else if constexpr (bUseAssetPathHash)
		{
			// This approach is great if a given ISMComponent's outer ULevel is never getting 
			// used twice at the same time (i.e. it will break if said ULevel gets instantiated
			// multiple times)
			const FSoftObjectPath ObjectPath(ISMComponent);
			const FString AssetPathString = ObjectPath.GetAssetPathString();
			return HashCombine(GetTypeHash(AssetPathString), GetTypeHash(ISMComponent.GetFName()));
		}
		else
		{
			// fallback hashing that will always work at runtime, but requires care when serializing
			// Note that at the moment Mass doesn't have a native serialization implementation.
			return PointerHash(ISMComponent.Get());
		}
	}
}

//---------------------------------------------------------------
// UMassVisualizationComponent
//---------------------------------------------------------------

namespace UE::Mass::Representation
{
	int32 GCallUpdateInstances = 1;
	FAutoConsoleVariableRef  CVarCallUpdateInstances(TEXT("Mass.CallUpdateInstances"), GCallUpdateInstances, TEXT("Toggle between UpdateInstances and BatchUpdateTransform."));

#if STATS
	uint32 LastStatsResetFrame = 0;
#endif // STATS
}  // UE::Mass::Representation

void UMassVisualizationComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject) == false && GetOuter())
	{
		ensureMsgf(GetOuter()->GetClass()->IsChildOf(AMassVisualizer::StaticClass()), TEXT("UMassVisualizationComponent should only be added to AMassVisualizer-like instances"));
	}
}

FStaticMeshInstanceVisualizationDescHandle UMassVisualizationComponent::AddInstancedStaticMeshInfo(const FStaticMeshInstanceVisualizationDesc& Desc)
{
	FStaticMeshInstanceVisualizationDescHandle Handle;
	if (InstancedStaticMeshInfosFreeIndices.Num() > 0)
	{
		Handle = InstancedStaticMeshInfosFreeIndices.Pop(EAllowShrinking::No);
		new(&InstancedStaticMeshInfos[Handle.ToIndex()]) FMassInstancedStaticMeshInfo(Desc);
	}
	else
	{
		int32 AddedInfoIndex = InstancedStaticMeshInfos.Emplace(Desc);
		Handle = FStaticMeshInstanceVisualizationDescHandle(AddedInfoIndex);
	}

	return Handle;
}

FStaticMeshInstanceVisualizationDescHandle UMassVisualizationComponent::FindOrAddVisualDesc(const FStaticMeshInstanceVisualizationDesc& Desc)
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	// First check to see if we already have a matching Desc already and reuse / return that
	// Note: FStaticMeshInstanceVisualizationDescHandle(int32) handles the INDEX_NONE case here, generating an invalid handle in this case
	FStaticMeshInstanceVisualizationDescHandle VisualDescHandle(InstancedStaticMeshInfos.IndexOfByPredicate([&Desc](const FMassInstancedStaticMeshInfo& Info) { return Info.GetDesc() == Desc; }));
	if (!VisualDescHandle.IsValid())
	{
		bool bValidDescription = false;

		for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Desc.Meshes)
		{
			if (MeshDesc.Mesh && MeshDesc.ISMComponentClass)
			{
				ISMCSharedData.FindOrAdd(GetTypeHash(MeshDesc), FMassISMCSharedData());
				bValidDescription = true;
			}
		}

		if (bValidDescription)
		{
			VisualDescHandle = AddInstancedStaticMeshInfo(Desc);
			BuildLODSignificanceForInfo(InstancedStaticMeshInfos[VisualDescHandle.ToIndex()]);

			InstancedSMComponentsRequiringConstructing.Add(VisualDescHandle);

			check(VisualDescHandle.IsValid());
		}
		else
		{
			UE_LOG(LogMassRepresentation, Warning, TEXT("%hs: invalid FStaticMeshInstanceVisualizationDesc passed in. Check the contained meshes."), __FUNCTION__);
		}
	}

	return VisualDescHandle;
}

FStaticMeshInstanceVisualizationDescHandle UMassVisualizationComponent::AddVisualDescWithISMComponent(const FStaticMeshInstanceVisualizationDesc& Desc, UInstancedStaticMeshComponent& ISMComponent)
{
	TObjectPtr<UInstancedStaticMeshComponent> AsObjectPtr = ISMComponent;
	return AddVisualDescWithISMComponents(Desc, MakeArrayView(&AsObjectPtr, 1));
}

FStaticMeshInstanceVisualizationDescHandle UMassVisualizationComponent::AddVisualDescWithISMComponents(const FStaticMeshInstanceVisualizationDesc& Desc, TArrayView<TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents)
{
	check(Desc.Meshes.Num() == ISMComponents.Num());
	
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);

	FStaticMeshInstanceVisualizationDescHandle VisualHandle;
	TArray<uint32> ISMComponentPathHashes;

	for (int32 EntryIndex = 0; EntryIndex < Desc.Meshes.Num(); ++EntryIndex)
	{
		const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Desc.Meshes[EntryIndex];
		if (MeshDesc.Mesh == nullptr || ISMComponents[EntryIndex] == nullptr)
		{
			// invalid description, log an continue.
			UE_VLOG_UELOG(this, LogMassRepresentation, Error, TEXT("Empty mesh at index %d while registering FStaticMeshInstanceVisualizationDesc instance"), EntryIndex);
			continue;
		}
	
		if (!VisualHandle.IsValid())
		{
			VisualHandle = AddInstancedStaticMeshInfo(Desc);
			check(VisualHandle.IsValid());
		}

		const uint32 ISMComponentPathHash = UE::Mass::Private::CalculateComponentHash(ISMComponents[EntryIndex]);
#if WITH_MASSGAMEPLAY_DEBUG
		const FString ISMCPath = ISMComponents[EntryIndex]->GetPathName();
		TArray<FString>& DebugPaths = DebugHashToPathMap.FindOrAdd(ISMComponentPathHash, TArray<FString>());
		if (!ensureMsgf(DebugPaths.Add(ISMCPath) == 0, TEXT("Multiple ISMC paths resulting in the same hash")))
		{
			UE_VLOG_UELOG(this, LogMassRepresentation, Error, TEXT("%hs multiple ISMComponents resulting in identical hash %u"), __FUNCTION__, ISMComponentPathHash);
			for (const FString& Path : DebugPaths)
			{
				UE_VLOG_UELOG(this, LogMassRepresentation, Error, TEXT("\t%s"), *Path);
			}
		}
#endif // WITH_MASSGAMEPLAY_DEBUG
		FMassISMCSharedData& NewData = ISMCSharedData.FindOrAdd(ISMComponentPathHash, FMassISMCSharedData(ISMComponents[EntryIndex], /*bInRequiresExternalInstanceIDTracking=*/true));
		InstancedStaticMeshInfos[VisualHandle.ToIndex()].AddISMComponent(NewData);
		ISMComponentPathHashes.Add(ISMComponentPathHash);
	
		ISMComponentMap.Add(ISMComponentPathHash, VisualHandle);
	}

	if (VisualHandle.IsValid())
	{
		BuildLODSignificanceForInfo(InstancedStaticMeshInfos[VisualHandle.ToIndex()], ISMComponentPathHashes);
	}

	return VisualHandle;
}

const FMassISMCSharedData* UMassVisualizationComponent::GetISMCSharedDataForDescriptionIndex(const int32 DescriptionIndex) const
{
	return ISMCSharedData.GetDataForIndex(DescriptionIndex);
}

void UMassVisualizationComponent::RemoveVisualDesc(const FStaticMeshInstanceVisualizationDescHandle VisualizationHandle)
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	
	if (ensure(InstancedStaticMeshInfos.IsValidIndex(VisualizationHandle.ToIndex()))
		&& ensureMsgf(InstancedStaticMeshInfos[VisualizationHandle.ToIndex()].IsValid(), TEXT("Trying to remove visualization data that has already been cleaned")))
	{
		for (TObjectPtr<UInstancedStaticMeshComponent>& ISMComponent : InstancedStaticMeshInfos[VisualizationHandle.ToIndex()].InstancedStaticMeshComponents)
		{
			// @todo using ISMComponent.GetPathName() here might be wrong for cases where we use GetTypeHash(MeshDesc)
			// to create the ISMComponentMap key
			const uint32 ISMComponentPathHash = UE::Mass::Private::CalculateComponentHash(ISMComponent);

			const bool bValidKey = ISMComponentMap.Contains(ISMComponentPathHash);
			checkf(bValidKey, TEXT("Failed to find %u as a key in ISMComponentMap, ISMC path: %s"), ISMComponentPathHash, *ISMComponent.GetPathName());
			if (bValidKey)
			{
				const FStaticMeshInstanceVisualizationDescHandle StoredVisualizationDescHandle = ISMComponentMap.FindAndRemoveChecked(ISMComponentPathHash);
				ensure(StoredVisualizationDescHandle == VisualizationHandle);
			}
		
			ISMCSharedData.Remove(ISMComponentPathHash);
#if WITH_MASSGAMEPLAY_DEBUG
			const FString ISMCPath = ISMComponent.GetPathName();
			TArray<FString>& Paths = DebugHashToPathMap.FindChecked(ISMComponentPathHash);
			ensure(Paths.Remove(ISMCPath));
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
		
		InstancedStaticMeshInfos[VisualizationHandle.ToIndex()].Reset();
		InstancedStaticMeshInfosFreeIndices.Add(VisualizationHandle);
	}
}

void UMassVisualizationComponent::ConstructStaticMeshComponents()
{
	AActor* ActorOwner = GetOwner();
	check(ActorOwner);
	
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (const FStaticMeshInstanceVisualizationDescHandle VisualDescHandle : InstancedSMComponentsRequiringConstructing)
	{
		if (!ensureMsgf(InstancedStaticMeshInfos.IsValidIndex(VisualDescHandle.ToIndex())
			, TEXT("InstancedStaticMeshInfos (size: %d) is never expected to shrink, so VisualDescHandle (value: %u) being invalid indicates it was wrong from the start.")
			, InstancedStaticMeshInfos.Num(), VisualDescHandle.ToIndex()))
		{
			continue;
		}

		FMassInstancedStaticMeshInfo& Info = InstancedStaticMeshInfos[VisualDescHandle.ToIndex()];

		// Check if it is already created
		if (!Info.InstancedStaticMeshComponents.IsEmpty())
		{
			continue;
		}

		// Check if there are any specified meshes for this visual type
		if(Info.Desc.Meshes.Num() == 0)
		{
			UE_LOG(LogMassRepresentation, Error, TEXT("No associated meshes for this instanced static mesh type"));
			continue;
		}
		for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
		{
			FMassISMCSharedData* SharedData = ISMCSharedData.Find(GetTypeHash(MeshDesc));
			UInstancedStaticMeshComponent* ISMC = SharedData ? SharedData->GetMutableISMComponent() : nullptr;

			if (ISMC == nullptr)
			{
				ISMC = NewObject<UInstancedStaticMeshComponent>(ActorOwner, MeshDesc.ISMComponentClass);
				CA_ASSUME(ISMC);
				REDIRECT_OBJECT_TO_VLOG(ISMC, this);

				ISMC->SetStaticMesh(MeshDesc.Mesh);
				for (int32 ElementIndex = 0; ElementIndex < MeshDesc.MaterialOverrides.Num(); ++ElementIndex)
				{
					if (UMaterialInterface* MaterialOverride = MeshDesc.MaterialOverrides[ElementIndex])
					{
						ISMC->SetMaterial(ElementIndex, MaterialOverride);
					}
				}
				ISMC->SetCullDistances(0, 1000000); // @todo: Need to figure out what to do here, either LOD or cull distances.
				ISMC->SetupAttachment(ActorOwner->GetRootComponent());
				ISMC->SetCanEverAffectNavigation(false);
				ISMC->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				ISMC->SetCastShadow(MeshDesc.bCastShadows);
				ISMC->Mobility = MeshDesc.Mobility;
				ISMC->SetReceivesDecals(false);
				ISMC->RegisterComponent();

				if (SharedData == nullptr)
				{
					SharedData = &ISMCSharedData.Add(GetTypeHash(MeshDesc), FMassISMCSharedData(ISMC));
				}
				else
				{
					SharedData->SetISMComponent(*ISMC);
				}

				const uint32 ISMComponentPathHash = UE::Mass::Private::CalculateComponentHash(ISMC);
				ensureMsgf(ISMComponentMap.Find(ISMComponentPathHash) == nullptr, TEXT("We've just created the ISMC that's being used here, so this check failing indicates hash-clash."));
				ISMComponentMap.Add(ISMComponentPathHash, VisualDescHandle); 
			}

			check(SharedData);
			Info.AddISMComponent(*SharedData);
		}

		// Build the LOD significance ranges
		if (Info.LODSignificanceRanges.Num() == 0)
		{
			BuildLODSignificanceForInfo(Info);
		}
	}
}

void UMassVisualizationComponent::BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info, TConstArrayView<uint32> ForcedStaticMeshRefKeys)
{
	TArray<float> AllLODSignificances;
	auto UniqueInsertOrdered = [&AllLODSignificances](const float Significance)
	{
		int i = 0;
		for (; i < AllLODSignificances.Num(); ++i)
		{
			// I did not use epsilon check here on purpose, because it will make it hard later meshes inside.
			if (Significance == AllLODSignificances[i])
			{
				return;
			}
			if (AllLODSignificances[i] > Significance)
			{
				break;
			}
		}
		AllLODSignificances.Insert(Significance, i);
	};
	for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
	{
		UniqueInsertOrdered(MeshDesc.MinLODSignificance);
		UniqueInsertOrdered(MeshDesc.MaxLODSignificance);
	}

	if (AllLODSignificances.Num() > 1)
	{
		Info.LODSignificanceRanges.SetNum(AllLODSignificances.Num() - 1);
		for (int RangeIndex = 0; RangeIndex < Info.LODSignificanceRanges.Num(); ++RangeIndex)
		{
			FMassLODSignificanceRange& Range = Info.LODSignificanceRanges[RangeIndex];
			Range.MinSignificance = AllLODSignificances[RangeIndex];
			Range.MaxSignificance = AllLODSignificances[RangeIndex + 1];
			Range.ISMCSharedDataPtr = &ISMCSharedData;

			for (int MeshIndex = 0; MeshIndex < Info.Desc.Meshes.Num(); ++MeshIndex)
			{
				const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Info.Desc.Meshes[MeshIndex];
				const bool bAddMeshInRange = (Range.MinSignificance >= MeshDesc.MinLODSignificance && Range.MinSignificance < MeshDesc.MaxLODSignificance);
				if (bAddMeshInRange)
				{
					Range.StaticMeshRefs.Add(ForcedStaticMeshRefKeys.IsValidIndex(MeshIndex) && ForcedStaticMeshRefKeys[MeshIndex]
						? ForcedStaticMeshRefKeys[MeshIndex] : GetTypeHash(MeshDesc));
				}
			}
		}
	}
}

void UMassVisualizationComponent::ClearAllVisualInstances()
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		Info.ClearVisualInstance(ISMCSharedData);
	}
	InstancedStaticMeshInfos.Reset();
	
	// Pool should already be empty, got a problem if it's not
	for (int32 SharedDataIndex = 0; SharedDataIndex < ISMCSharedData.Num(); ++SharedDataIndex)
	{
		if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = ISMCSharedData.GetAtIndex(SharedDataIndex).GetMutableISMComponent())
		{
			InstancedStaticMeshComponent->ClearInstances();
			InstancedStaticMeshComponent->DestroyComponent();
		}
	}

	ISMCSharedData.Reset();
	InstancedSMComponentsRequiringConstructing.Reset();
}

void UMassVisualizationComponent::DirtyVisuals()
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		for (UInstancedStaticMeshComponent* InstancedStaticMeshComponent : Info.InstancedStaticMeshComponents)
		{
			InstancedStaticMeshComponent->MarkRenderStateDirty();
		}
	}
}

void UMassVisualizationComponent::BeginVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent BeginVisualChanges")

	// Conditionally construct static mesh components
	if (InstancedSMComponentsRequiringConstructing.Num())
	{
		ConstructStaticMeshComponents();
		InstancedSMComponentsRequiringConstructing.Reset();
	}
}

void UMassVisualizationComponent::HandleChangesWithExternalIDTracking(UInstancedStaticMeshComponent& ISMComponent, FMassISMCSharedData& SharedData)
{
	if (SharedData.HasUpdatesToApply() == false)
	{
		// nothing to do here. We most probably were called as the part of the very first tick of this given SharedData
		// since all the SharedData starts off as `dirty`.
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_Mass_VisualizationComponent_HandleChangesWithExternalIDTracking);

	// removing instances first, since this operation is more resilient to duplicates. Plus we make an arbitrary decision 
	// that it's better to have redundant things visible than not seeing required things
	ProcessRemoves(ISMComponent, SharedData, /*bUpdateNavigation=*/false);

	// NOTE: This code path is designed to only perform Adds, never updates so updates are filtered out along with duplicates.
	TArray<FMassEntityHandle>& EntityHandles = SharedData.UpdateInstanceIds;
	if (EntityHandles.Num())
	{
		INC_DWORD_STAT_BY(STAT_Mass_VisualizationComponent_InstancesAddedNum, EntityHandles.Num());

		FMassISMCSharedData::FEntityToPrimitiveIdMap& SharedIdMap = SharedData.GetMutableEntityPrimitiveToIdMap();
		TArray<Experimental::FHashElementId> ElementIds;
		ElementIds.SetNumUninitialized(EntityHandles.Num());
		// Filter out all updates & duplicate adds
		for (int32 IDIndex = EntityHandles.Num() - 1; IDIndex >= 0; --IDIndex)
		{
			bool bWasAlreadyInMap = false;
			Experimental::FHashElementId ElementId = SharedIdMap.FindOrAddId(EntityHandles[IDIndex], FPrimitiveInstanceId{INDEX_NONE}, bWasAlreadyInMap);

			if (bWasAlreadyInMap)
			{
				SharedData.RemoveUpdatedInstanceIdsAtSwap(IDIndex);
				ElementIds.RemoveAtSwap(IDIndex);
			}
			else
			{
				ElementIds[IDIndex] = ElementId;
			}
		}

		// it's possible the loop above removed all the data, so we do one last check
		if (!EntityHandles.IsEmpty())
		{
			check(ElementIds.Num() == EntityHandles.Num());

			const TConstArrayView<FMassEntityHandle> InstanceIds = SharedData.UpdateInstanceIds;

			const TArray<FTransform>& InstanceTransforms = SharedData.GetStaticMeshInstanceTransformsArray();
			const int32 InNumCustomDataFloats = SharedData.GetStaticMeshInstanceCustomFloats().Num();
			TConstArrayView<float> CustomFloatData = SharedData.GetStaticMeshInstanceCustomFloats();

			// if these are the first entities we're adding we need to set NumCustomDataFloats so that the PerInstanceSMCustomData
			// gets populated properly by the AddInstancesInternal call below
			const int32 StartingCount = ISMComponent.GetNumInstances();
			const bool bInitiallyEmpty = (StartingCount == 0); 
			if (StartingCount == 0 && CustomFloatData.Num() && ISMComponent.Mobility != EComponentMobility::Static)
			{
				ISMComponent.SetNumCustomDataFloats(InNumCustomDataFloats);
			}

			check(EntityHandles.Num() == InstanceTransforms.Num());
			TArray<FPrimitiveInstanceId> NewIds = ISMComponent.AddInstancesById(InstanceTransforms, /*bWorldSpace=*/true, /*bUpdateNavigation =*/bInitiallyEmpty);
			check(EntityHandles.Num() == NewIds.Num());
			for (int32 i = 0; i < EntityHandles.Num(); ++i)
			{
				SharedIdMap.GetByElementId(ElementIds[i]).Value = NewIds[i];
			}
			ensureMsgf(CustomFloatData.Num() == 0, TEXT("Custom floats not supported with this set up just yet."));
		}
	}

	if (bNavigationRelevant && ISMComponent.GetInstanceCount() == 0)
	{
		FNavigationSystem::UnregisterComponent(ISMComponent);
	}
}


void UMassVisualizationComponent::ProcessRemoves(UInstancedStaticMeshComponent& ISMComponent, FMassISMCSharedData& SharedData, const bool bUpdateNavigation /*= true*/)
{
	if (!SharedData.GetRemoveInstanceIds().IsEmpty())
	{
		FMassISMCSharedData::FEntityToPrimitiveIdMap& SharedIdMap = SharedData.GetMutableEntityPrimitiveToIdMap();
		INC_DWORD_STAT_BY(STAT_Mass_VisualizationComponent_InstancesRemovedNum, SharedData.GetRemoveInstanceIds().Num());

		TConstArrayView<FMassEntityHandle> EntityHandles = SharedData.GetRemoveInstanceIds();

		TArray<FPrimitiveInstanceId> ISMInstanceIds;
		ISMInstanceIds.Reserve(EntityHandles.Num());
		
		// Translate Mass IDs to ISMC IDs
		for (const FMassEntityHandle MassInstanceId : EntityHandles)
		{
			Experimental::FHashElementId ElementId = SharedIdMap.FindId(MassInstanceId);
			if (ElementId.IsValid())
			{
				FPrimitiveInstanceId InstanceId = SharedIdMap.GetByElementId(ElementId).Value;
				check(InstanceId.IsValid());
				SharedIdMap.RemoveByElementId(ElementId);
				ISMInstanceIds.Add(InstanceId);
			}
		}

		ISMComponent.RemoveInstancesById(ISMInstanceIds, bUpdateNavigation);
	}
}

void UMassVisualizationComponent::EndVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent EndVisualChanges")
	SCOPE_CYCLE_COUNTER(STAT_Mass_VisualizationComponent_EndVisualChanges);

#if STATS
	if (UE::Mass::Representation::LastStatsResetFrame != GFrameNumber)
	{
		SET_DWORD_STAT(STAT_Mass_VisualizationComponent_InstancesRemovedNum, 0);
		SET_DWORD_STAT(STAT_Mass_VisualizationComponent_InstancesAddedNum, 0);
		UE::Mass::Representation::LastStatsResetFrame = GFrameNumber;
	}
#endif // STATS

	// Batch update gathered instance transforms
	for (FMassISMCSharedDataMap::FDirtyIterator It(ISMCSharedData); It; ++It)
	{
		FMassISMCSharedData& SharedData = *It;

		UInstancedStaticMeshComponent* ISMComponent = SharedData.GetMutableISMComponent();
		// @todo need to check validity this way since Mass used to rely on the assumption that all the ISM components used were
		// under its control. That's no longer the case, but the system has not been updated to take that into consideration.
		// This is a temporary fix. 
		if (IsValid(ISMComponent))
		{
			ensureMsgf(!Cast<UHierarchicalInstancedStaticMeshComponent>(ISMComponent), TEXT("The UMassVisualizationComponent does not support driving a HISM, since it is not suitable for rapid updates, replace `%s`."), *ISMComponent->GetFullName());

			if (SharedData.RequiresExternalInstanceIDTracking())
			{
				HandleChangesWithExternalIDTracking(*ISMComponent, SharedData);
				It.ClearDirtyFlag();
			}
			else
			{
				// Process all removes.
				ProcessRemoves(*ISMComponent, SharedData);

				const int32 NumCustomDataFloats = SharedData.StaticMeshInstanceCustomFloats.Num() / (FMath::Max(1, SharedData.UpdateInstanceIds.Num()));

				// Ensure InstanceCustomData is passed if NumCustomDataFloats > 0. If it is, also make sure
				// its length is NumCustomDataFloats * InstanceTransforms.Num()
				ensure(NumCustomDataFloats == 0 || (SharedData.StaticMeshInstanceCustomFloats.Num() == NumCustomDataFloats * SharedData.UpdateInstanceIds.Num()));
				ISMComponent->SetNumCustomDataFloats(NumCustomDataFloats);
				TArray<FMassEntityHandle>& EntityHandles = SharedData.UpdateInstanceIds;
				{
					// Loop over all the instances in the update and 
					// 1. Sort the data such that all Adds are last
					// 2. Remove any duplicates (unsure if they may exist)
					FMassISMCSharedData::FEntityToPrimitiveIdMap& SharedIdMap = SharedData.GetMutableEntityPrimitiveToIdMap();
					// Filter out all updates & duplicate adds
					TBitArray<> Unprocessed;
					Unprocessed.SetNum(SharedIdMap.GetMaxIndex(), true);
					// Process interval

					TConstArrayView<FTransform> PrevInstanceTransforms = SharedData.GetStaticMeshInstancePrevTransforms();
					TConstArrayView<FTransform> InstanceTransforms = SharedData.GetStaticMeshInstanceTransformsArray();
					TConstArrayView<float> CustomDataFloats = SharedData.GetStaticMeshInstanceCustomFloats();

					// Enable support for per-instance prev transforms, if it was not already enabled it will copy the current transforms.
					ISMComponent->SetHasPerInstancePrevTransforms(!PrevInstanceTransforms.IsEmpty());

					struct FAddItem
					{
						Experimental::FHashElementId ElementId;
						int32 IDIndex;
					};
					TArray<FAddItem> ToAdd;
					ToAdd.Reserve(EntityHandles.Num());
					for (int32 IDIndex = 0; IDIndex < EntityHandles.Num(); ++IDIndex)
					{
						bool bWasAlreadyInMap = false;
						Experimental::FHashElementId ElementId = SharedIdMap.FindOrAddId(EntityHandles[IDIndex], FPrimitiveInstanceId{INDEX_NONE}, bWasAlreadyInMap);

						// if it was already in the map, it may be a duplicate if we have processed it already
						bool bIsDuplicate = bWasAlreadyInMap && !Unprocessed[ElementId.GetIndex()];
						if (bIsDuplicate)
						{
							continue;
						}

						FPrimitiveInstanceId Id = SharedIdMap.GetByElementId(ElementId).Value;
						if (!Id.IsValid())
						{
							check(!bWasAlreadyInMap);
							ToAdd.Emplace(FAddItem{ElementId, IDIndex});
						}
						else
						{
							ISMComponent->UpdateInstanceTransformById(Id, InstanceTransforms[IDIndex]);
							if (!PrevInstanceTransforms.IsEmpty())
							{
								ISMComponent->SetPreviousTransformById(Id, PrevInstanceTransforms[IDIndex]);
							}
							if (!CustomDataFloats.IsEmpty())
							{
								ISMComponent->SetCustomDataById(Id, MakeArrayView(CustomDataFloats.GetData() + IDIndex * NumCustomDataFloats, NumCustomDataFloats));
							}
						}

						// Make sure we have enough space to track the already processed IDs
						Unprocessed.SetNum(SharedIdMap.GetMaxIndex(), true);
						Unprocessed[ElementId.GetIndex()] = false;
					}
					// Collect unwanted items & remove
					TArray<FPrimitiveInstanceId> RemovedISMInstanceIds;
					RemovedISMInstanceIds.Reserve(Unprocessed.Num());
					{
						for(TConstSetBitIterator<> BitIt(Unprocessed); BitIt; ++BitIt)
						{
							Experimental::FHashElementId ElementId(BitIt.GetIndex());
							if (SharedIdMap.ContainsElementId(ElementId))
							{
								FPrimitiveInstanceId InstanceId = SharedIdMap.GetByElementId(ElementId).Value;
								check(InstanceId.IsValid());
								SharedIdMap.RemoveByElementId(ElementId);
								RemovedISMInstanceIds.Add(InstanceId);
							}
						}		
						ISMComponent->RemoveInstancesById(RemovedISMInstanceIds);
					}
					// Process deferred adds.
					for (FAddItem AddItem : ToAdd)
					{
						FPrimitiveInstanceId Id = ISMComponent->AddInstanceById(InstanceTransforms[AddItem.IDIndex]);
						check(!SharedIdMap.GetByElementId(AddItem.ElementId).Value.IsValid());
						SharedIdMap.GetByElementId(AddItem.ElementId).Value = Id;

						if (!PrevInstanceTransforms.IsEmpty())
						{
							ISMComponent->SetPreviousTransformById(Id, PrevInstanceTransforms[AddItem.IDIndex]);
						}
						if (!CustomDataFloats.IsEmpty())
						{
							ISMComponent->SetCustomDataById(Id, MakeArrayView(CustomDataFloats.GetData() + AddItem.IDIndex * NumCustomDataFloats, NumCustomDataFloats));
						}

					}
					// note that we're not clearing the dirty flag on purpose - these components require constant updates
				}
			}

			// bump the touch counter so that anyone caching data based on contents of this SharedData can detect the change
			++SharedData.ComponentInstanceIdTouchCounter;
		}
		
		SharedData.ResetAccumulatedData();
	}
}

//---------------------------------------------------------------
// FMassInstancedStaticMeshInfo
//---------------------------------------------------------------

void FMassInstancedStaticMeshInfo::ClearVisualInstance(FMassISMCSharedDataMap& ISMCSharedData)
{
	for (int i = 0; i < Desc.Meshes.Num(); i++)
	{
		const uint32 MeshDescHash = GetTypeHash(Desc.Meshes[i]);
		FMassISMCSharedData* SharedData = ISMCSharedData.Find(MeshDescHash);
		if (SharedData && SharedData->OnISMComponentReferenceReleased() == 0)
		{
			if (UInstancedStaticMeshComponent* ISMC = SharedData->GetMutableISMComponent())
			{
				ISMC->ClearInstances();
				ISMC->DestroyComponent();
			}
			ISMCSharedData.Remove(MeshDescHash);
		}
	}

	InstancedStaticMeshComponents.Reset();
	LODSignificanceRanges.Reset();
}

//---------------------------------------------------------------
// FMassLODSignificanceRange
//---------------------------------------------------------------

void FMassLODSignificanceRange::AddBatchedTransform(const FMassEntityHandle EntityHandle, const FTransform& Transform, const FTransform& PrevTransform, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int32 StaticMeshIndex = 0; StaticMeshIndex < StaticMeshRefs.Num(); ++StaticMeshIndex)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[StaticMeshIndex]))
		{
			continue;
		}

		if (FMassISMCSharedData* SharedData = ISMCSharedDataPtr->GetAndMarkDirty(StaticMeshRefs[StaticMeshIndex]))
		{
			SharedData->UpdateInstanceIds.Add(EntityHandle);
			SharedData->StaticMeshInstanceTransforms.Add(Transform);
			SharedData->StaticMeshInstancePrevTransforms.Add(PrevTransform);
		}
	}
}

void FMassLODSignificanceRange::AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int32 StaticMeshIndex = 0; StaticMeshIndex < StaticMeshRefs.Num(); ++StaticMeshIndex)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[StaticMeshIndex]))
		{
			continue;
		}

		if (FMassISMCSharedData* SharedData = ISMCSharedDataPtr->GetAndMarkDirty(StaticMeshRefs[StaticMeshIndex]))
		{
			SharedData->StaticMeshInstanceCustomFloats.Append(CustomFloats);
		}
	}
}

void FMassLODSignificanceRange::AddInstance(const FMassEntityHandle EntityHandle, const FTransform& Transform)
{
	check(ISMCSharedDataPtr);
	for (int32 StaticMeshIndex = 0; StaticMeshIndex < StaticMeshRefs.Num(); ++StaticMeshIndex)
	{
		if (FMassISMCSharedData* SharedData = ISMCSharedDataPtr->GetAndMarkDirty(StaticMeshRefs[StaticMeshIndex]))
		{
			SharedData->UpdateInstanceIds.Add(EntityHandle);
			SharedData->StaticMeshInstanceTransforms.Add(Transform);
			SharedData->StaticMeshInstancePrevTransforms.Add(Transform);
		}
	}
}

void FMassLODSignificanceRange::RemoveInstance(const FMassEntityHandle EntityHandle)
{
	check(ISMCSharedDataPtr);
	for (int32 StaticMeshIndex = 0; StaticMeshIndex < StaticMeshRefs.Num(); ++StaticMeshIndex)
	{
		if (FMassISMCSharedData* SharedData = ISMCSharedDataPtr->GetAndMarkDirty(StaticMeshRefs[StaticMeshIndex]))
		{
			SharedData->RemoveInstanceIds.Add(EntityHandle);
		}
	}
}

void FMassLODSignificanceRange::WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const int32 FloatsPerInstance, const int32 StartFloatIndex, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	if (StaticMeshRefs.IsValidIndex(StaticMeshIndex))
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[StaticMeshIndex]))
		{
			return;
		}

		if (FMassISMCSharedData* SharedData = ISMCSharedDataPtr->GetAndMarkDirty(StaticMeshRefs[StaticMeshIndex]))
		{
			const int32 StartIndex = FloatsPerInstance * SharedData->WriteIterator + StartFloatIndex;

			ensure(SharedData->StaticMeshInstanceCustomFloats.Num() >= StartIndex + CustomFloats.Num());

			for (int CustomFloatIdx = 0; CustomFloatIdx < CustomFloats.Num(); CustomFloatIdx++)
			{
				SharedData->StaticMeshInstanceCustomFloats[StartIndex + CustomFloatIdx] = CustomFloats[CustomFloatIdx];
			}
			SharedData->WriteIterator++;
		}
	}
}

//-----------------------------------------------------------------------------
// DEPRECATED
//-----------------------------------------------------------------------------
void UMassVisualizationComponent::BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info, const uint32 ForcedStaticMeshRefKeys)
{
	BuildLODSignificanceForInfo(Info, MakeArrayView(&ForcedStaticMeshRefKeys, 1));
}

void UMassVisualizationComponent::RemoveISMComponent(UInstancedStaticMeshComponent& ISMComponent)
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);

	const uint32 ISMComponentPathHash = UE::Mass::Private::CalculateComponentHash(&ISMComponent);
	const FStaticMeshInstanceVisualizationDescHandle* VisualDescHandlePtr = ISMComponentMap.Find(ISMComponentPathHash);
	if (VisualDescHandlePtr)
	{
		RemoveVisualDesc(*VisualDescHandlePtr);
	}
}

void UMassVisualizationComponent::RemoveVisualDescByIndex(const int32 VisualizationIndex)
{
	RemoveVisualDesc(FStaticMeshInstanceVisualizationDescHandle(VisualizationIndex));
}
