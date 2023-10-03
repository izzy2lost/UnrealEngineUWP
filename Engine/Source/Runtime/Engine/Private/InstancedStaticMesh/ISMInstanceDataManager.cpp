// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStaticMesh/ISMInstanceDataManager.h"
#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"
#include "Engine/InstancedStaticMesh.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Rendering/MotionVectorSimulation.h"
#include "SceneInterface.h"
#include "Rendering/RenderCommandPipes.h"

#define IDPROXY_ENABLE_ASYNC_TASK 1

#if 0
#define LOG_INST_DATA(_Format_, ...) UE_LOG(LogInstanceProxy, Log, _Format_, ##__VA_ARGS__)
#else
	#define LOG_INST_DATA(_Format_, ...) 
#endif

TAutoConsoleVariable<float> CVarInstanceUpdateTaskDebugDelay(
	TEXT("r.InstanceUpdateTaskDebugDelay"),
	0.0f,
	TEXT("Instance update debug delay in seconds."),
	ECVF_RenderThreadSafe);

FPrimitiveInstanceDataManager::FPrimitiveInstanceDataManager(UPrimitiveComponent* InPrimitiveComponent) 
	: PrimitiveComponent(InPrimitiveComponent) 
{
}

void FPrimitiveInstanceDataManager::SetMode(EMode InMode)
{
	if (InMode != Mode)
	{
		// This should never be called in mid-use
		check(GetMaxInstanceIndex() == 0);
		Invalidate(0);
	}
	Mode = InMode;
}

void FPrimitiveInstanceDataManager::Add(int32 InInstanceAddAtIndex, bool bInsert)
{
	ValidateMapping();

	// If the manager is marked for legacy-only mode, we should not see any tracking calls!
	check(Mode != EMode::ExternalLegacyData);

	// 1. determine if we need to enable explicit tracking, this happens when an instance is inserted.
	bool bIsActualInsert = bInsert && InInstanceAddAtIndex != GetMaxInstanceIndex();

	// Create explicit mapping if we need it now
	if (bIsActualInsert && HasIdentityMapping())
	{
		CreateExplicitIdentityMapping();
	}

	check(bInsert || InInstanceAddAtIndex == GetMaxInstanceIndex());

	MarkComponentRenderInstancesDirty();

	if (HasIdentityMapping())
	{
		int32 InstanceId = NumInstances++;
		MarkChangeHelper(AddedInstances, InstanceId);

		LOG_INST_DATA(TEXT("Add(IDX: %d, bInsert: %d) -> Id: %d"), InInstanceAddAtIndex, bInsert ? 1 : 0, InstanceId);
		return;
	}

	FPrimitiveInstanceId InstanceId{ValidInstanceIdMask.FindAndSetFirstZeroBit(IdSearchStartIndex)};
	if (!InstanceId.IsValid())
	{
		InstanceId = FPrimitiveInstanceId{ValidInstanceIdMask.Add(true)};
	}
	// Optimize search for next time
	IdSearchStartIndex = InstanceId.Id;
	IdToIndexMap.SetNumUninitialized(ValidInstanceIdMask.Num());

	// if these do not line up, then we are inserting an instance, this is a thing in the editor
	if (InInstanceAddAtIndex != IndexToIdMap.Num())
	{
		check(bInsert);
		IdToIndexMap[InstanceId.Id] = InInstanceAddAtIndex;
		LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), InstanceId.Id, InInstanceAddAtIndex);
		// first move all the existing data down one step by inserting the new one
		IndexToIdMap.Insert(InstanceId, InInstanceAddAtIndex);
		// then update all the relevant id->index mappings
		for (int32 Index = InInstanceAddAtIndex + 1; Index < IndexToIdMap.Num(); ++Index)
		{
			FPrimitiveInstanceId MovedId = IndexToIdMap[Index];
			IdToIndexMap[MovedId.Id] = Index;
			LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), MovedId.GetAsIndex(), Index);
			MarkIndexChanged(MovedId);
		}
	}
	else
	{
		int32 InstanceIndex = IndexToIdMap.Num();
		IdToIndexMap[InstanceId.Id] = InstanceIndex;
		LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), InstanceId.Id, InstanceIndex);

		IndexToIdMap.Add(InstanceId);
	}
	NumInstances = IndexToIdMap.Num();
	check(ValidInstanceIdMask.Num() >= NumInstances);
	MarkChangeHelper(AddedInstances, InstanceId);
	LOG_INST_DATA(TEXT("Add(IDX: %d, bInsert: %d) -> Id: %d"), InInstanceAddAtIndex, bInsert, InstanceId.Id);

	ValidateMapping();
}

void FPrimitiveInstanceDataManager::RemoveAtSwap(int32 InstanceIndex)
{
	ValidateMapping();

	check(Mode != EMode::ExternalLegacyData);

	// If the remove would cause reordering, we create the explicit mapping
	const bool bCausesReordering = InstanceIndex != NumInstances - 1;
	if (bCausesReordering && HasIdentityMapping())
	{
		CreateExplicitIdentityMapping();
	}

	MarkComponentRenderInstancesDirty();

	// If we still have the identity mapping, we must be removing the last item
	if (HasIdentityMapping())
	{
		check(!bCausesReordering);
		FPrimitiveInstanceId InstanceId {InstanceIndex};
		MarkRemoved(InstanceId);
		--NumInstances;
		LOG_INST_DATA(TEXT("RemoveAtSwap(IDX: %d) -> Id: %d"), InstanceIndex, InstanceId.Id);
		return;
	}

	FPrimitiveInstanceId InstanceId = IndexToIdMap[InstanceIndex];
	MarkRemoved(InstanceId);

	FPrimitiveInstanceId LastInstanceId = IndexToIdMap.Pop();
	NumInstances = IndexToIdMap.Num();
	check(ValidInstanceIdMask.Num() >= NumInstances);

	if (InstanceId != LastInstanceId)
	{
		IdToIndexMap[LastInstanceId.Id] = InstanceIndex;
		LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), LastInstanceId.Id, InstanceIndex);
		IndexToIdMap[InstanceIndex] = LastInstanceId;
		MarkIndexChanged(LastInstanceId);
	}
	ValidateMapping();
	LOG_INST_DATA(TEXT("RemoveAtSwap(IDX: %d) -> Id: %d"), InstanceIndex, InstanceId.Id);
}
	
