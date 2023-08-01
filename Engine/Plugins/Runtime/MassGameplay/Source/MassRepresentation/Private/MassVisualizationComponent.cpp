// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassVisualizationComponent.h"
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

//---------------------------------------------------------------
// UMassVisualizationComponent
//---------------------------------------------------------------

namespace UE::Mass::Representation
{
	int32 GCallUpdateInstances = 1;
	FAutoConsoleVariableRef  CVarCallUpdateInstances(TEXT("Mass.CallUpdateInstances"), GCallUpdateInstances, TEXT("Toggle between UpdateInstances and BatchUpdateTransform."));
}  // UE::Mass::Representation

void UMassVisualizationComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject) == false && GetOuter())
	{
		ensureMsgf(GetOuter()->GetClass()->IsChildOf(AMassVisualizer::StaticClass()), TEXT("UMassVisualizationComponent should only be added to AMassVisualizer-like instances"));
	}
}

int16 UMassVisualizationComponent::FindOrAddVisualDesc(const FStaticMeshInstanceVisualizationDesc& Desc)
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	int32 VisualIndex = InstancedStaticMeshInfos.IndexOfByPredicate([&Desc](const FMassInstancedStaticMeshInfo& Info) { return Info.GetDesc() == Desc; });
	if (VisualIndex == INDEX_NONE)
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
			VisualIndex = InstancedStaticMeshInfos.Emplace(Desc);
			BuildLODSignificanceForInfo(InstancedStaticMeshInfos[VisualIndex]);

			bNeedStaticMeshComponentConstruction = true;
		}
	}
	checkf(VisualIndex < INT16_MAX, TEXT("%hs resulting VisualIndex is out of expected bounds"), __FUNCTION__);
	return (int16)VisualIndex;
}

int16 UMassVisualizationComponent::AddVisualDescWithISMComponent(const FStaticMeshInstanceVisualizationDesc& Desc, UInstancedStaticMeshComponent& ISMComponent)
{
	checkf(Desc.Meshes.Num() > 0, TEXT("%hs is expected to be used when there's exactly one mesh description contained"), __FUNCTION__);
	ensureMsgf(Desc.Meshes.Num() == 1, TEXT("%hs is expected to be used when there's exactly one mesh description contained"), __FUNCTION__);

	const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Desc.Meshes[0];
	if (MeshDesc.Mesh == nullptr)
	{
		// invalid description, bail out
		return (int16)INDEX_NONE;
	}
	
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);

	const int32 VisualIndex = InstancedStaticMeshInfos.Emplace(Desc);
	const uint32 MeshDescHash = GetTypeHash(MeshDesc);
	const uint32 CombinedHash = PointerHash(&ISMComponent, MeshDescHash);
	FMassISMCSharedData& NewData = ISMCSharedData.FindOrAdd(CombinedHash, FMassISMCSharedData(&ISMComponent, 1));
	InstancedStaticMeshInfos[VisualIndex].AddISMComponent(NewData);

	BuildLODSignificanceForInfo(InstancedStaticMeshInfos[VisualIndex], CombinedHash);

	checkf(VisualIndex < INT16_MAX, TEXT("%hs resulting VisualIndex is out of expected bounds"), __FUNCTION__);
	return (int16)VisualIndex;
}

void UMassVisualizationComponent::ConstructStaticMeshComponents()
{
	AActor* ActorOwner = GetOwner();
	check(ActorOwner);
	
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		// Check if it is already created
		if (!Info.InstancedStaticMeshComponents.IsEmpty())
		{
			continue;
		}

		// Check if there are any specified meshes for this visual type
		if(Info.Desc.Meshes.Num() == 0)
		{
			UE_LOG(LogMassRepresentation, Error, TEXT("No associated meshes for this intanced static mesh type"));
			continue;
		}
		for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
		{
			FMassISMCSharedData* SharedData = ISMCSharedData.Find(GetTypeHash(MeshDesc));
			UInstancedStaticMeshComponent* ISMC = SharedData ? SharedData->GetISMComponent() : nullptr;

			if (SharedData == nullptr || SharedData->GetISMComponent() == nullptr)
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
					SharedData = &ISMCSharedData.Emplace(GetTypeHash(MeshDesc), FMassISMCSharedData(ISMC));
				}
				else
				{
					SharedData->SetISMComponent(*ISMC);
				}
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

void UMassVisualizationComponent::BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info, const uint32 ForcedStaticMeshRefKey)
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
		for (int i = 0; i < Info.LODSignificanceRanges.Num(); ++i)
		{
			FMassLODSignificanceRange& Range = Info.LODSignificanceRanges[i];
			Range.MinSignificance = AllLODSignificances[i];
			Range.MaxSignificance = AllLODSignificances[i+1];
			Range.ISMCSharedDataPtr = &ISMCSharedData;

			for (int j = 0; j < Info.Desc.Meshes.Num(); ++j)
			{
				const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Info.Desc.Meshes[j];
				const bool bAddMeshInRange = (Range.MinSignificance >= MeshDesc.MinLODSignificance && Range.MinSignificance < MeshDesc.MaxLODSignificance);
				if (bAddMeshInRange)
				{
					Range.StaticMeshRefs.Add(ForcedStaticMeshRefKey ? ForcedStaticMeshRefKey : GetTypeHash(MeshDesc));
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
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = It.Value().GetISMComponent())
		{
			InstancedStaticMeshComponent->ClearInstances();
			InstancedStaticMeshComponent->DestroyComponent();
		}
	}

	ISMCSharedData.Reset();
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
	if (bNeedStaticMeshComponentConstruction)
	{
		ConstructStaticMeshComponents();
		bNeedStaticMeshComponentConstruction = false;
	}
}

