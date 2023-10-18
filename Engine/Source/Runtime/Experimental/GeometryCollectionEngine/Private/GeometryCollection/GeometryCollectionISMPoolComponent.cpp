// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionISMPoolComponent)

// Use the FreeLists to enable recycling of ISM components.
// Might want to add a Tick function to keep the pool at a stable value over time.
// We would always want some spare components for fast allocation, but want to clean up when numbers get too high.
static bool GUseComponentFreeList = true;
FAutoConsoleVariableRef CVarISMPoolUseComponentFreeList(
	TEXT("r.ISMPool.UseComponentFreeList"),
	GUseComponentFreeList,
	TEXT("Recycle ISM components in the ISMPool."));

static int32 GComponentFreeListTargetSize = 50;
FAutoConsoleVariableRef CVarISMPoolComponentFreeListTargetSize(
	TEXT("r.ISMPool.ComponentFreeListTargetSize"),
	GComponentFreeListTargetSize,
	TEXT("Target size for number of ISM components in the ISMPool."));


FGeometryCollectionMeshGroup::FMeshId FGeometryCollectionMeshGroup::AddMesh(const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, const FGeometryCollectionMeshInfo& ISMInstanceInfo)
{
	const FMeshId MeshInfoIndex = MeshInfos.Emplace(ISMInstanceInfo);
	return MeshInfoIndex;
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (MeshInfos.IsValidIndex(MeshId))
	{
		return ISMPool.BatchUpdateInstancesTransforms(MeshInfos[MeshId], StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport, bAllowPerInstanceRemoval);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid mesh Id (%d) for this mesh group"), MeshId);
	return false;
}

void FGeometryCollectionMeshGroup::BatchUpdateInstanceCustomData(FGeometryCollectionISMPool& ISMPool, int32 CustomFloatIndex, float CustomFloatValue)
{
	for (const FGeometryCollectionMeshInfo& MeshInfo : MeshInfos)
	{
		ISMPool.BatchUpdateInstanceCustomData(MeshInfo, CustomFloatIndex, CustomFloatValue);
	}
}

void FGeometryCollectionMeshGroup::RemoveAllMeshes(FGeometryCollectionISMPool& ISMPool)
{
	for (const FGeometryCollectionMeshInfo& MeshInfo: MeshInfos)
	{
		ISMPool.RemoveISM(MeshInfo);
	}
	MeshInfos.Empty();
}

void FGeometryCollectionISM::CreateISM(AActor* InOwningActor)
{
	check(InOwningActor);

	ISMComponent = NewObject<UInstancedStaticMeshComponent>(InOwningActor, NAME_None, RF_Transient | RF_DuplicateTransient);

	ISMComponent->SetRemoveSwap();
	ISMComponent->SetCanEverAffectNavigation(false);
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMComponent->SetupAttachment(InOwningActor->GetRootComponent());

	InOwningActor->AddInstanceComponent(ISMComponent);
	ISMComponent->RegisterComponent();
}

void FGeometryCollectionISM::InitISM(const FGeometryCollectionStaticMeshInstance& InMeshInstance)
{
	MeshInstance = InMeshInstance;
	check(MeshInstance.StaticMesh);
	check(ISMComponent != nullptr);

#if WITH_EDITOR
	const FName ISMName = MakeUniqueObjectName(ISMComponent->GetOwner(), UInstancedStaticMeshComponent::StaticClass(), InMeshInstance.StaticMesh->GetFName());
	const FString ISMNameString = ISMName.ToString();
	ISMComponent->Rename(*ISMNameString);
#endif

	ISMComponent->SetStaticMesh(MeshInstance.StaticMesh);
	ISMComponent->SetMobility((MeshInstance.Desc.Flags & FISMComponentDescription::StaticMobility) != 0 ? EComponentMobility::Static : EComponentMobility::Movable);

	ISMComponent->EmptyOverrideMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MeshInstance.MaterialsOverrides.Num(); MaterialIndex++)
	{
		ISMComponent->SetMaterial(MaterialIndex, MeshInstance.MaterialsOverrides[MaterialIndex]);
	}
	
	ISMComponent->NumCustomDataFloats = MeshInstance.Desc.NumCustomDataFloats;
	for (int32 DataIndex = 0; DataIndex < MeshInstance.CustomPrimitiveData.Num(); DataIndex++)
	{
		ISMComponent->SetDefaultCustomPrimitiveDataFloat(DataIndex, MeshInstance.CustomPrimitiveData[DataIndex]);
	}

	const bool bReverseCulling = (MeshInstance.Desc.Flags & FISMComponentDescription::ReverseCulling) != 0;
	// Instead of reverse culling we put the mirror in the component transform so that PRIMITIVE_SCENE_DATA_FLAG_DETERMINANT_SIGN will be set for use by materials.
	//ISMComponent->SetReverseCulling(bReverseCulling);
	const FVector Scale = bReverseCulling ? FVector(-1, 1, 1) : FVector(1, 1, 1);
	const FTransform NewRelativeTransform(FQuat::Identity, MeshInstance.Desc.Position, Scale);
	if (!ISMComponent->GetRelativeTransform().Equals(NewRelativeTransform))
	{
		ISMComponent->SetRelativeTransform(FTransform(FQuat::Identity, MeshInstance.Desc.Position, Scale));
	}
	if ((MeshInstance.Desc.Flags & FISMComponentDescription::DistanceCullPrimitive) != 0)
	{
		ISMComponent->SetCachedMaxDrawDistance(MeshInstance.Desc.EndCullDistance);
	}
	ISMComponent->SetCullDistances(MeshInstance.Desc.StartCullDistance, MeshInstance.Desc.EndCullDistance);
	ISMComponent->SetCastShadow((MeshInstance.Desc.Flags & FISMComponentDescription::AffectShadow) != 0);
	ISMComponent->bAffectDynamicIndirectLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDynamicIndirectLighting) != 0;
	ISMComponent->bAffectDistanceFieldLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDistanceFieldLighting) != 0;
	ISMComponent->bWorldPositionOffsetWritesVelocity = (MeshInstance.Desc.Flags & FISMComponentDescription::WorldPositionOffsetWritesVelocity) != 0;
	ISMComponent->bEvaluateWorldPositionOffset = (MeshInstance.Desc.Flags & FISMComponentDescription::EvaluateWorldPositionOffset) != 0;
	ISMComponent->bUseGpuLodSelection = (MeshInstance.Desc.Flags & FISMComponentDescription::GpuLodSelection) != 0;
	ISMComponent->bOverrideMinLOD = MeshInstance.Desc.MinLod > 0;
	ISMComponent->MinLOD = MeshInstance.Desc.MinLod;
	ISMComponent->SetLODDistanceScale(MeshInstance.Desc.LodScale);
	ISMComponent->SetMeshDrawCommandStatsCategory(MeshInstance.Desc.StatsCategory);
	ISMComponent->ComponentTags = MeshInstance.Desc.Tags;
}