void FPrimitiveInstanceDataManager::RemoveAt(int32 InstanceIndex)
{
	ValidateMapping();

	check(Mode != EMode::ExternalLegacyData);
		
	const bool bCausesReordering = InstanceIndex != NumInstances - 1;
	if (bCausesReordering && HasIdentityMapping())
	{
		CreateExplicitIdentityMapping();
	}

	MarkComponentRenderInstancesDirty();

	// If we still have the identity mapping, do the simplified tracking update
	if (HasIdentityMapping())
	{
		check(!bCausesReordering);
		FPrimitiveInstanceId InstanceId {InstanceIndex};
		MarkRemoved(InstanceId);
		--NumInstances;
		LOG_INST_DATA(TEXT("RemoveAt(IDX: %d) -> Id: %d"), InstanceIndex, InstanceId.Id);
		return;
	}

	FPrimitiveInstanceId InstanceId = IndexToIdMap[InstanceIndex];
	MarkRemoved(InstanceId);

	if (InstanceIndex == IndexToIdMap.Num() - 1)
	{
		IndexToIdMap.SetNum(InstanceIndex);
	}
	else
	{
		IndexToIdMap.RemoveAt(InstanceIndex);
		for (int32 Index = InstanceIndex; Index < IndexToIdMap.Num(); ++Index)
		{
			FPrimitiveInstanceId MovedId = IndexToIdMap[Index];
			IdToIndexMap[MovedId.Id] = Index;
			LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), MovedId.GetAsIndex(), Index);
			MarkIndexChanged(MovedId);
		}
	}
	NumInstances = IndexToIdMap.Num();
	check(ValidInstanceIdMask.Num() >= NumInstances);
	LOG_INST_DATA(TEXT("RemoveAt(IDX: %d) -> Id: %d"), InstanceIndex, InstanceId.Id);

	ValidateMapping();
}

void FPrimitiveInstanceDataManager::TransformChanged(int32 InstanceIndex)
{
	LOG_INST_DATA(TEXT("TransformChanged(IDX: %d)"), InstanceIndex);
	MarkChangeHelper(TransformChangedInstances, InstanceIndex);
}

void FPrimitiveInstanceDataManager::TransformChanged(FPrimitiveInstanceId InstanceId)
{
	LOG_INST_DATA(TEXT("TransformChanged(ID: %d)"), InstanceId.Id);
	MarkChangeHelper(TransformChangedInstances, InstanceId);
}
void FPrimitiveInstanceDataManager::TransformsChangedAll()
{
	LOG_INST_DATA(TEXT("TransformsChangedAll(%s)"), TEXT(""));
	bTransformChangedAllInstances = true;
	MarkComponentRenderInstancesDirty();
}

void FPrimitiveInstanceDataManager::CustomDataChanged(int32 InstanceIndex)
{
	LOG_INST_DATA(TEXT("CustomDataChanged(IDX: %d)"), InstanceIndex);
	MarkChangeHelper(CustomDataChangedInstances, InstanceIndex);
}

void FPrimitiveInstanceDataManager::BakedLightingDataChanged(int32 InstanceIndex)
{
	LOG_INST_DATA(TEXT("BakedLightingDataChanged(IDX: %d)"), InstanceIndex);
	MarkChangeHelper(BakedLightingDataChangedInstances, InstanceIndex);
}

void FPrimitiveInstanceDataManager::BakedLightingDataChangedAll()
{
	LOG_INST_DATA(TEXT("BakedLightingDataChangedAll(%s)"), TEXT(""));
	bBakedLightingDataChanged = true;
	MarkComponentRenderInstancesDirty();
}

void FPrimitiveInstanceDataManager::NumCustomDataChanged()
{
	LOG_INST_DATA(TEXT("NumCustomDataChanged(%s)"), TEXT(""));
	bNumCustomDataChanged = true;
	MarkComponentRenderInstancesDirty();
}

#if WITH_EDITOR

void FPrimitiveInstanceDataManager::EditorDataChangedAll()
{
	LOG_INST_DATA(TEXT("EditorDataChangedAll(%s)"), TEXT(""));
	bAnyEditorDataChanged = true;
	MarkComponentRenderInstancesDirty();
}

#endif

void FPrimitiveInstanceDataManager::PrimitiveTransformChanged()
{
	LOG_INST_DATA(TEXT("PrimitiveTransformChanged(%s)"), TEXT(""));
	bPrimitiveTransformChanged = true;
	MarkComponentRenderInstancesDirty();
}

bool FPrimitiveInstanceDataManager::HasAnyInstanceChanges() const
{
	return !AddedInstances.IsEmpty() 
#if WITH_EDITOR
		|| bAnyEditorDataChanged 
#endif
		|| bNumCustomDataChanged || bBakedLightingDataChanged || bTransformChangedAllInstances;
}


void FPrimitiveInstanceDataManager::SerializeRenderData(FArchive& Ar, bool bCooked)
{
	if (bCooked)
	{
		bool bHasCookedData = false;//CachedCookedData != nullptr;
		Ar << bHasCookedData;
#if 0
		if (bHasCookedData)
		{
			if (Ar.IsLoading())
			{
				CachedCookedData = new FInstanceSceneDataBuffers;
			}
			CachedCookedData->Serialize(Ar);
		}
#endif
	}
}

/**
 * Describes what has changed, that can be derived from the primitive desc, or internal tracking state.
 */
union FChangeDesc 
{
	struct 
	{
		uint8 bUntrackedState : 1;
		uint8 bInstancesChanged : 1;
		uint8 bPrimitiveTransformChanged : 1;
		uint8 bMaterialUsageFlagsChanged : 1;
		uint8 bMaxDisplacementChanged : 1;
		uint8 bStaticMeshBoundsChanged : 1;
	};
	uint8 Packed;
	
