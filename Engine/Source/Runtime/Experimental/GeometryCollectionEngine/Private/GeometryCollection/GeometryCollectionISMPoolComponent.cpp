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
	FMeshId* MeshIndex = Meshes.Find(MeshInstance);
	if (MeshIndex)
	{
		return *MeshIndex;
	}

	const FMeshId MeshInfoIndex = MeshInfos.Emplace(ISMInstanceInfo);
	Meshes.Add(MeshInstance, MeshInfoIndex);
	return MeshInfoIndex;
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	return BatchUpdateInstancesTransforms(ISMPool, MeshId, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (MeshInfos.IsValidIndex(MeshId))
	{
		return ISMPool.BatchUpdateInstancesTransforms(MeshInfos[MeshId], StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
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
	Meshes.Empty();
}

void FGeometryCollectionISM::CreateISM(AActor* InOwningActor, bool bInUseHISM)
{
	check(InOwningActor);

	if (bInUseHISM)
	{
		ISMComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(InOwningActor, NAME_None, RF_Transient | RF_DuplicateTransient);
	}
	else
	{
		ISMComponent = NewObject<UInstancedStaticMeshComponent>(InOwningActor, NAME_None, RF_Transient | RF_DuplicateTransient);
	}

	ISMComponent->SetRemoveSwap();
	ISMComponent->SetCanEverAffectNavigation(false);
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

	ISMComponent->SetMobility((MeshInstance.Desc.Flags & FISMComponentDescription::StaticMobility) != 0 ? EComponentMobility::Static : EComponentMobility::Stationary);
	ISMComponent->SetCachedMaxDrawDistance(MeshInstance.Desc.EndCullDistance);
	ISMComponent->SetCullDistances(MeshInstance.Desc.StartCullDistance, MeshInstance.Desc.EndCullDistance);
	ISMComponent->SetCastShadow((MeshInstance.Desc.Flags & FISMComponentDescription::AffectShadow) != 0);
	ISMComponent->bAffectDynamicIndirectLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDynamicIndirectLighting) != 0;
	ISMComponent->bAffectDistanceFieldLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDistanceFieldLighting) != 0;
	ISMComponent->bWorldPositionOffsetWritesVelocity = (MeshInstance.Desc.Flags & FISMComponentDescription::WorldPositionOffsetWritesVelocity) != 0;
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

	FTransform ZeroScaleTransform;
	ZeroScaleTransform.SetIdentityZeroScale();
	TArray<FTransform> ZeroScaleTransforms;
	ZeroScaleTransforms.Init(ZeroScaleTransform, InstanceCount);

	ISMComponent->PreAllocateInstancesMemory(InstanceCount);
	TArray<int32> RenderInstances = ISMComponent->AddInstances(ZeroScaleTransforms, true, true);

	// Ensure that remapping arrays are big enough to hold any new items.
	InstanceIndexToRenderIndex.SetNum(InstanceGroups.GetMaxInstanceIndex(), false);
	RenderIndexToInstanceIndex.SetNum(ISMComponent->PerInstanceSMData.Num(), false);
	// Store mapping between our fixed instance index and the mutable ISM render index.
	// todo: Improve ISM API so that we don't need to pay the memory overhead here to manage this.
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		InstanceIndexToRenderIndex[NewInstanceGroup.Start + InstanceIndex] = RenderInstances[InstanceIndex];
		RenderIndexToInstanceIndex[RenderInstances[InstanceIndex]] = NewInstanceGroup.Start + InstanceIndex;
	}

	// Set any custom data.
	if (CustomDataFloats.Num())
	{
		const int32 NumCustomDataFloats = ISMComponent->NumCustomDataFloats;
		if (ensure(NumCustomDataFloats * InstanceCount == CustomDataFloats.Num()))
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				ISMComponent->SetCustomData(RenderInstances[InstanceIndex], CustomDataFloats.Slice(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats));
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
	const bool bIsHISM = (MeshInstance.Desc.Flags & FISMComponentDescription::UseHISM) != 0;

	FISMIndex ISMIndex = INDEX_NONE;
	if (bIsHISM && FreeListHISM.Num())
	{
		ISMIndex = FreeListHISM.Last();
		FreeListHISM.RemoveAt(FreeListHISM.Num() - 1);
	}
	else if (!bIsHISM && FreeListISM.Num())
	{
		ISMIndex = FreeListISM.Last();
		FreeListISM.RemoveAt(FreeListISM.Num() - 1);
	}
	else if (FreeList.Num())
	{
		ISMIndex = FreeList.Last();
		FreeList.RemoveAt(FreeList.Num() - 1);
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner(), bIsHISM);
	}
	else
	{
		ISMIndex = ISMs.AddDefaulted();
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner(), bIsHISM);
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

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	return BatchUpdateInstancesTransforms(MeshInfo, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		ensure((StartInstanceIndex + NewInstancesTransforms.Num()) <= InstanceGroup.Count);

		// If ISM component has identity transform (the common case) then we can skip world space to component space maths inside BatchUpdateInstancesTransforms()
		bWorldSpace &= !ISM.ISMComponent->GetComponentTransform().Equals(FTransform::Identity, 0.f);

		int32 StartIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start];
		int32 TransformIndex = 0;
		int32 BatchCount = 1;

		for (int InstanceIndex = StartInstanceIndex + 1; InstanceIndex < NewInstancesTransforms.Num(); ++InstanceIndex)
		{
			// Flush batch for non-sequential instances.
			int32 RenderIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
			if (RenderIndex != (StartIndex + BatchCount))
			{
				TArrayView<const FTransform> BatchedTransformsView = MakeArrayView(NewInstancesTransforms.GetData() + TransformIndex, BatchCount);
				ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchedTransformsView, bWorldSpace, bMarkRenderStateDirty, bTeleport);
				StartIndex = RenderIndex;
				TransformIndex += BatchCount;
				BatchCount = 0;
			}
			BatchCount++;
		}

		// last one
		TArrayView<const FTransform> BatchedTransformsView = MakeArrayView(NewInstancesTransforms.GetData() + TransformIndex, BatchCount);
		return ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchedTransformsView, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid ISM Id (%d) when updating the transform "), MeshInfo.ISMIndex);
	return false;
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
		const int32 RenderIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
		const bool bLastUpdate = InstanceIndex == InstanceGroup.Count - 1;
		ISM.ISMComponent->SetCustomDataValue(RenderIndex, CustomFloatIndex, CustomFloatValue, bLastUpdate);
	}
}