FInstanceGroups::FInstanceGroupId FGeometryCollectionISM::AddInstanceGroup(int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	// When adding new group it will always have a single range
	const FInstanceGroups::FInstanceGroupId InstanceGroupIndex = InstanceGroups.AddGroup(InstanceCount);
	const FInstanceGroups::FInstanceGroupRange& NewInstanceGroup = InstanceGroups.GroupRanges[InstanceGroupIndex];

	// Ensure that remapping arrays are big enough to hold any new items.
	InstanceIds.SetNum(InstanceGroups.GetMaxInstanceIndex(), false);

	FTransform ZeroScaleTransform;
	ZeroScaleTransform.SetIdentityZeroScale();
	TArray<FTransform> ZeroScaleTransforms;
	ZeroScaleTransforms.Init(ZeroScaleTransform, InstanceCount);

	ISMComponent->PreAllocateInstancesMemory(InstanceCount);
	TArray<FPrimitiveInstanceId> AddedInstanceIds = ISMComponent->AddInstancesById(ZeroScaleTransforms, true, true);
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		InstanceIds[NewInstanceGroup.Start + InstanceIndex] = AddedInstanceIds[InstanceIndex];
	}

	// Set any custom data.
	if (CustomDataFloats.Num())
	{
		const int32 NumCustomDataFloats = ISMComponent->NumCustomDataFloats;
		if (ensure(NumCustomDataFloats * InstanceCount == CustomDataFloats.Num()))
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				ISMComponent->SetCustomDataById(AddedInstanceIds[InstanceIndex], CustomDataFloats.Slice(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats));
			}
		}
	}

	return InstanceGroupIndex;
}

FGeometryCollectionISMPool::FISMIndex FGeometryCollectionISMPool::AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	FISMIndex* ISMIndexPtr = MeshToISMIndex.Find(MeshInstance);
	if (ISMIndexPtr != nullptr)
	{
		return *ISMIndexPtr;
	}

	// Take an ISM from the current FreeLists if available instead of allocating a new slot.
	FISMIndex ISMIndex = INDEX_NONE;
	if (FreeListISM.Num())
	{
		ISMIndex = FreeListISM.Last();
		FreeListISM.RemoveAt(FreeListISM.Num() - 1);
	}
	else if (FreeList.Num())
	{
		ISMIndex = FreeList.Last();
		FreeList.RemoveAt(FreeList.Num() - 1);
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner());
	}
	else
	{
		ISMIndex = ISMs.AddDefaulted();
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner());
	}
	
	ISMs[ISMIndex].InitISM(MeshInstance);
	
	MeshToISMIndex.Add(MeshInstance, ISMIndex);
	return ISMIndex;
}