void UMassVisualizationComponent::HandleChangesWithExternalIDTracking(UInstancedStaticMeshComponent& ISMComponent, const FMassISMCSharedData& SharedData)
{
	constexpr float EqualTolerance = 1e-6;

	if (SharedData.HasUpdatesToApply() == false)
	{
		return;
	}

	UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(&ISMComponent);
	bool bAutoReset = HISMComp ? HISMComp->bAutoRebuildTreeOnInstanceChanges : false;

	if (SharedData.GetUpdateInstanceIds().Num())
	{
		TConstArrayView<int32> InstanceIds = SharedData.GetUpdateInstanceIds();
		const TArray<FTransform>& InstanceTransforms = SharedData.GetStaticMeshInstanceTransformsArray();
		const int32 InNumCustomDataFloats = SharedData.GetStaticMeshInstanceCustomFloats().Num();
		TConstArrayView<float> CustomFloatData = SharedData.GetStaticMeshInstanceCustomFloats();

		const int32 StartingCount = ISMComponent.PerInstanceSMData.Num();
		check(ISMComponent.InstanceIdToInstanceIndexMap.Num() == StartingCount);

		// if these are the first entities we're adding we need to set NumCustomDataFloats so that the PerInstanceSMCustomData
		// gets populated properly by the AddInstancesInternal call below
		if (StartingCount == 0 && CustomFloatData.Num() && ISMComponent.Mobility != EComponentMobility::Static)
		{
			ISMComponent.NumCustomDataFloats = InNumCustomDataFloats;
		}

		check(InstanceIds.Num() == InstanceTransforms.Num());
		TArray<int32> NewIndices = ISMComponent.AddInstances(InstanceTransforms, /*bShouldReturnIndices=*/true, /*bWorldSpace=*/true);
		
		check(InstanceIds.Num() == NewIndices.Num());
		ISMComponent.PerInstanceIds.AddDefaulted(ISMComponent.PerInstanceSMData.Num() - ISMComponent.PerInstanceIds.Num());

		for (int32 i = 0; i < InstanceIds.Num(); ++i)
		{
			checkfSlow(ISMComponent.InstanceIdToInstanceIndexMap.Find(InstanceIds[i]) == nullptr
				, TEXT("This occuring signals trouble. None of the InstanceIds is expected to have already been added to this MassISM component instance."));

			ISMComponent.InstanceIdToInstanceIndexMap.Add(InstanceIds[i], NewIndices[i]);
			ISMComponent.PerInstanceIds[NewIndices[i]] = InstanceIds[i];
		}

		ensureMsgf(CustomFloatData.Num() == 0, TEXT("Custom floats not supported with this set up just yet."));
#if 0 // CustomFloatData support below
		if (CustomFloatData.Num() && ISMComponent.Mobility != EComponentMobility::Static)
		{
			checkf(ISMComponent.NumCustomDataFloats == InNumCustomDataFloats, TEXT("Adding instances with a Custrom Floats count inconsisntent with previously added instances"));

			for (int32 i = 0; i < NewIndices.Num(); ++i)
			{
				const int32 InstanceIndex = NewIndices[i];
				const int32 TargetCustomDataOffset = InstanceIndex * ISMComponent.NumCustomDataFloats;
				const int32 SrcCustomDataOffset = i * ISMComponent.NumCustomDataFloats;
				for (int32 FloatIndex = 0; FloatIndex < ISMComponent.NumCustomDataFloats; ++FloatIndex)
				{
					// we're making a change only if any of the input data differs of the currently stored ones
					if (FMath::Abs(CustomFloatData[SrcCustomDataOffset + FloatIndex] - ISMComponent.PerInstanceSMCustomData[TargetCustomDataOffset + FloatIndex]) > EqualTolerance)
					{
						// Update the component's data in place.
						FMemory::Memcpy(&ISMComponent.PerInstanceSMCustomData[TargetCustomDataOffset], &CustomFloatData[SrcCustomDataOffset], ISMComponent.NumCustomDataFloats * sizeof(float));

						// Record in a command buffer for future use.
						// Using AddInstance here rather than SetCustomData because AddInstancesInternal we used to create 
						// instances doesn't add commands to InstanceUpdateCmdBuffer
						ISMComponent.InstanceUpdateCmdBuffer.AddInstance(InstanceIds[i], InstanceTransforms[i].ToMatrixWithScale(), FMatrix()
							, MakeArrayView((const float*)&CustomFloatData[SrcCustomDataOffset], ISMComponent.NumCustomDataFloats));

						break;
					}
				}
			}
		}
		else 
		{
			const FTransform& ComponentTransform = ISMComponent.GetComponentTransform();
			// since AddInstancesInternal called above doesn't add any commands we need to add them here.
			// The "there are custom floats" path is using a different, command flavor
			for (const FTransform& InstanceTransform : InstanceTransforms)
			{
				ISMComponent.InstanceUpdateCmdBuffer.AddInstance(InstanceTransform.GetRelativeTransform(ComponentTransform).ToMatrixWithScale());
			}
		}
#endif // 0
	}

	if (SharedData.GetRemoveInstanceIds().Num())
	{
		//RemoveInstanceWithIds(SharedData.GetRemoveInstanceIds());
		TConstArrayView<int32> InstanceIds = SharedData.GetRemoveInstanceIds();

		TArray<int32> InstanceIndices;
		InstanceIndices.Reserve(InstanceIds.Num());
		// Inform the cmd buffer which instances were removed.
		for (const int32 InstanceId : InstanceIds)
		{
			const int32* InstanceIndexPtr = ISMComponent.InstanceIdToInstanceIndexMap.Find(InstanceId);
			if (InstanceIndexPtr)
			{
				if (*InstanceIndexPtr != INDEX_NONE)
				{
					InstanceIndices.Add(*InstanceIndexPtr);
				}
				ISMComponent.InstanceIdToInstanceIndexMap.Remove(InstanceId);
			}
		}

		if (InstanceIndices.Num() == 0)
		{
			return;
		}

		// below is a reimplementation of ISMComponent.RemoveInstances since the original doesn't support keeping 
		// ISMComponent.InstanceIdToInstanceIndexMap up to date and since ISMComponent.InstanceIdToInstanceIndexMap 
		// mechanics are to be reimplemented there's no point in adding this to the original function. This code is 
		// a stop-gap until the ISM changes come online

		InstanceIndices.Sort(TGreater<int32>());

		if (!ISMComponent.PerInstanceSMData.IsValidIndex(InstanceIndices[0]) || !ISMComponent.PerInstanceSMData.IsValidIndex(InstanceIndices.Last()))
		{
			return;
		}

		// update the Id <-> Index mappings
		for (int32 Index : InstanceIndices)
		{
			ISMComponent.InstanceUpdateCmdBuffer.HideInstance(Index);

			if (Index == ISMComponent.PerInstanceIds.Num() - 1)
			{
				ISMComponent.PerInstanceIds.RemoveAt(Index, 1, /*bAllowShrinking=*/false);
			}
			else
			{
				ISMComponent.PerInstanceIds.RemoveAtSwap(Index, 1, /*bAllowShrinking=*/false);
				const int32 NewIdAtIndex = ISMComponent.PerInstanceIds[Index];
				ISMComponent.InstanceIdToInstanceIndexMap.FindChecked(NewIdAtIndex) = Index;
			}
		}

		for (const int32 InstanceIndex : InstanceIndices)
		{
#if WITH_EDITOR
			ISMComponent.DeletionState = UInstancedStaticMeshComponent::EInstanceDeletionReason::EntryRemoval;
#endif

			const int32 LastInstanceIndex = ISMComponent.PerInstanceSMData.Num() - 1;

			// remove instance
			if (ISMComponent.PerInstanceSMData.IsValidIndex(InstanceIndex))
			{
				ISMComponent.PerInstanceSMData.RemoveAtSwap(InstanceIndex, 1, false);
				ISMComponent.PerInstanceSMCustomData.RemoveAt(InstanceIndex * ISMComponent.NumCustomDataFloats, ISMComponent.NumCustomDataFloats, false);
			}

#if WITH_EDITOR
			// remove selection flag if array is filled in
			if (ISMComponent.SelectedInstances.IsValidIndex(InstanceIndex))
			{
				ISMComponent.SelectedInstances.RemoveAtSwap(InstanceIndex);
			}
#endif

			// update the physics state
			if (ISMComponent.IsPhysicsStateCreated() && ISMComponent.InstanceBodies.IsValidIndex(InstanceIndex))
			{
				// Clean up physics for removed instance
				if (ISMComponent.InstanceBodies[InstanceIndex])
				{
					ISMComponent.InstanceBodies[InstanceIndex]->TermBody();
					delete ISMComponent.InstanceBodies[InstanceIndex];
				}

				if (InstanceIndex == LastInstanceIndex)
				{
					// If we removed the last instance in the array we just need to remove it from the InstanceBodies array too.
					ISMComponent.InstanceBodies.RemoveAt(InstanceIndex);
				}
				else
				{
					if (ISMComponent.InstanceBodies[LastInstanceIndex])
					{                      
						// term physics for swapped instance
						ISMComponent.InstanceBodies[LastInstanceIndex]->TermBody();
					}

					// swap in the last instance body if we have one
					ISMComponent.InstanceBodies.RemoveAtSwap(InstanceIndex, 1, false);

					// recreate physics for the instance we swapped in the removed item's place
					// a bit hacky update to FBodyInstance - the FBodyInstance.InstanceBodyIndex needs to match InstanceIndex
					if (ISMComponent.InstanceBodies[InstanceIndex])
					{
						ISMComponent.InstanceBodies[InstanceIndex]->InstanceBodyIndex = InstanceIndex;
					}
				}
			}

			// Notify that these instances have been removed/relocated
			if (FInstancedStaticMeshDelegates::OnInstanceIndexUpdated.IsBound())
			{
				TArray<FInstancedStaticMeshDelegates::FInstanceIndexUpdateData, TInlineAllocator<2>> IndexUpdates;
				IndexUpdates.Reserve(1 + (ISMComponent.PerInstanceSMData.Num() - InstanceIndex));

				IndexUpdates.Add(FInstancedStaticMeshDelegates::FInstanceIndexUpdateData{ FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Removed, InstanceIndex });
				if (InstanceIndex != LastInstanceIndex)
				{
					// ISMs use swap remove, so the last index has been moved to the spot we removed from
					IndexUpdates.Add(FInstancedStaticMeshDelegates::FInstanceIndexUpdateData{ FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Relocated, InstanceIndex, LastInstanceIndex });
				}

				FInstancedStaticMeshDelegates::OnInstanceIndexUpdated.Broadcast(&ISMComponent, IndexUpdates);
			}

			// Force recreation of the render data
			ISMComponent.InstanceUpdateCmdBuffer.Edit();
#if WITH_EDITOR
			ISMComponent.DeletionState = UInstancedStaticMeshComponent::EInstanceDeletionReason::NotDeleting;
#endif
		}
	}

	ISMComponent.MarkRenderStateDirty();

	if (bNavigationRelevant && ISMComponent.GetInstanceCount() == 0)
	{
		FNavigationSystem::UnregisterComponent(ISMComponent);
	}
	else
	{
		FNavigationSystem::UpdateComponentData(ISMComponent);
	}

	if (HISMComp)
	{
		HISMComp->bAutoRebuildTreeOnInstanceChanges = bAutoReset;
		HISMComp->BuildTreeIfOutdated(true, false);
	}
}