	FChangeDesc()
	{
		Packed = 0u;
	}

};

template <typename TaskLambdaType>
void FPrimitiveInstanceDataManager::BeginUpdateTask(FInstanceDataUpdateTaskInfo &InstanceDataUpdateTaskInfo, TaskLambdaType &&TaskLambda, const FInstanceDataBufferHeader &InInstanceDataBufferHeader)
{
	// Make sure any previous tasks are done.
	InstanceDataUpdateTaskInfo.WaitForUpdateCompletion();
	InstanceDataUpdateTaskInfo.InstanceDataBufferHeader = InInstanceDataBufferHeader;

#if IDPROXY_ENABLE_ASYNC_TASK
	auto OuterTaskFunc = [TaskLambda = MoveTemp(TaskLambda)]() mutable
	{
		const float DebugDelay = CVarInstanceUpdateTaskDebugDelay.GetValueOnAnyThread();
		if (DebugDelay != 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Instance update debug delay %5.1fs (CVar r.InstanceUpdateTaskDebugDelay)"), DebugDelay);
			FPlatformProcess::Sleep(DebugDelay);
		}

		TaskLambda();
	};
	InstanceDataUpdateTaskInfo.UpdateTaskHandle = UE::Tasks::Launch(TEXT("FInstanceDataUpdateTaskInfo::BeginUpdateTask"), MoveTemp(OuterTaskFunc));
#else
	InstanceDataUpdateTaskInfo.UpdateTaskHandle = UE::Tasks::FTask();
	TaskLambda();
#endif
}

template <typename TaskLambdaType>
void FPrimitiveInstanceDataManager::DispatchUpdateTask(bool bIsUnattached, const FInstanceDataBufferHeader &InstanceDataBufferHeader, TaskLambdaType &&TaskLambda)
{
	FInstanceDataUpdateTaskInfo *InstanceDataUpdateTaskInfo = Proxy->GetUpdateTaskInfo();
#if DO_CHECK
	auto OuterTaskLambda = [InnerTaskLambda = MoveTemp(TaskLambda), InstanceDataBufferHeader, Proxy = Proxy]() mutable
	{
		FInstanceDataUpdateTaskInfo *InstanceDataUpdateTaskInfo = Proxy->GetUpdateTaskInfo();
		check(!InstanceDataUpdateTaskInfo || InstanceDataUpdateTaskInfo->GetHeader() == InstanceDataBufferHeader);
		InnerTaskLambda();
		check(!InstanceDataUpdateTaskInfo || InstanceDataUpdateTaskInfo->GetHeader() == InstanceDataBufferHeader);
		// check(InstanceDataBufferHeader.Flags == Proxy->GetData().GetFlags());
		const FInstanceDataFlags HeaderFlags = InstanceDataBufferHeader.Flags;
		const bool bHasAnyPayloadData = HeaderFlags.bHasPerInstanceHierarchyOffset || HeaderFlags.bHasPerInstanceLocalBounds || HeaderFlags.bHasPerInstanceDynamicData || HeaderFlags.bHasPerInstanceLMSMUVBias || HeaderFlags.bHasPerInstanceCustomData || HeaderFlags.bHasPerInstancePayloadExtension || HeaderFlags.bHasPerInstanceEditorData;
		check(bHasAnyPayloadData || InstanceDataBufferHeader.PayloadDataStride == 0);
		check(!bHasAnyPayloadData || InstanceDataBufferHeader.PayloadDataStride != 0);
	};
#else
	TaskLambdaType OuterTaskLambda = MoveTemp(TaskLambda);
#endif
	// Dispatch from any thread.
	if (bIsUnattached)
	{
		if (InstanceDataUpdateTaskInfo)
		{
			BeginUpdateTask(*InstanceDataUpdateTaskInfo, MoveTemp(OuterTaskLambda), InstanceDataBufferHeader);
		}
		else
		{
			OuterTaskLambda();
		}
	}
	else
	{
		// Mutating an existing data, must dispatch from RT (such that it does not happen mid-frame).
		// (One could imagine other scheduling mechanisms)
		ENQUEUE_RENDER_COMMAND(UpdateInstanceProxyData)(UE::RenderCommandPipe::Scene, 
			[InstanceDataUpdateTaskInfo, 
			InstanceDataBufferHeader, 
			OuterTaskLambda = MoveTemp(OuterTaskLambda)](FRHICommandList& RHICmdList) mutable
		{
			if (InstanceDataUpdateTaskInfo)
			{
				BeginUpdateTask(*InstanceDataUpdateTaskInfo, MoveTemp(OuterTaskLambda), InstanceDataBufferHeader);
			}
			else
			{
				OuterTaskLambda();
			}
		});
	}
}

struct FExternalGenericChangeSet
{
	FRenderBounds InstanceLocalBounds;
	FVector PrimitiveWorldSpaceOffset;
	FRenderTransform PrimitiveToRelativeWorld;
	FInstanceDataFlags Flags;
	float AbsMaxDisplacement = 0.0f;
#if WITH_EDITOR
	TPimplPtr<FOpaqueHitProxyContainer> HitProxyContainer;
#endif
	FPrimitiveInstanceDataManager::FExternalUpdateData ExternalUpdateData;
};