void FGeometryCollectionISMPool::RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		
		TArray<int32> InstancesToRemove;
		InstancesToRemove.SetNum(InstanceGroup.Count);
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceGroup.Count; ++InstanceIndex)
		{
			// We need render index to pass to the ISMComponent.
			InstancesToRemove[InstanceIndex] = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
			// Clear the stored render index since we're about to remove it.
			ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex] = -1;
		}

		// we sort the array on the spot because we use it after calling RemoveInstances to fix up our own indices
		InstancesToRemove.Sort(TGreater<int32>());
		constexpr bool bArrayAlreadySorted = true;
		ISM.ISMComponent->RemoveInstances(InstancesToRemove, bArrayAlreadySorted);

		// Fix up instance index remapping to match what will have happened in our ISM component in RemoveInstances()
		check(ISM.ISMComponent->SupportsRemoveSwap());
		for (int32 RenderIndex : InstancesToRemove)
		{
			ISM.RenderIndexToInstanceIndex.RemoveAtSwap(RenderIndex, 1, false);
			if (RenderIndex < ISM.RenderIndexToInstanceIndex.Num())
			{
				const int32 MovedInstanceIndex = ISM.RenderIndexToInstanceIndex[RenderIndex];
				ISM.InstanceIndexToRenderIndex[MovedInstanceIndex] = RenderIndex;
			}
		}

		ISM.InstanceGroups.RemoveGroup(MeshInfo.InstanceGroupIndex);
	
		if (ISM.InstanceGroups.IsEmpty())
		{
			// No live instances, so take opportunity to reset indexing.
			ISM.InstanceGroups.Reset();
			ISM.InstanceIndexToRenderIndex.Reset();
			ISM.RenderIndexToInstanceIndex.Reset();
		}

		if (GUseComponentFreeList && ISM.ISMComponent->PerInstanceSMData.Num() == 0)
		{
			// Remove component and push this ISM slot to the free list.
			MeshToISMIndex.Remove(ISM.MeshInstance);

			const bool bIsHISM = (ISM.MeshInstance.Desc.Flags & FISMComponentDescription::UseHISM) != 0;
			if (bIsHISM)
			{
				FreeListHISM.Add(MeshInfo.ISMIndex);
			}
			else
			{
				FreeListISM.Add(MeshInfo.ISMIndex);
			}

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
	FreeListHISM.Reset();
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
	const int32 NumFreeISMSlots = FreeListISM.Num();
	const int32 NumFreeHISMSlots = FreeListHISM.Num();

	if (NumFreeISMSlots + NumFreeHISMSlots > GComponentFreeListTargetSize)
	{
		int32 ISMIndex = INDEX_NONE;
		if (NumFreeHISMSlots >= NumFreeISMSlots)
		{
			ISMIndex = FreeListHISM.Last();
			FreeListHISM.RemoveAt(FreeListHISM.Num() - 1);
		}
		else
		{
			ISMIndex = FreeListISM.Last();
			FreeListISM.RemoveAt(FreeListISM.Num() - 1);
		}
		
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

UGeometryCollectionISMPoolComponent::FMeshGroupId  UGeometryCollectionISMPoolComponent::CreateMeshGroup()
{
	MeshGroups.Add(NextMeshGroupId);
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
		+ Pool.FreeListISM.GetAllocatedSize()
		+ Pool.FreeListHISM.GetAllocatedSize();
	
	for (FGeometryCollectionISM ISM : Pool.ISMs)
	{
		SizeBytes += ISM.InstanceIndexToRenderIndex.GetAllocatedSize()
			+ ISM.RenderIndexToInstanceIndex.GetAllocatedSize()
			+ ISM.InstanceGroups.GroupRanges.GetAllocatedSize()
			+ ISM.InstanceGroups.FreeList.GetAllocatedSize();
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SizeBytes);
}