FGeometryCollectionMeshInfo FGeometryCollectionISMPool::AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	FGeometryCollectionMeshInfo Info;
	Info.ISMIndex = AddISM(OwningComponent, MeshInstance);
	Info.InstanceGroupIndex = ISMs[Info.ISMIndex].AddInstanceGroup(InstanceCount, CustomDataFloats);
	return Info;
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport, bool bAllowPerInstanceRemoval)
{
	if (!ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid ISM Id (%d) when updating the transform "), MeshInfo.ISMIndex);
		return false;
	}
		
	FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
	const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
	// If ISM component has identity transform (the common case) then we can skip world space to component space maths inside BatchUpdateInstancesTransforms()
	bWorldSpace &= !ISM.ISMComponent->GetComponentTransform().Equals(FTransform::Identity, 0.f);

	// The transform count should fit within the instance group.
	// Clamp it if it doesn't, but if we hit this ensure we need to investigate why.
	ensure(StartInstanceIndex + NewInstancesTransforms.Num() <= InstanceGroup.Count);
	const int32 NumTransforms = FMath::Min(NewInstancesTransforms.Num(), InstanceGroup.Count - StartInstanceIndex);

	// Loop over transforms.
	// todo: There may be some value in batching InstanceIds and caling one function for each of Add/Remove/Update.
	// However the ISM batched calls themselves seem to be just simple loops over the single instance calls, so probably no benefit.
	for (int InstanceIndex = StartInstanceIndex; InstanceIndex < StartInstanceIndex + NumTransforms; ++InstanceIndex)
	{
		FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + InstanceIndex];
		FTransform const& Transform = NewInstancesTransforms[InstanceIndex];

		if (bAllowPerInstanceRemoval)
		{
			if (Transform.GetScale3D().IsZero() && InstanceId.IsValid())
			{
				// Zero scale is used to indicate that we should remove the instance from the ISM.
				ISM.ISMComponent->RemoveInstanceById(InstanceId);
				ISM.InstanceIds[InstanceGroup.Start + InstanceIndex] = FPrimitiveInstanceId();
				continue;
			}
			else if (!Transform.GetScale3D().IsZero() && !InstanceId.IsValid())
			{
				// Re-add the instance to the ISM if the scale becomes non-zero.
				ISM.InstanceIds[InstanceGroup.Start + InstanceIndex] = ISM.ISMComponent->AddInstanceById(Transform, bWorldSpace);
				continue;
			}
		}

		if (InstanceId.IsValid())
		{
			ISM.ISMComponent->UpdateInstanceTransformById(InstanceId, Transform, bWorldSpace, bTeleport);
		}
	}

	return true;
}

void FGeometryCollectionISMPool::BatchUpdateInstanceCustomData(FGeometryCollectionMeshInfo const& MeshInfo, int32 CustomFloatIndex, float CustomFloatValue)
{
	if (!ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		return;
	}
	
	FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
	if (!ensure(CustomFloatIndex < ISM.MeshInstance.Desc.NumCustomDataFloats))
	{
		return;
	}
		
	const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceGroup.Count; ++InstanceIndex)
	{
		const FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + InstanceIndex];
		if (InstanceId.IsValid())
		{
			ISM.ISMComponent->SetCustomDataValueById(InstanceId, CustomFloatIndex, CustomFloatValue);
		}
	}
}

void FGeometryCollectionISMPool::RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		
		for (int32 Index = 0; Index < InstanceGroup.Count; ++Index)
		{
			FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + Index];
			if (InstanceId.IsValid())
			{
				// todo: Could RemoveInstanceByIds() instead as long as that function can handle skipping invalid InstanceIds.
				ISM.ISMComponent->RemoveInstanceById(InstanceId);
			}
		}
		
#if DO_CHECK
		// clear the IDs
		for (int32 Index = 0; Index < InstanceGroup.Count; ++Index)
		{
			ISM.InstanceIds[InstanceGroup.Start + Index] = FPrimitiveInstanceId();
		}
#endif
		ISM.InstanceGroups.RemoveGroup(MeshInfo.InstanceGroupIndex);
	
		if (ISM.InstanceGroups.IsEmpty())
		{
			// No live instances, so take opportunity to reset indexing.
			ISM.InstanceGroups.Reset();
			ISM.InstanceIds.Reset();
		}

		if (GUseComponentFreeList && ISM.InstanceGroups.IsEmpty())
		{
			// Remove component and push this ISM slot to the free list.
			ensure(ISM.ISMComponent->PerInstanceSMData.Num() == 0);
			MeshToISMIndex.Remove(ISM.MeshInstance);
			FreeListISM.Add(MeshInfo.ISMIndex);

#if WITH_EDITOR
			ISM.ISMComponent->Rename(nullptr);
#endif
		}
	}
}