void FPrimitiveInstanceDataManager::InitChangeSet(const FChangeDesc &ChangeDesc, const FInstanceUpdateComponentDesc &ComponentData, FISMInstanceUpdateChangeSet &ChangeSet)
{
	if (ChangeSet.IsFullUpdate())
	{
		// Initialize to full update collection
		ChangeSet.CustomDataDelta 
#if WITH_EDITOR
			= ChangeSet.InstanceEditorDataDelta 
#endif
			= ChangeSet.InstanceLightShadowUVBiasDelta 
			= ChangeSet.TransformsDelta 
			= FArrayIndexDelta(GetMaxInstanceIndex());
	}
	else
	{
		// TODO: It is moderately efficient to iterate all the bits multiple times... may need to use a different scheme anyway,
		//       maybe collect set of "anything changed" indexes.

		// If the primitive transform changed, or all instances changed, we want to send all the transforms since there is no proxy side storage of the pre-transformed ones.
		bool bSendAllTransforms = ChangeDesc.bPrimitiveTransformChanged || bTransformChangedAllInstances;
		// skip the delta collection if none is used
		bool bAnyAddDeltaUsed = !(bSendAllTransforms  && bNumCustomDataChanged  && bBakedLightingDataChanged);
		if (bAnyAddDeltaUsed)
		{
			for (TConstSetBitIterator<> BitIt(AddedInstances); BitIt; ++BitIt)
			{
				int32 Index = IdToIndex(FPrimitiveInstanceId{BitIt.GetIndex()});
				check(Index != INDEX_NONE);
				if (!ChangeDesc.bPrimitiveTransformChanged)
				{
					ChangeSet.TransformsDelta.AddIndex(Index);
				}

				if (!bNumCustomDataChanged)
				{
					ChangeSet.CustomDataDelta.AddIndex(Index);
				}

				if (!bBakedLightingDataChanged)
				{
					ChangeSet.InstanceLightShadowUVBiasDelta.AddIndex(Index);
				}
				//ChangeSet.InstanceEditorDataDelta.AddIndex(Index);
			}
		}
#if WITH_EDITOR
		// Bulk update editor data - the hit-proxies are recreated all the time.
		ChangeSet.InstanceEditorDataDelta = FArrayIndexDelta(GetMaxInstanceIndex());
#endif
		if (bSendAllTransforms)
		{
			ChangeSet.TransformsDelta = FArrayIndexDelta(ComponentData.NumSourceInstances);
		}
		else
		{
			for (TConstSetBitIterator<> BitIt(TransformChangedInstances); BitIt; ++BitIt)
			{
				FPrimitiveInstanceId Id{BitIt.GetIndex()};
				if (!AddedInstances[Id] && !RemovedInstances[Id])
				{
					int32 Index = IdToIndex(Id);
					check(Index != INDEX_NONE);
					ChangeSet.TransformsDelta.AddIndex(Index);
				}
			}
		}

		// Update all the custom data if there was a change in number of floats (this resets the whole thing).
		if (bNumCustomDataChanged)
		{
			ChangeSet.CustomDataDelta = FArrayIndexDelta(GetMaxInstanceIndex());
		}
		else
		{
			for (TConstSetBitIterator<> BitIt(CustomDataChangedInstances); BitIt; ++BitIt)
			{
				FPrimitiveInstanceId Id{BitIt.GetIndex()};
				if (!AddedInstances[Id] && !RemovedInstances[Id])
				{
					int32 Index = IdToIndex(Id);
					check(Index != INDEX_NONE);
					ChangeSet.CustomDataDelta.AddIndex(Index);
				}
			}
		}

		if (bBakedLightingDataChanged)
		{
			ChangeSet.InstanceLightShadowUVBiasDelta = FArrayIndexDelta(GetMaxInstanceIndex());
		}
		else
		{
			for (TConstSetBitIterator<> BitIt(BakedLightingDataChangedInstances); BitIt; ++BitIt)
			{
				FPrimitiveInstanceId Id{BitIt.GetIndex()};
				if (!AddedInstances[Id] && !RemovedInstances[Id])
				{
					int32 Index = IdToIndex(Id);
					check(Index != INDEX_NONE);
					ChangeSet.InstanceLightShadowUVBiasDelta.AddIndex(Index);
				}
			}
		}
	}

	// Move & reset tracked state
	ChangeSet.ChangeMask.AddedInstances = MoveTemp(AddedInstances);
	ChangeSet.ChangeMask.RemovedInstances = MoveTemp(RemovedInstances);
	ChangeSet.ChangeMask.TransformChangedInstances = MoveTemp(TransformChangedInstances);
	ChangeSet.ChangeMask.CustomDataChangedInstances = MoveTemp(CustomDataChangedInstances);
	ChangeSet.ChangeMask.bNumCustomDataChanged = bNumCustomDataChanged;
	ChangeSet.ChangeMask.bBakedLightingDataChanged = bBakedLightingDataChanged;

#if WITH_EDITOR
	ChangeSet.ChangeMask.bAnyEditorDataChanged = bAnyEditorDataChanged;
	bAnyEditorDataChanged = false;
#endif	
	ChangeSet.ChangeMask.IndexChangeInstances = MoveTemp(IndexChangeInstances);

	bTransformChangedAllInstances = false;
	bNumCustomDataChanged = false;
	bBakedLightingDataChanged = false;

	ChangeSet.PrimitiveWorldSpaceOffset = FLargeWorldRenderPosition(PrimitiveLocalToWorld.GetOrigin()).GetTileOffset();
	ChangeSet.PrimitiveToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(ChangeSet.PrimitiveWorldSpaceOffset, PrimitiveLocalToWorld);
	ChangeSet.Flags = Flags;
	ChangeSet.AbsMaxDisplacement = AbsMaxDisplacement;
	if (!ChangeSet.Flags.bHasPerInstanceCustomData)
	{
		ChangeSet.NumCustomDataFloats = 0;
		ChangeSet.CustomDataDelta = FArrayIndexDelta();
	}
	ChangeSet.InstanceIdIndexMap = FInstanceIdIndexMap(*this);

	// This is the odd one out
	ChangeSet.SetInstanceLocalBounds(ComponentData.StaticMeshBounds);
}