void UMassVisualizationComponent::EndVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent EndVisualChanges")

	// Batch update gathered instance transforms
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		FMassISMCSharedData& SharedData = It.Value();
		if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = SharedData.GetISMComponent())
		{
			if (SharedData.RequiresExternalInstanceIDTracking())
			{
				HandleChangesWithExternalIDTracking(*InstancedStaticMeshComponent, SharedData);
			}
			else
			{
				const int32 NumCustomDataFloats = SharedData.StaticMeshInstanceCustomFloats.Num() / (FMath::Max(1, SharedData.UpdateInstanceIds.Num()));

				// Ensure InstanceCustomData is passed if NumCustomDataFloats > 0. If it is, also make sure
				// its length is NumCustomDataFloats * InstanceTransforms.Num()
				ensure(NumCustomDataFloats == 0 || (SharedData.StaticMeshInstanceCustomFloats.Num() == NumCustomDataFloats * SharedData.UpdateInstanceIds.Num()));

				// Resize PerInstanceSMData & PerInstanceSMCustomData to new (possibly culled or expanded) transform batch length
				const int32 NewNumInstances = SharedData.StaticMeshInstanceTransforms.Num();

				// Update PerInstanceSMData transforms
				if ((bool)UE::Mass::Representation::GCallUpdateInstances)
				{
					InstancedStaticMeshComponent->UpdateInstances(SharedData.UpdateInstanceIds, SharedData.StaticMeshInstanceTransforms, SharedData.StaticMeshInstancePrevTransforms, NumCustomDataFloats, SharedData.StaticMeshInstanceCustomFloats);
					if (UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(InstancedStaticMeshComponent))
					{
						HISMComp->BuildTreeIfOutdated(true, false);
					}
				}
				else
				{
					// Update NumCustomDataFloats
					InstancedStaticMeshComponent->NumCustomDataFloats = NumCustomDataFloats;
					if (NumCustomDataFloats > 0)
					{
						InstancedStaticMeshComponent->PerInstanceSMCustomData = SharedData.StaticMeshInstanceCustomFloats;
					}

					InstancedStaticMeshComponent->PerInstanceSMData.SetNum(NewNumInstances, /*bAllowShrinking*/false);
					InstancedStaticMeshComponent->PerInstancePrevTransform.SetNum(NewNumInstances, /*bAllowShrinking*/false);

					// Update PerInstanceSMData transforms
					InstancedStaticMeshComponent->BatchUpdateInstancesTransforms(/*StartInstanceIndex*/0, SharedData.StaticMeshInstanceTransforms, SharedData.StaticMeshInstancePrevTransforms, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/false);

					// Nanite ISMC? 
					TObjectPtr<UStaticMesh> StaticMeshObjectPtr = InstancedStaticMeshComponent->GetStaticMesh();
					FStaticMeshRenderData* StaticMeshRenderData = nullptr;
					if (UStaticMesh* StaticMesh = StaticMeshObjectPtr.Get())
					{
						StaticMeshRenderData = StaticMesh->GetRenderData();
					}
					const bool bNaniteISMC = UseNanite(InstancedStaticMeshComponent->GetScene()->GetShaderPlatform()) && StaticMeshRenderData && StaticMeshRenderData->HasValidNaniteData();
					if (bNaniteISMC)
					{
						// ISMC currently rebuilds PerInstanceRenderData regardless of whether it's using a 
						// Nanite::FSceneProxy which doesn't actually use this data. So to skip that we 
						// reset the InstanceUpdateCmdBuffer here after BatchUpdateInstancesTransforms has
						// marked it dirty but before CreateSceneProxy checks it
						// @todo This should be in ISMC code
						InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.Cmds.Reset();
						InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.NumAdds = 0;
						InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.NumEdits = 0;
					}

					// Dirty render state
					InstancedStaticMeshComponent->MarkRenderStateDirty();
				}
			}
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
		if (SharedData && SharedData->ReleaseReference() == 0)
		{
			if (UInstancedStaticMeshComponent* ISMC = SharedData->GetISMComponent())
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

void FMassLODSignificanceRange::AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
		{
			continue;
		}

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];

		SharedData.UpdateInstanceIds.Add(InstanceId);
		SharedData.StaticMeshInstanceTransforms.Add(Transform);
		SharedData.StaticMeshInstancePrevTransforms.Add(PrevTransform);
	}
}

void FMassLODSignificanceRange::AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
		{
			continue;
		}

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.StaticMeshInstanceCustomFloats.Append(CustomFloats);
	}
}

void FMassLODSignificanceRange::AddInstance(const int32 InstanceId, const FTransform& Transform)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.UpdateInstanceIds.Add(InstanceId);
		SharedData.StaticMeshInstanceTransforms.Add(Transform);
		SharedData.StaticMeshInstancePrevTransforms.Add(Transform);
	}
}

void FMassLODSignificanceRange::RemoveInstance(const int32 InstanceId)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.RemoveInstanceIds.Add(InstanceId);
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

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[StaticMeshIndex]];

		int32 StartIndex = FloatsPerInstance * SharedData.WriteIterator + StartFloatIndex;

		ensure(SharedData.StaticMeshInstanceCustomFloats.Num() >= StartIndex + CustomFloats.Num());

		for (int CustomFloatIdx = 0; CustomFloatIdx < CustomFloats.Num(); CustomFloatIdx++)
		{
			SharedData.StaticMeshInstanceCustomFloats[StartIndex + CustomFloatIdx] = CustomFloats[CustomFloatIdx];
		}
		SharedData.WriteIterator++;
	}
}