void FGeometryCollectionISMPool::Clear()
{
	MeshToISMIndex.Reset();
	FreeList.Reset();
	FreeListISM.Reset();
	if (ISMs.Num() > 0)
	{
		if (AActor* OwningActor = ISMs[0].ISMComponent->GetOwner())
		{
			for(FGeometryCollectionISM& ISM : ISMs)
			{
				ISM.ISMComponent->UnregisterComponent();
				ISM.ISMComponent->DestroyComponent();
				OwningActor->RemoveInstanceComponent(ISM.ISMComponent);
			}
		}
		ISMs.Reset();
	}
}

void FGeometryCollectionISMPool::GarbageCollect()
{
	// Release one component per call until we reach minimum pool size.
	if (FreeListISM.Num() > GComponentFreeListTargetSize)
	{
		const int32 ISMIndex = FreeListISM.Last();
		FreeListISM.RemoveAt(FreeListISM.Num() - 1);

		UInstancedStaticMeshComponent* ISM = ISMs[ISMIndex].ISMComponent;
		ISM->UnregisterComponent();
		ISM->DestroyComponent();
		ISM->GetOwner()->RemoveInstanceComponent(ISM);
		ISMs[ISMIndex].ISMComponent = nullptr;

		FreeList.Add(ISMIndex);
	}
}


UGeometryCollectionISMPoolComponent::UGeometryCollectionISMPoolComponent(const FObjectInitializer& ObjectInitializer)
	: NextMeshGroupId(0)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UGeometryCollectionISMPoolComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Pool.GarbageCollect();
}

UGeometryCollectionISMPoolComponent::FMeshGroupId  UGeometryCollectionISMPoolComponent::CreateMeshGroup(bool bAllowPerInstanceRemoval)
{
	FGeometryCollectionMeshGroup Group;
	Group.bAllowPerInstanceRemoval = bAllowPerInstanceRemoval;
	MeshGroups.Add(NextMeshGroupId, Group);
	return NextMeshGroupId++;
}

void UGeometryCollectionISMPoolComponent::DestroyMeshGroup(FMeshGroupId MeshGroupId)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		MeshGroup->RemoveAllMeshes(Pool);
		MeshGroups.Remove(MeshGroupId);
	}
}

UGeometryCollectionISMPoolComponent::FMeshId UGeometryCollectionISMPoolComponent::AddMeshToGroup(FMeshGroupId MeshGroupId, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		const FGeometryCollectionMeshInfo ISMInstanceInfo = Pool.AddISM(this, MeshInstance, InstanceCount, CustomDataFloats);
		return MeshGroup->AddMesh(MeshInstance, InstanceCount, ISMInstanceInfo);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to add a mesh to a mesh group (%d) that does not exists"), MeshGroupId);
	return INDEX_NONE;
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	return BatchUpdateInstancesTransforms(MeshGroupId, MeshId, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		return MeshGroup->BatchUpdateInstancesTransforms(Pool, MeshId, StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to update instance with mesh group (%d) that not exists"), MeshGroupId);
	return false;
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstanceCustomData(FMeshGroupId MeshGroupId, int32 CustomFloatIndex, float CustomFloatValue)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		MeshGroup->BatchUpdateInstanceCustomData(Pool, CustomFloatIndex, CustomFloatValue);
		return true;
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to update instance with mesh group (%d) that not exists"), MeshGroupId);
	return false;
}

void UGeometryCollectionISMPoolComponent::PreallocateMeshInstance(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	// If we are recycling components with a free list then we don't expect to have zero instance components.
	// So don't do preallocation of components either in that case.
	if (!GUseComponentFreeList)
{
		Pool.AddISM(this, MeshInstance);
	}
}

void UGeometryCollectionISMPoolComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	int32 SizeBytes =
		MeshGroups.GetAllocatedSize()
		+ Pool.MeshToISMIndex.GetAllocatedSize()
		+ Pool.ISMs.GetAllocatedSize()
		+ Pool.FreeList.GetAllocatedSize()
		+ Pool.FreeListISM.GetAllocatedSize();
	
	for (FGeometryCollectionISM ISM : Pool.ISMs)
	{
		SizeBytes += ISM.InstanceIds.GetAllocatedSize()
			+ ISM.InstanceGroups.GroupRanges.GetAllocatedSize()
			+ ISM.InstanceGroups.FreeList.GetAllocatedSize();
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SizeBytes);
}