bool FPrimitiveInstanceDataManager::FlushChanges(FInstanceUpdateComponentDesc &&ComponentData, bool bNewPrimitiveProxy)
{
	// Always clear the flag such that any subsequent change marks it as needing update again.
	bComponentMarkedDirty = false;

	// No point updating data if there is no owning proxy that might use the data.
	if (ComponentData.PrimitiveSceneProxy == nullptr)
	{
		return false;
	}

	// Can't update if there is no proxy, if it was destroyed we must wait for a new scene proxy to come around and request a new one to be created.
	if (!Proxy)
	{
		return false;
	}

	// Determine if the instance data proxy is exposed to the RT such that we can do early dispatch, this is important for level loading, probably, where we could move the work earlier.
	// In fact, we could, e.g., for static ISMs kick this directly in post load perhaps.
	const bool bIsUnattached = bNewPrimitiveProxy && Proxy->bIsNew;
	Proxy->bIsNew = false;

	EShaderPlatform ShaderPlatform = ComponentData.PrimitiveSceneProxy->GetScene().GetShaderPlatform();
	ERHIFeatureLevel::Type FeatureLevel = ComponentData.PrimitiveSceneProxy->GetScene().GetFeatureLevel();
	check(Proxy->CheckPlatformFeatureLevel(ShaderPlatform, FeatureLevel));

	// Marked for externally managed update, this may be combined with tracked changes (which results in double updates)
	if (ExternalBuildUpdateData)
	{
		LOG_INST_DATA(TEXT("Rebuild with EState::External %s"), TEXT(""));

		FExternalGenericChangeSet ExternalChangeSet;
		ExternalChangeSet.PrimitiveWorldSpaceOffset = FLargeWorldRenderPosition(PrimitiveLocalToWorld.GetOrigin()).GetTileOffset();
		ExternalChangeSet.PrimitiveToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(ExternalChangeSet.PrimitiveWorldSpaceOffset, PrimitiveLocalToWorld);
		ExternalChangeSet.InstanceLocalBounds = ComponentData.StaticMeshBounds;
		ExternalChangeSet.Flags = ComponentData.Flags;

		TArray<TRefCountPtr<HHitProxy>> HitProxies;
		ExternalChangeSet.ExternalUpdateData = ExternalBuildUpdateData(HitProxies);
#if WITH_EDITOR
		ExternalChangeSet.HitProxyContainer = MakeOpaqueHitProxyContainer(HitProxies);
#endif

		ExternalChangeSet.AbsMaxDisplacement = AbsMaxDisplacement;
		
		// Make sure we have a tracking state that matches the number of instances, but doesn't retain any history
		ClearIdTracking(ExternalChangeSet.ExternalUpdateData.NumInstances);

		// Need to store this off for tracking purposes
		NumCustomDataFloats = ExternalChangeSet.ExternalUpdateData.NumCustomDataFloats;

		// Assemble header info to enable nonblocking primitive update.
		FInstanceDataBufferHeader InstanceDataBufferHeader;
		InstanceDataBufferHeader.NumInstances = NumInstances;
		InstanceDataBufferHeader.PayloadDataStride = FInstanceSceneDataBuffers::CalcPayloadDataStride(ExternalChangeSet.Flags, ExternalChangeSet.ExternalUpdateData.NumCustomDataFloats, 0);
		InstanceDataBufferHeader.Flags = ExternalChangeSet.Flags;

		DispatchUpdateTask(bIsUnattached, InstanceDataBufferHeader,
			[ExternalChangeSet = MoveTemp(ExternalChangeSet), 
			Proxy = Proxy,
			InstanceIdIndexMap = FInstanceIdIndexMap(*this)] () mutable 
		{
			FISMCInstanceDataSceneProxy &ProxyRef = *Proxy;
			
			// Forcibly destroy any tracking state, if the external entity manages this it can track the data on its own.
			ProxyRef.ChangeMask.Reset();
			ProxyRef.InstanceIdIndexMap = MoveTemp(InstanceIdIndexMap);
#if WITH_EDITOR
			ProxyRef.HitProxyContainer = MoveTemp(ExternalChangeSet.HitProxyContainer);
#endif
			FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(&ProxyRef));
			FInstanceSceneDataBuffers::FWriteView WriteView = ProxyRef.InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
			WriteView.Flags = ExternalChangeSet.Flags;
			WriteView.PrimitiveToRelativeWorld = ExternalChangeSet.PrimitiveToRelativeWorld;
			WriteView.PrimitiveWorldSpaceOffset = ExternalChangeSet.PrimitiveWorldSpaceOffset;
			ProxyRef.InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

			// TODO: DISP - Fix me (this comment came along from FPrimitiveSceneProxy::SetInstanceLocalBounds and is probably still true...)
			const FVector3f PadExtent = FISMCInstanceDataSceneProxy::GetLocalBoundsPadExtent(ExternalChangeSet.PrimitiveToRelativeWorld, ExternalChangeSet.AbsMaxDisplacement);
			ExternalChangeSet.InstanceLocalBounds.Min -= PadExtent;
			ExternalChangeSet.InstanceLocalBounds.Max += PadExtent;

			ExternalChangeSet.ExternalUpdateData.UpdateProxy(ProxyRef, ExternalChangeSet.InstanceLocalBounds);

			check(ProxyRef.GetData().GetNumInstances() == InstanceIdIndexMap.GetMaxInstanceIndex());
			check(ProxyRef.GetUpdateTaskInfo()->GetHeader().NumInstances == InstanceIdIndexMap.GetMaxInstanceIndex());
			check(ProxyRef.GetUpdateTaskInfo()->GetHeader().Flags == ProxyRef.InstanceSceneDataBuffers.GetFlags());
		});
	}

	FChangeDesc ChangeDesc;

	// Note: this must be supplied to allow calculating correct bounds for each instance, currently this is derived in the primitive proxy ctor, meaning it should be fetched from the proxy
	//       However, we may want to remove this coupling.
	float NewAbsMaxDisplacement = ComponentData.PrimitiveSceneProxy->GetAbsMaxDisplacement();


	// TODO: We may decide to do so if other conditions are met (e.g., large change-set or marked for full invalidation).
	// TODO: Need to figure this out in some other way, e.g., attachment counter or whatnot, since the creation has been moved up we no longer know if this is a fresh one.
	ChangeDesc.bUntrackedState = GetState() != ETrackingState::Tracked;

	// Figure out the deltas.
	if (!ChangeDesc.bUntrackedState)
	{
		ChangeDesc.bInstancesChanged =  HasAnyInstanceChanges();

		ChangeDesc.bPrimitiveTransformChanged = bPrimitiveTransformChanged || !ComponentData.PrimitiveLocalToWorld.Equals(PrimitiveLocalToWorld);
		ChangeDesc.bMaterialUsageFlagsChanged = Flags != ComponentData.Flags;
		ChangeDesc.bMaxDisplacementChanged = AbsMaxDisplacement != NewAbsMaxDisplacement;
		ChangeDesc.bStaticMeshBoundsChanged = !StaticMeshBounds.Equals(ComponentData.StaticMeshBounds);
	}

	FMatrix PrevPrimitiveLocalToWorld = PrimitiveLocalToWorld;
	// Update the tracked state
	PrimitiveLocalToWorld = ComponentData.PrimitiveLocalToWorld;
	AbsMaxDisplacement = NewAbsMaxDisplacement;
	StaticMeshBounds = ComponentData.StaticMeshBounds;
	Flags = ComponentData.Flags;

	const bool bAnyChange = ChangeDesc.Packed != 0;

	if (!bAnyChange)
	{
		return false;
	}
	
	// 
	{
		// TODO: Maybe specialize for only bPrimitiveTransformChanged (no need to send/modify anything _other_ than the transform data)
		// TODO: The states other than bCreateNewProxy _can_ be handled through delta updates (e.g., add instance + offset all the rest).
		bool bNeedFullUpdate = ChangeDesc.bUntrackedState || ChangeDesc.bMaterialUsageFlagsChanged;

		// TODO: fix this up somehow: can probably remove now since we now propagate the count with invalidations & just leave a check
		// In this case, here's where we found out the true count of instances, since before it's not been tracked.
		if (ChangeDesc.bUntrackedState)
		{
			check(ComponentData.NumSourceInstances >= 0);
			NumInstances = ComponentData.NumSourceInstances;
		}

		FISMInstanceUpdateChangeSet ChangeSet(bNeedFullUpdate);

		// Initialize the change set before collecting instance change data.
		InitChangeSet(ChangeDesc, ComponentData, ChangeSet);

		// Callback to the owner to fill in change data.
		ComponentData.BuildChangeSet(ChangeSet);
		
		// make sure the custom data change is correctly tracked
		check(bNeedFullUpdate || bNumCustomDataChanged || NumCustomDataFloats == ChangeSet.NumCustomDataFloats);
		NumCustomDataFloats = ChangeSet.NumCustomDataFloats;
		check(ChangeSet.Flags.bHasPerInstanceCustomData || ChangeSet.CustomDataDelta.IsEmpty() && ChangeSet.PerInstanceCustomData.IsEmpty());

		// If we have per-instance previous local to world, they are expected to be in the local space of the _previous_ local to world. If they are in fact not (e.g., if someone sets them explicitly from world space) then, well, this won't be correct
		if (ChangeSet.Flags.bHasPerInstanceDynamicData)
		{
			TOptional<FTransform> PreviousTransform = FMotionVectorSimulation::Get().GetPreviousTransform(PrimitiveComponent.Get());
			if (PreviousTransform.IsSet())
			{
				ChangeSet.PreviousPrimitiveToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(ChangeSet.PrimitiveWorldSpaceOffset, PreviousTransform.GetValue().ToMatrixWithScale());
			}
			else
			{
				ChangeSet.PreviousPrimitiveToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(ChangeSet.PrimitiveWorldSpaceOffset, PrevPrimitiveLocalToWorld);				
			}
		}

		// Assemble header info to enable nonblocking primitive update.
		FInstanceDataBufferHeader InstanceDataBufferHeader;

		// Too many numbers, make sure they line up.
		if (Mode == EMode::Default)
		{
			check(ChangeSet.LegacyInstanceReorderTable.IsEmpty());
			check(ComponentData.NumSourceInstances == GetMaxInstanceIndex());
			check(ComponentData.NumProxyInstances == GetMaxInstanceIndex());
		}

		ChangeSet.PostUpdateNumInstances = ComponentData.NumProxyInstances;
		InstanceDataBufferHeader.NumInstances = ComponentData.NumProxyInstances;
		InstanceDataBufferHeader.PayloadDataStride = FInstanceSceneDataBuffers::CalcPayloadDataStride(ChangeSet.Flags, ChangeSet.NumCustomDataFloats, 0);
		InstanceDataBufferHeader.Flags = ChangeSet.Flags;


		// Special handling for the case of external data & transform change, may want to extend this to others (but need to send data in that case)
		FChangeDesc OnlyPrimitiveTransformChangeDesc;
		OnlyPrimitiveTransformChangeDesc.bPrimitiveTransformChanged = true;
		if (Mode == EMode::ExternalLegacyData && ChangeDesc.Packed == OnlyPrimitiveTransformChangeDesc.Packed)
		{
			LOG_INST_DATA(TEXT("Primitive Transform Update %s"), TEXT(""));
			DispatchUpdateTask(bIsUnattached, InstanceDataBufferHeader, [ChangeSet = MoveTemp(ChangeSet), Proxy = Proxy] () mutable 
			{
				Proxy->UpdatePrimitiveTransform(MoveTemp(ChangeSet));
			});
		}
		else if (bNeedFullUpdate)
		{
			LOG_INST_DATA(TEXT("Full Build %s"), TEXT(""));
			DispatchUpdateTask(bIsUnattached, InstanceDataBufferHeader, [ChangeSet = MoveTemp(ChangeSet), Proxy = Proxy] () mutable 
			{
				Proxy->Build(MoveTemp(ChangeSet));
			});
		}
		else
		{
			LOG_INST_DATA(TEXT("Delta Update %s"), TEXT(""));
			check(!ChangeDesc.bUntrackedState);
			DispatchUpdateTask(bIsUnattached, InstanceDataBufferHeader, [ChangeSet = MoveTemp(ChangeSet), Proxy = Proxy] () mutable 
			{
				Proxy->Update(MoveTemp(ChangeSet));
			});
		}
	}

	// After an update has been sent, we need to track all deltas.
	TrackingState = ETrackingState::Tracked;

	return true;
}

void FPrimitiveInstanceDataManager::PostLoad(int32 InNumInstances, TUniquePtr<FStaticMeshInstanceData> &&InStaticMeshInstanceData)
{
	if (LegacyStaticMeshInstanceData.IsValid())
	{
		check(NumInstances == LegacyStaticMeshInstanceData->GetNumInstances());
		// kick off conversion job to extract the needed data? When do we need what?

		// Not sure about this whole thing... when do we need it, should it belong with the HISM implementation (probably)
#ifdef NEW_INSTANCE_DATA_PATH_TODO
		MarkForRebuildFromExternal([
			LegacyInstanceData = MoveTemp(LegacyStaticMeshInstanceData)] (TArray<TRefCountPtr<HHitProxy>> &OutHitProxies) mutable
		{
			// TODO: Figure out the flow of these things again.
			// NEW_INSTANCE_DATA_PATH_TODO: Need to differentiate wrt HISM such that we can send along the reorder table if needed...

			OutHitProxies.Reset(); 
			FPrimitiveInstanceDataManager::FExternalUpdateData ExternalUpdateData;
			ExternalUpdateData.NumInstances = LegacyInstanceData ? LegacyInstanceData->GetNumInstances() : 0;
			ExternalUpdateData.UpdateProxy = [LegacyInstanceDataInner = MoveTemp(LegacyInstanceData)](FInstanceDataSceneProxy &InstanceDataSceneProxy, const FRenderBounds &InstanceLocalBounds) mutable
			{
				// No reorder table needed since we never perform delta updates (is that true? how do we know that?).
				InstanceDataSceneProxy.BuildFromLegacyData(MoveTemp(LegacyInstanceDataInner), InstanceLocalBounds, TArray<int32>());
			};
			return ExternalUpdateData;
		});
#endif
	}
	NumInstances = InNumInstances;
}

void FPrimitiveInstanceDataManager::ClearIdTracking(int32 InNumInstances)
{
	// Reset the mapping to identity & clear allocator, this looses all association with ID:Index that existed before
	IndexToIdMap.Empty();
	IdToIndexMap.Empty();
	NumInstances = InNumInstances;
	ValidInstanceIdMask.Empty();
	IdSearchStartIndex = 0;

	// Also clear the change tracking since it is not valid anymore
	ClearChangeTracking();
}

void FPrimitiveInstanceDataManager::ClearChangeTracking()
{
	// When tracking data is cleared, we loose connection to previously tracked state until the next update is sent.
	TrackingState = ETrackingState::Initial;

	ExternalBuildUpdateData.Reset();

	AddedInstances.Empty();
	RemovedInstances.Empty();
	TransformChangedInstances.Empty();
	CustomDataChangedInstances.Empty();
	bNumCustomDataChanged = false;
	bBakedLightingDataChanged = false;
	bTransformChangedAllInstances = false;

	BakedLightingDataChangedInstances.Empty();
#if WITH_EDITOR
	bAnyEditorDataChanged = false;
#endif	
	bPrimitiveTransformChanged = false;
	IndexChangeInstances.Empty();
}

int32 FPrimitiveInstanceDataManager::GetMaxInstanceId() const
{
	return HasIdentityMapping() ? NumInstances : ValidInstanceIdMask.Num();
}

int32 FPrimitiveInstanceDataManager::GetMaxInstanceIndex() const
{
	return HasIdentityMapping() ? NumInstances : IndexToIdMap.Num();
}

bool FPrimitiveInstanceDataManager::ResizeChangeTrackingState()
{
	if (GetState() == ETrackingState::Tracked)
	{
		int32 MaxInstanceId = GetMaxInstanceId();

		// Grow to fit the new max
		if (AddedInstances.Num() < MaxInstanceId)
		{
			AddedInstances.SetNum(MaxInstanceId, false);
			RemovedInstances.SetNum(MaxInstanceId, false);
			IndexChangeInstances.SetNum(MaxInstanceId, false);
			TransformChangedInstances.SetNum(MaxInstanceId, false);
			CustomDataChangedInstances.SetNum(MaxInstanceId, false);
			BakedLightingDataChangedInstances.SetNum(MaxInstanceId, false);
#if WITH_EDITOR
			//bAnyEditorDataChanged = false;
#endif	
		}
		return true;
	}
	return false;
}

void FPrimitiveInstanceDataManager::CreateExplicitIdentityMapping()
{
	check(HasIdentityMapping());
	IndexToIdMap.SetNumUninitialized(NumInstances);
	IdToIndexMap.SetNumUninitialized(NumInstances);
	for (int32 Index = 0; Index < NumInstances; ++Index)
	{
		IndexToIdMap[Index] = FPrimitiveInstanceId{Index};
		IdToIndexMap[Index] = Index;
	}
	ValidInstanceIdMask.Reset();
	ValidInstanceIdMask.SetNum(NumInstances, true);
	IdSearchStartIndex = NumInstances;

	ResizeChangeTrackingState();
}

void FPrimitiveInstanceDataManager::MarkChangeHelper(FChangeBitArray& TrackingArray, int32 InstanceIndex)
{
	check(Mode != EMode::ExternalLegacyData);

	if (GetState() != ETrackingState::Tracked)
	{
		MarkComponentRenderInstancesDirty();
		return;
	}
	MarkChangeHelper(TrackingArray, IndexToId(InstanceIndex));
}

void FPrimitiveInstanceDataManager::MarkChangeHelper(FChangeBitArray& TrackingArray, FPrimitiveInstanceId InstanceId)
{
	check(Mode != EMode::ExternalLegacyData);

	if (GetState() != ETrackingState::Tracked)
	{
		MarkComponentRenderInstancesDirty();
		return;
	}

	// Make sure we have (lazy) allocated the change tracking arrays
	ResizeChangeTrackingState();

	// Just mark the info, actual data will be pulled out when dispatching an update
	TrackingArray[InstanceId] = true;

	MarkComponentRenderInstancesDirty();
}

void FPrimitiveInstanceDataManager::MarkComponentRenderInstancesDirty()
{
	if (!bComponentMarkedDirty)
	{
		if (UPrimitiveComponent *PrimitiveComponentPtr = PrimitiveComponent.Get())
		{
			PrimitiveComponentPtr->MarkRenderInstancesDirty();
			bComponentMarkedDirty = true;
		}
	}
}

bool FPrimitiveInstanceDataManager::HasIdentityMapping() const
{
	return IndexToIdMap.IsEmpty();
}

bool FPrimitiveInstanceDataManager::ShouldTrackIds() const
{
	return Proxy != nullptr;
}

void FPrimitiveInstanceDataManager::MarkRemoved(FPrimitiveInstanceId InstanceId)
{
	LOG_INST_DATA(TEXT("MarkRemoved(Id: %d)"), InstanceId.Id);

	if (ResizeChangeTrackingState())
	{
		RemovedInstances[InstanceId] = true;
		AddedInstances[InstanceId] = false;
		IndexChangeInstances[InstanceId] = false;
	}

	if (!HasIdentityMapping())
	{
		IdToIndexMap[InstanceId.Id] = INDEX_NONE;
		ValidInstanceIdMask[InstanceId.Id] = false;
		// Must start from the lowest free index since we'd otherwise get holes when adding things.
		IdSearchStartIndex = FMath::Min(IdSearchStartIndex, InstanceId.Id);
	}

	LOG_INST_DATA(TEXT("IdToIndexMap[%d] = %d"), InstanceId.Id, INDEX_NONE);
}

void FPrimitiveInstanceDataManager::MarkIndexChanged(FPrimitiveInstanceId InstanceId)
{
	LOG_INST_DATA(TEXT("MarkIndexChanged(Id: %d)"), InstanceId.Id);
	MarkChangeHelper(IndexChangeInstances, InstanceId);	
}

TSharedPtr<FISMCInstanceDataSceneProxy, ESPMode::ThreadSafe> FPrimitiveInstanceDataManager::GetOrCreateProxy(FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (Proxy && !Proxy->CheckPlatformFeatureLevel(InShaderPlatform, InFeatureLevel))
	{
		// TODO: May need to add some attachment counter checks here?
		Proxy.Reset();
		ClearChangeTracking();
	}

	if (!Proxy)
	{
		if (!UseGPUScene(InShaderPlatform, InFeatureLevel))
		{
			// Catch all proxy for both ISM / HISM in mobile mode.
			Proxy = MakeShared<FISMCInstanceDataSceneProxyNoGPUScene>(InShaderPlatform, InFeatureLevel, Mode == EMode::Legacy);
		}
		else if (Mode == EMode::Legacy || Mode == EMode::ExternalLegacyData)
		{
			Proxy = MakeShared<FISMCInstanceDataSceneProxyLegacyReordered>(InShaderPlatform, InFeatureLevel, Mode != EMode::ExternalLegacyData);
		}
		else
		{
			Proxy = MakeShared<FISMCInstanceDataSceneProxy>(InShaderPlatform, InFeatureLevel);
		}
	}

	return Proxy;
}

void FPrimitiveInstanceDataManager::Invalidate(int32 InNumInstances)
{
	Proxy.Reset();
	ClearIdTracking(InNumInstances);
	// When the proxy is being replaced, we want to invalidate the owning proxy thing as the continuity is lost.
	if (UPrimitiveComponent *PrimitiveComponentPtr = PrimitiveComponent.Get())
	{
		PrimitiveComponentPtr->MarkRenderStateDirty();
		bComponentMarkedDirty = true;
	}
}

void FPrimitiveInstanceDataManager::ValidateMapping() const
{
#if DO_GUARD_SLOW
	check(HasIdentityMapping() || IndexToIdMap.Num() == NumInstances);
	for (int32 Index = 0; Index < IndexToIdMap.Num(); ++Index)
	{
		FPrimitiveInstanceId Id = IndexToIdMap[Index];
		check(ValidInstanceIdMask[Id.GetAsIndex()]);
		check(Index == IdToIndexMap[Id.GetAsIndex()]);
	}
	for (int32 Id = 0; Id < IdToIndexMap.Num(); ++Id)
	{
		int32 Index = IdToIndexMap[Id];
		if (Index != INDEX_NONE)
		{
			check(ValidInstanceIdMask[Id]);
			check(IndexToIdMap[Index].GetAsIndex() == Id);
		}
		else
		{
			check(!ValidInstanceIdMask[Id]);
		}
	}
	int32 FirstFalse = ValidInstanceIdMask.Find(false);
	check(FirstFalse < 0 || FirstFalse >= IdSearchStartIndex);
#endif
}

void FPrimitiveInstanceDataManager::MarkForRebuildFromExternal(int32 InNumInstances, TUniqueFunction<FExternalUpdateData(TArray<TRefCountPtr<HHitProxy>> &OutHitProxies)> &&BuildUpdateData)
{
	// Forget any tracked changes that happened before (and ID-mapping)
	ClearIdTracking(InNumInstances);
	ExternalBuildUpdateData = MoveTemp(BuildUpdateData);
	// Must actually track the changes because we may see incremental changes in the same frame,
	TrackingState = ETrackingState::Tracked;
	MarkComponentRenderInstancesDirty();
}

SIZE_T FPrimitiveInstanceDataManager::GetAllocatedSize() const
{
	return ValidInstanceIdMask.GetAllocatedSize() +
		AddedInstances.GetAllocatedSize() +
		RemovedInstances.GetAllocatedSize() +
		TransformChangedInstances.GetAllocatedSize() +
		CustomDataChangedInstances.GetAllocatedSize() +
		BakedLightingDataChangedInstances.GetAllocatedSize() +
		IndexChangeInstances.GetAllocatedSize();
}
