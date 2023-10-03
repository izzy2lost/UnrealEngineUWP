// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStaticMesh/ISMInstanceDataSceneProxy.h"
#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"
#include "Engine/InstancedStaticMesh.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Rendering/RenderingSpatialHash.h"
#include "Rendering/MotionVectorSimulation.h"
#include "Rendering/RenderCommandPipes.h"

DEFINE_LOG_CATEGORY(LogInstanceProxy);

#if 0
#define LOG_INST_DATA(_Format_, ...) UE_LOG(LogInstanceProxy, Log, _Format_, ##__VA_ARGS__)
#else
	#define LOG_INST_DATA(_Format_, ...) 
#endif

FISMCInstanceDataSceneProxy::FISMCInstanceDataSceneProxy(FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel) 
	: ShaderPlatform(InShaderPlatform)
	, FeatureLevel(InFeatureLevel) 
{
	bUseLegacyRenderingPath = !UseGPUScene(ShaderPlatform, FeatureLevel);
}

struct FIdentityIndexRemap
{
	inline int32 operator[](int32 InIndex) const { return InIndex; }

	template <typename ValueType>
	inline void Scatter(bool bHasData, const FArrayIndexDelta &Delta, TArray<ValueType> &DestData, int32 NumOutElements, TArray<ValueType> &&InData, int32 ElementStride = 1) const
	{
		if (bHasData)
		{
			Delta.Scatter(DestData, NumOutElements, MoveTemp(InData), ElementStride);
		}
		else
		{
			DestData.Reset();
		}
	}

	FORCEINLINE constexpr bool RemapIndex(int32 Index) const { return true; }
};

struct FReorderTableIndexRemap
{
	FReorderTableIndexRemap(const TArray<int32> &InReorderTable, int32 InMaxValidIndex) : ReorderTable(InReorderTable), MaxValidIndex(InMaxValidIndex) {}

	inline int32 ClampValidIndex(int32 Index) const
	{
		if (Index < MaxValidIndex)
		{
			return Index;
		}
		return INDEX_NONE;
	}

	inline int32 operator[](int32 InIndex) const 
	{ 
		if (ReorderTable.IsValidIndex(InIndex))
		{
			return ClampValidIndex(ReorderTable[InIndex]);
		}
		return ClampValidIndex(InIndex); 
	}

	FORCEINLINE bool RemapIndex(int32 &Index) const 
	{ 
		Index = operator[](Index);
		return Index != INDEX_NONE; 
	}

	template <typename ValueType>
	inline void Scatter(bool bHasData, const FArrayIndexDelta &Delta, TArray<ValueType> &DestData, int32 NumOutElements, TArray<ValueType> &&InData, int32 ElementStride = 1) const
	{
		if (bHasData)
		{
			Delta.Scatter(DestData, NumOutElements, MoveTemp(InData), *this, ElementStride);
		}
		else
		{
			DestData.Reset();
		}
	}

	const TArray<int32> &ReorderTable;
	int32 MaxValidIndex = 0;
};

FVector3f FISMCInstanceDataSceneProxy::GetLocalBoundsPadExtent(const FRenderTransform& LocalToWorld, float PadAmount)
{
	if (FMath::Abs(PadAmount) < UE_SMALL_NUMBER)
	{
		return FVector3f::ZeroVector;
	}

	FVector3f Scale = LocalToWorld.GetScale();
	return FVector3f(
		Scale.X > 0.0f ? PadAmount / Scale.X : 0.0f,
		Scale.Y > 0.0f ? PadAmount / Scale.Y : 0.0f,
		Scale.Z > 0.0f ? PadAmount / Scale.Z : 0.0f);
}

template <typename IndexRemapType>
void FISMCInstanceDataSceneProxy::ApplyDataChanges(FISMInstanceUpdateChangeSet &ChangeSet, const IndexRemapType &IndexRemap, int32 PostUpdateNumInstances, FInstanceSceneDataBuffers::FWriteView &ProxyData)
{
	ProxyData.PrimitiveToRelativeWorld = ChangeSet.PrimitiveToRelativeWorld;
	ProxyData.PrimitiveWorldSpaceOffset = ChangeSet.PrimitiveWorldSpaceOffset;
		
	check(!ChangeSet.Flags.bHasPerInstanceLocalBounds);
	// TODO: delta support & always assume all bounds changed, and that there is in fact only one
	ProxyData.InstanceLocalBounds = MoveTemp(ChangeSet.InstanceLocalBounds);

	// TODO: DISP - Fix me (this comment came along from FPrimitiveSceneProxy::SetInstanceLocalBounds and is probably still true...)
	const FVector3f PadExtent = GetLocalBoundsPadExtent(ProxyData.PrimitiveToRelativeWorld, ChangeSet.AbsMaxDisplacement);
	for (FRenderBounds& Bounds : ProxyData.InstanceLocalBounds)
	{
		Bounds.Min -= PadExtent;
		Bounds.Max += PadExtent;
	}

	// unpack transform deltas
	ProxyData.InstanceToPrimitiveRelative.SetNumUninitialized(PostUpdateNumInstances);
	for (int32 PackedIndex = 0; PackedIndex < ChangeSet.TransformsDelta.Num(); ++PackedIndex)
	{
		int32 InstanceIndex = ChangeSet.TransformsDelta[PackedIndex];
		if (IndexRemap.RemapIndex(InstanceIndex))
		{
			FRenderTransform LocalToPrimitiveRelativeWorld = ChangeSet.Transforms[PackedIndex] * ChangeSet.PrimitiveToRelativeWorld;
			// Remove shear
			LocalToPrimitiveRelativeWorld.Orthogonalize();
			ProxyData.InstanceToPrimitiveRelative[InstanceIndex] = LocalToPrimitiveRelativeWorld;
		}
	}

	if (ChangeSet.Flags.bHasPerInstanceDynamicData)
	{
		FRenderTransform PrevPrimitiveToRelativeWorld = ChangeSet.PreviousPrimitiveToRelativeWorld.Get(ChangeSet.PrimitiveToRelativeWorld);
		ProxyData.PrevInstanceToPrimitiveRelative.SetNumUninitialized(PostUpdateNumInstances);
		for (int32 PackedIndex = 0; PackedIndex < ChangeSet.TransformsDelta.Num(); ++PackedIndex)
		{
			int32 InstanceIndex = ChangeSet.TransformsDelta[PackedIndex];
			if (IndexRemap.RemapIndex(InstanceIndex))
			{
				FRenderTransform PrevLocalToPrimitiveRelativeWorld = ChangeSet.PrevTransforms[PackedIndex] * PrevPrimitiveToRelativeWorld;
				PrevLocalToPrimitiveRelativeWorld.Orthogonalize();
				ProxyData.PrevInstanceToPrimitiveRelative[InstanceIndex] = PrevLocalToPrimitiveRelativeWorld;
			}
		}
	}
	else
	{
		ProxyData.PrevInstanceToPrimitiveRelative.Reset();
	}

	if (ChangeSet.Flags.bHasPerInstanceCustomData)
	{
		ProxyData.NumCustomDataFloats = ChangeSet.NumCustomDataFloats;
		IndexRemap.Scatter(ChangeSet.Flags.bHasPerInstanceCustomData, ChangeSet.CustomDataDelta, ProxyData.InstanceCustomData, PostUpdateNumInstances, MoveTemp(ChangeSet.PerInstanceCustomData), ProxyData.NumCustomDataFloats);
	}
	else
	{
		ProxyData.NumCustomDataFloats = 0;
		ProxyData.InstanceCustomData.Reset();
	}

	IndexRemap.Scatter(ChangeSet.Flags.bHasPerInstanceLMSMUVBias, ChangeSet.InstanceLightShadowUVBiasDelta, ProxyData.InstanceLightShadowUVBias, PostUpdateNumInstances, MoveTemp(ChangeSet.InstanceLightShadowUVBias));
#if WITH_EDITOR
	IndexRemap.Scatter(ChangeSet.Flags.bHasPerInstanceEditorData, ChangeSet.InstanceEditorDataDelta, ProxyData.InstanceEditorData, PostUpdateNumInstances, MoveTemp(ChangeSet.InstanceEditorData));

	// replace the HP container.
	if (ChangeSet.HitProxyContainer)
	{
		HitProxyContainer = MoveTemp(ChangeSet.HitProxyContainer);
	}

#endif

	// Delayed per instance random generation, moves it off the GT and RT, but still sucks
	if (ChangeSet.Flags.bHasPerInstanceRandom)
	{
		// TODO: only need to process added instances? No help for ISM since the move path would be taken.
		// TODO: OTOH for HISM there is no meaningful data, so just skipping and letting the SetNumZeroed fill in the blanks is fine.
		FArrayIndexDelta PerInstanceRandomDelta(PostUpdateNumInstances);

		ProxyData.InstanceRandomIDs.SetNumZeroed(PostUpdateNumInstances);
		if (ChangeSet.GeneratePerInstanceRandomIds)
		{
			// NOTE: this is not super efficient(!)
			TArray<float> TmpInstanceRandomIDs;
			TmpInstanceRandomIDs.SetNumZeroed(PostUpdateNumInstances);
			ChangeSet.GeneratePerInstanceRandomIds(TmpInstanceRandomIDs);
			IndexRemap.Scatter(true, PerInstanceRandomDelta, ProxyData.InstanceRandomIDs, PostUpdateNumInstances, MoveTemp(TmpInstanceRandomIDs));
		}
		//else 
		//{
		//	IndexRemap.Scatter(true, PerInstanceRandomDelta, ProxyData.InstanceRandomIDs, PostUpdateNumInstances, MoveTemp(ChangeSet.InstanceRandomIDs));
		//}
	}
	else
	{
		ProxyData.InstanceRandomIDs.Reset();
	}
}

struct FDataRemap
{
	// Keep bit mask to efficiently iterate, but not save any data.
	// TODO: Maybe make sparse in the future?
	TBitArray<> Used;
	TArray<int32> FromInices;

	FDataRemap(int32 InNum)
	{
		Used.SetNum(InNum, false);
		FromInices.AddUninitialized(InNum);
	}

	inline void Set(int32 To, int32 From)
	{
		Used[To] = true;
		FromInices[To] = From;
	}
};

template<typename ValueType>
void CondMove(bool bCondition, TArray<ValueType> &Data, int32 FromIndex, int32 ToIndex, int32 NumElements = 1)
{
	if (bCondition)
	{
		FMemory::Memcpy(&Data[ToIndex * NumElements], &Data[FromIndex * NumElements], NumElements * sizeof(ValueType));
	}
}

void FISMCInstanceDataSceneProxy::Build(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	DecStatCounters();
	check(ChangeSet.IsFullUpdate());
	check(!ChangeSet.TransformsDelta.IsDelta());
	check(!ChangeSet.CustomDataDelta.IsDelta() || (!ChangeSet.Flags.bHasPerInstanceCustomData && ChangeSet.CustomDataDelta.IsEmpty()));
	check(!ChangeSet.InstanceLightShadowUVBiasDelta.IsDelta() || ChangeSet.InstanceLightShadowUVBiasDelta.IsEmpty());
#if WITH_EDITOR
	check(!ChangeSet.InstanceEditorDataDelta.IsDelta() || ChangeSet.InstanceEditorDataDelta.IsEmpty());
#endif
	check(ChangeSet.PostUpdateNumInstances == ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex());

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView WriteView = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	WriteView.Flags = ChangeSet.Flags;

	FIdentityIndexRemap IndexRemap;
	ApplyDataChanges(ChangeSet, IndexRemap, ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex(), WriteView);
	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);

	InstanceSceneDataBuffers.ValidateData();

	IncStatCounters();
}

void FISMCInstanceDataSceneProxy::Update(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	check(!ChangeSet.IsFullUpdate());

	DecStatCounters();

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	ProxyData.Flags = ChangeSet.Flags;

	int32 PostUpdateNumInstances = ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex();
	check(ChangeSet.PostUpdateNumInstances == ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex());

	// 1. Perform any changes to instance ordering (that might have resulted from deleting or otherwise rearranging instances)
	FDataRemap DataRemap(PostUpdateNumInstances);

	// Build a remap from old instance order to new instance order, carrying out any movement requested to fill holes from removes
		
//FIXME:
//	1. Resize dest arrays first, since we may move data to the upper reaches
//	2. Filter out anything that is added or fully updated anyway
//	3. Add validation to check that we don't validate reordering assumptions.

	// Note: in general, it is more efficient to forego the source order (since it supports inefficient Remove() operations, not RemoveSwap) and instead figure out a 
	//       better mapping for the renderer. However, this is the simplest implementation! Or supposed to be anyway.
	for (TConstSetBitIterator<> BitIt(ChangeSet.ChangeMask.IndexChangeInstances); BitIt; ++BitIt)
	{
		FPrimitiveInstanceId InstanceId{BitIt.GetIndex()};
		// Filter out added instances, since they don't exist in the previous state.
		if (!ChangeSet.ChangeMask.AddedInstances[InstanceId])
		{
			int32 ToIndex = ChangeSet.InstanceIdIndexMap.IdToIndex(InstanceId);
			if (ToIndex != INDEX_NONE && ToIndex < PostUpdateNumInstances)
			{
				DataRemap.Set(ToIndex, InstanceIdIndexMap.IdToIndex(InstanceId));
			}
		}
	}

	// Iterate in order from low to high index since remove always moves data up (does not work for more general shuffling!)
	for (TConstSetBitIterator<> BitIt(DataRemap.Used); BitIt; ++BitIt)
	{
		int32 ToIndex = BitIt.GetIndex();
		int32 FromIndex = DataRemap.FromInices[ToIndex];
		ProxyData.InstanceToPrimitiveRelative[ToIndex] = ProxyData.InstanceToPrimitiveRelative[FromIndex];
		CondMove(ChangeSet.Flags.bHasPerInstanceCustomData, ProxyData.InstanceCustomData, FromIndex, ToIndex, ChangeSet.NumCustomDataFloats);
		CondMove(ChangeSet.Flags.bHasPerInstanceRandom, ProxyData.InstanceRandomIDs, FromIndex, ToIndex);
		CondMove(ChangeSet.Flags.bHasPerInstanceLMSMUVBias, ProxyData.InstanceLightShadowUVBias, FromIndex, ToIndex);
#if WITH_EDITOR
		CondMove(ChangeSet.Flags.bHasPerInstanceEditorData, ProxyData.InstanceEditorData, FromIndex, ToIndex);
#endif
	}

	FIdentityIndexRemap IndexRemap;
	ApplyDataChanges(ChangeSet, IndexRemap, PostUpdateNumInstances, ProxyData);

	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);

	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	InstanceSceneDataBuffers.ValidateData();

	IncStatCounters();
}

void FISMCInstanceDataSceneProxy::DebugDrawInstanceChanges(FPrimitiveDrawInterface* DebugPDI, ESceneDepthPriorityGroup SceneDepthPriorityGroup)
{
	InstanceDataUpdateTaskInfo.WaitForUpdateCompletion();

	for (int Index = 0; Index < InstanceSceneDataBuffers.GetNumInstances(); ++Index)
	{
		FPrimitiveInstanceId InstanceId = InstanceIdIndexMap.IndexToId(Index);

		FMatrix InstanceToWorld = InstanceSceneDataBuffers.GetInstanceToWorld(Index);
		DrawWireStar(DebugPDI, InstanceToWorld.GetOrigin(), 40.0f, ChangeMask.TransformChangedInstances.WasChanged(InstanceId) ? FColor::Red : FColor::Green, SceneDepthPriorityGroup);

		if (ChangeMask.CustomDataChangedInstances.WasChanged(InstanceId))
		{
			DrawCircle(DebugPDI, InstanceToWorld.GetOrigin(), FVector(1, 0, 0), FVector(0, 1, 0), FColor::Orange, 40.0f, 32, SceneDepthPriorityGroup);
		}
	}
}

FISMCInstanceDataSceneProxyLegacyReordered::FISMCInstanceDataSceneProxyLegacyReordered(FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, bool bInLegacyReordered) 
	: FISMCInstanceDataSceneProxy(InShaderPlatform, InFeatureLevel)
	, bLegacyReordered(bInLegacyReordered)
{
}

void FISMCInstanceDataSceneProxyLegacyReordered::Update(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	check(!ChangeSet.IsFullUpdate());
	check(bLegacyReordered || ChangeSet.LegacyInstanceReorderTable.IsEmpty());
	DecStatCounters();

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	ProxyData.Flags = ChangeSet.Flags;
	// Handle deletions before updating the data.
	{
		FReorderTableIndexRemap IndexRemapOld(LegacyInstanceReorderTable, InstanceSceneDataBuffers.GetNumInstances(AccessTag));

		ProxyData.VisibleInstances.SetNum(ChangeSet.PostUpdateNumInstances, true);
		ProxyData.Flags.bHasPerInstanceVisible = true;
		for (TConstSetBitIterator<> BitIt(ChangeSet.ChangeMask.RemovedInstances); BitIt; ++BitIt)
		{
			// This is somewhat nonintuitive, but the current instance->index map is where we retain knowledge of where the instance used to be placed (in the component address space at last update)
			int32 InstanceIndex = InstanceIdIndexMap.IdToIndex(FPrimitiveInstanceId{BitIt.GetIndex()});
			if (IndexRemapOld.RemapIndex(InstanceIndex))
			{
				LOG_INST_DATA(TEXT("Update/HideInstance, ID: %d, IDX: %d"), BitIt.GetIndex(), InstanceIndex);
				ProxyData.VisibleInstances[InstanceIndex] = false;
			}
		}
	}
	LegacyInstanceReorderTable = MoveTemp(ChangeSet.LegacyInstanceReorderTable);
	FReorderTableIndexRemap IndexRemap(LegacyInstanceReorderTable, ChangeSet.PostUpdateNumInstances);

	// Use the index reorder table to scatter the data to the correct locations.
	ApplyDataChanges(ChangeSet, IndexRemap, ChangeSet.PostUpdateNumInstances, ProxyData);
	
	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);

	InstanceSceneDataBuffers.ValidateData();
	IncStatCounters();
}


void FISMCInstanceDataSceneProxyLegacyReordered::Build(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	DecStatCounters();
	check(ChangeSet.IsFullUpdate());
	check(!ChangeSet.TransformsDelta.IsDelta());
	check(!ChangeSet.CustomDataDelta.IsDelta() || (!ChangeSet.Flags.bHasPerInstanceCustomData && ChangeSet.CustomDataDelta.IsEmpty()));
	check(!ChangeSet.InstanceLightShadowUVBiasDelta.IsDelta() || ChangeSet.InstanceLightShadowUVBiasDelta.IsEmpty());
	check(bLegacyReordered || ChangeSet.LegacyInstanceReorderTable.IsEmpty());
#if WITH_EDITOR
	check(!ChangeSet.InstanceEditorDataDelta.IsDelta() || ChangeSet.InstanceEditorDataDelta.IsEmpty());
#endif

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	LegacyInstanceReorderTable = MoveTemp(ChangeSet.LegacyInstanceReorderTable);
	ProxyData.Flags = ChangeSet.Flags;

	FReorderTableIndexRemap IndexRemap(LegacyInstanceReorderTable, ChangeSet.PostUpdateNumInstances);
	ApplyDataChanges(ChangeSet, IndexRemap, ChangeSet.PostUpdateNumInstances, ProxyData);

	// Is there is a reorder table and it does not have the same number as the instances, some must be hidden
	if (bLegacyReordered && ChangeSet.PostUpdateNumInstances != LegacyInstanceReorderTable.Num())
	{
		ProxyData.VisibleInstances.Reset();
		ProxyData.VisibleInstances.SetNum(ChangeSet.PostUpdateNumInstances, false);
		for (int32 Index : LegacyInstanceReorderTable)
		{
			if (Index != INDEX_NONE)
			{
				ProxyData.VisibleInstances[Index] = true;
			}
		}
		ProxyData.Flags.bHasPerInstanceVisible = true;
	}
	else
	{
		// Mark everything as visible from the start.
		ProxyData.VisibleInstances.Reset();
		ProxyData.Flags.bHasPerInstanceVisible = false;
	}	
	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);
	
	InstanceSceneDataBuffers.ValidateData();
	IncStatCounters();
}


void FISMCInstanceDataSceneProxyLegacyReordered::BuildFromLegacyData(TUniquePtr<FStaticMeshInstanceData> &&InExternalLegacyData, const FRenderBounds &InstanceLocalBounds, TArray<int32> &&InLegacyInstanceReorderTable)
{
	DecStatCounters();

	ExternalLegacyData = MoveTemp(InExternalLegacyData);
	check(bLegacyReordered || InLegacyInstanceReorderTable.IsEmpty());
	LegacyInstanceReorderTable = MoveTemp(InLegacyInstanceReorderTable);

	check(!bUseLegacyRenderingPath);
	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	// Not supported in this path
	ProxyData.Flags.bHasPerInstanceDynamicData = false;
	ProxyData.PrevInstanceToPrimitiveRelative.Empty();
	check(!ProxyData.Flags.bHasPerInstancePayloadExtension);

	int32 NumInstances = ExternalLegacyData->GetNumInstances();
	ProxyData.VisibleInstances.Reset();
	ProxyData.VisibleInstances.SetNum(NumInstances, true);

	ProxyData.InstanceToPrimitiveRelative.Reset(NumInstances);

	ProxyData.InstanceLightShadowUVBias.SetNumZeroed(ProxyData.Flags.bHasPerInstanceLMSMUVBias ? NumInstances : 0);
	ProxyData.InstanceLocalBounds = MakeArrayView(&InstanceLocalBounds, 1);
	ProxyData.NumCustomDataFloats = ExternalLegacyData->GetNumCustomDataFloats();
	ProxyData.InstanceCustomData.SetNumZeroed(ProxyData.Flags.bHasPerInstanceCustomData ? NumInstances * ProxyData.NumCustomDataFloats : 0); 

	ProxyData.InstanceRandomIDs.SetNumZeroed(ProxyData.Flags.bHasPerInstanceRandom ? NumInstances : 0);

#if WITH_EDITOR
	ProxyData.InstanceEditorData.SetNumZeroed(ProxyData.Flags.bHasPerInstanceEditorData ? NumInstances : 0);
#endif
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		FRenderTransform InstanceToPrimitive;
		ExternalLegacyData->GetInstanceTransform(InstanceIndex, InstanceToPrimitive);
		FRenderTransform LocalToPrimitiveRelativeWorld = InstanceToPrimitive * ProxyData.PrimitiveToRelativeWorld;
		// Remove shear
		LocalToPrimitiveRelativeWorld.Orthogonalize();
		ProxyData.InstanceToPrimitiveRelative.Add(LocalToPrimitiveRelativeWorld);

		if (ProxyData.Flags.bHasPerInstanceDynamicData)
		{
			// TODO: this doesn't exist...
		}

		if (ProxyData.Flags.bHasPerInstanceCustomData)
		{
			ExternalLegacyData->GetInstanceCustomDataValues(InstanceIndex, MakeArrayView(ProxyData.InstanceCustomData.GetData() + InstanceIndex * ProxyData.NumCustomDataFloats, ProxyData.NumCustomDataFloats));
		}

		if (ProxyData.Flags.bHasPerInstanceRandom)
		{
			ExternalLegacyData->GetInstanceRandomID(InstanceIndex, ProxyData.InstanceRandomIDs[InstanceIndex]);
		}

		if (ProxyData.Flags.bHasPerInstanceLMSMUVBias)
		{
			ExternalLegacyData->GetInstanceLightMapData(InstanceIndex, ProxyData.InstanceLightShadowUVBias[InstanceIndex]);
		}

#if WITH_EDITOR
		// TODO:
		if (ProxyData.Flags.bHasPerInstanceEditorData)
		{
			FColor HitProxyColor;
			bool bSelected;
			ExternalLegacyData->GetInstanceEditorData(InstanceIndex, HitProxyColor, bSelected);
			ProxyData.InstanceEditorData[InstanceIndex] = FInstanceEditorData::Pack(HitProxyColor, bSelected);
		}
#endif
	}
	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	InstanceSceneDataBuffers.ValidateData();
	IncStatCounters();
}

void FISMCInstanceDataSceneProxyLegacyReordered::UpdateInstancesTransforms(FInstanceSceneDataBuffers::FWriteView &ProxyData, const FStaticMeshInstanceData &LegacyInstanceData)
{
	ProxyData.PrevInstanceToPrimitiveRelative.Empty();
	check(!ProxyData.Flags.bHasPerInstancePayloadExtension);
	int32 NumInstances = LegacyInstanceData.GetNumInstances();
	ProxyData.InstanceToPrimitiveRelative.Reset(NumInstances);
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		FRenderTransform InstanceToPrimitive;
		LegacyInstanceData.GetInstanceTransform(InstanceIndex, InstanceToPrimitive);
		FRenderTransform LocalToPrimitiveRelativeWorld = InstanceToPrimitive * ProxyData.PrimitiveToRelativeWorld;
		// Remove shear
		LocalToPrimitiveRelativeWorld.Orthogonalize();
		ProxyData.InstanceToPrimitiveRelative.Add(LocalToPrimitiveRelativeWorld);
	}
}

void FISMCInstanceDataSceneProxyLegacyReordered::UpdatePrimitiveTransform(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	check(ExternalLegacyData && InstanceSceneDataBuffers.GetNumInstances() == ExternalLegacyData->GetNumInstances() || InstanceSceneDataBuffers.GetNumInstances() == 0);

	if (ExternalLegacyData)
	{
		FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
		FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
		ProxyData.PrimitiveToRelativeWorld = ChangeSet.PrimitiveToRelativeWorld;
		ProxyData.PrimitiveWorldSpaceOffset = ChangeSet.PrimitiveWorldSpaceOffset;
		UpdateInstancesTransforms(ProxyData, *ExternalLegacyData);
		InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

		InstanceSceneDataBuffers.ValidateData();
	}
}

FISMCInstanceDataSceneProxyNoGPUScene::FISMCInstanceDataSceneProxyNoGPUScene(FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, bool bInLegacyReordered) 
	: FISMCInstanceDataSceneProxyLegacyReordered(InShaderPlatform, InFeatureLevel, bInLegacyReordered)
{
}

FISMCInstanceDataSceneProxyNoGPUScene::~FISMCInstanceDataSceneProxyNoGPUScene()
{
	ReleaseStaticMeshInstanceBuffer();
}

template <typename IndexRemapType>
void FISMCInstanceDataSceneProxyNoGPUScene::ApplyDataChanges(FISMInstanceUpdateChangeSet &ChangeSet, const IndexRemapType &IndexRemap, int32 PostUpdateNumInstances, FInstanceSceneDataBuffers::FWriteView &ProxyData, FStaticMeshInstanceData &LegacyInstanceData)
{

	ProxyData.NumCustomDataFloats = ChangeSet.Flags.bHasPerInstanceCustomData ? ChangeSet.NumCustomDataFloats : 0;
	LegacyInstanceData.AllocateInstances(PostUpdateNumInstances, ProxyData.NumCustomDataFloats, GIsEditor ? EResizeBufferFlags::AllowSlackOnGrow|EResizeBufferFlags::AllowSlackOnReduce : EResizeBufferFlags::None, false); // In Editor always permit overallocation, to prevent too much realloc

	ProxyData.PrimitiveToRelativeWorld = ChangeSet.PrimitiveToRelativeWorld;
	ProxyData.PrimitiveWorldSpaceOffset = ChangeSet.PrimitiveWorldSpaceOffset;
		
	check(!ChangeSet.Flags.bHasPerInstanceLocalBounds);
	// TODO: delta support & always assume all bounds changed, and that there is in fact only one
	ProxyData.InstanceLocalBounds = MoveTemp(ChangeSet.InstanceLocalBounds);

	// TODO: DISP - Fix me (this comment came along from FPrimitiveSceneProxy::SetInstanceLocalBounds and is probably still true...)
	const FVector3f PadExtent = GetLocalBoundsPadExtent(ProxyData.PrimitiveToRelativeWorld, ChangeSet.AbsMaxDisplacement);
	for (FRenderBounds& Bounds : ProxyData.InstanceLocalBounds)
	{
		Bounds.Min -= PadExtent;
		Bounds.Max += PadExtent;
	}

	// TODO: Dont bother for delta updates perhaps since it didnt use to work anyway, though now we are potentially doing more of that...
	TArray<float> InstanceRandomIDs;
	// Delayed per instance random generation, moves it off the GT and RT, but still sucks
	if (ChangeSet.Flags.bHasPerInstanceRandom)
	{
		// TODO: only need to process added instances? No help for ISM since the move path would be taken.
		// TODO: OTOH for HISM there is no meaningful data, so just skipping and letting the SetNumZeroed fill in the blanks is fine.
		FArrayIndexDelta PerInstanceRandomDelta(PostUpdateNumInstances);

		InstanceRandomIDs.SetNumZeroed(PostUpdateNumInstances);
		if (ChangeSet.GeneratePerInstanceRandomIds)
		{
			ChangeSet.GeneratePerInstanceRandomIds(InstanceRandomIDs);
		}
	}

	// unpack transform deltas
	// TODO: Only do if requested / needed
	ProxyData.InstanceToPrimitiveRelative.SetNumUninitialized(PostUpdateNumInstances);
	for (int32 PackedIndex = 0; PackedIndex < ChangeSet.TransformsDelta.Num(); ++PackedIndex)
	{
		int32 InstanceIndex = ChangeSet.TransformsDelta[PackedIndex];
		if (IndexRemap.RemapIndex(InstanceIndex))
		{
			LegacyInstanceData.SetInstance(InstanceIndex, ChangeSet.Transforms[PackedIndex].ToMatrix44f(), ChangeSet.Flags.bHasPerInstanceRandom ? InstanceRandomIDs[InstanceIndex] : 0.0f);

			// TODO: Only do if requested / needed
			FRenderTransform LocalToPrimitiveRelativeWorld = ChangeSet.Transforms[PackedIndex] * ChangeSet.PrimitiveToRelativeWorld;
			// Remove shear
			LocalToPrimitiveRelativeWorld.Orthogonalize();
			ProxyData.InstanceToPrimitiveRelative[InstanceIndex] = LocalToPrimitiveRelativeWorld;
		}
	}

	if (ChangeSet.Flags.bHasPerInstanceCustomData)
	{
		for (int32 PackedIndex = 0; PackedIndex < ChangeSet.CustomDataDelta.Num(); ++PackedIndex)
		{
			int32 InstanceIndex = ChangeSet.CustomDataDelta[PackedIndex];
			if (IndexRemap.RemapIndex(InstanceIndex))
			{
				for (int32 j = 0; j < ProxyData.NumCustomDataFloats; ++j)
				{
					LegacyInstanceData.SetInstanceCustomData(InstanceIndex, j, ChangeSet.PerInstanceCustomData[PackedIndex * ProxyData.NumCustomDataFloats + j]);
				}
			}
		}
	}
	if (ChangeSet.Flags.bHasPerInstanceLMSMUVBias)
	{
		for (int32 PackedIndex = 0; PackedIndex < ChangeSet.InstanceLightShadowUVBiasDelta.Num(); ++PackedIndex)
		{
			int32 InstanceIndex = ChangeSet.InstanceLightShadowUVBiasDelta[PackedIndex];
			if (IndexRemap.RemapIndex(InstanceIndex))
			{
				FVector4f Packed = ChangeSet.InstanceLightShadowUVBias[PackedIndex];
				FVector2D LightmapUVBias = FVector2D(Packed.X, Packed.Y);
				FVector2D ShadowmapUVBias = FVector2D(Packed.Z, Packed.W);

				LegacyInstanceData.SetInstanceLightMapData(InstanceIndex, LightmapUVBias, ShadowmapUVBias);
			}
		}
	}

#if WITH_EDITOR
	if (ChangeSet.Flags.bHasPerInstanceEditorData)
	{
		for (int32 PackedIndex = 0; PackedIndex < ChangeSet.InstanceEditorDataDelta.Num(); ++PackedIndex)
		{
			int32 InstanceIndex = ChangeSet.InstanceEditorDataDelta[PackedIndex];
			if (IndexRemap.RemapIndex(InstanceIndex))
			{
				FColor HitProxyColor;
				bool bSelected;
				FInstanceEditorData::Unpack(ChangeSet.InstanceEditorData[PackedIndex], HitProxyColor, bSelected);

				LegacyInstanceData.SetInstanceEditorData(InstanceIndex, HitProxyColor, bSelected);
			}
		}
	}

	// replace the HP container.
	if (ChangeSet.HitProxyContainer)
	{
		HitProxyContainer = MoveTemp(ChangeSet.HitProxyContainer);
	}
#endif
}

FStaticMeshInstanceBuffer* FISMCInstanceDataSceneProxyNoGPUScene::GetLegacyInstanceBuffer() 
{ 
	if (bUseLegacyRenderingPath)
	{
		// Must sync to be sure the build is complete
		InstanceDataUpdateTaskInfo.WaitForUpdateCompletion();
		return LegacyInstanceBuffer.Get(); 
	}
	return nullptr;
}

void FISMCInstanceDataSceneProxyNoGPUScene::Update(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	check(!ChangeSet.IsFullUpdate());

	check(bLegacyReordered || ChangeSet.PostUpdateNumInstances == ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex());
	check(bLegacyReordered || ChangeSet.LegacyInstanceReorderTable.IsEmpty());

	DecStatCounters();

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

	ProxyData.Flags = ChangeSet.Flags;
	// Handle deletions before updating the data.
	{
		FReorderTableIndexRemap IndexRemapOld(LegacyInstanceReorderTable, LegacyInstanceBuffer->GetNumInstances());

		// HISMTODO: move to own implementation
		ProxyData.VisibleInstances.SetNum(ChangeSet.PostUpdateNumInstances, true);
		ProxyData.Flags.bHasPerInstanceVisible = true;
		for (TConstSetBitIterator<> BitIt(ChangeSet.ChangeMask.RemovedInstances); BitIt; ++BitIt)
		{
			// This is somewhat nonintuitive, but the current instance->index map is where we retain knowledge of where the instance used to be placed (in the component address space at last update)
			int32 InstanceIndex = InstanceIdIndexMap.IdToIndex(FPrimitiveInstanceId{BitIt.GetIndex()});
			if (IndexRemapOld.RemapIndex(InstanceIndex))
			{
				LOG_INST_DATA(TEXT("Update/HideInstance, ID: %d, IDX: %d"), BitIt.GetIndex(), InstanceIndex);
				LegacyInstanceBuffer->InstanceData->NullifyInstance(InstanceIndex);
			}
		}
	}
	LegacyInstanceReorderTable = MoveTemp(ChangeSet.LegacyInstanceReorderTable);
	FReorderTableIndexRemap IndexRemap(LegacyInstanceReorderTable, ChangeSet.PostUpdateNumInstances);

	ApplyDataChanges(ChangeSet, IndexRemap, ChangeSet.PostUpdateNumInstances, ProxyData, *LegacyInstanceBuffer->InstanceData.Get());

	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	LegacyInstanceBuffer->SetFlushToGPUPending();
		
	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);

	IncStatCounters();
}

void FISMCInstanceDataSceneProxyNoGPUScene::Build(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	DecStatCounters();
	check(ChangeSet.IsFullUpdate());
	check(!ChangeSet.TransformsDelta.IsDelta());
	check(!ChangeSet.CustomDataDelta.IsDelta() || (!ChangeSet.Flags.bHasPerInstanceCustomData && ChangeSet.CustomDataDelta.IsEmpty()));
	check(!ChangeSet.InstanceLightShadowUVBiasDelta.IsDelta() || ChangeSet.InstanceLightShadowUVBiasDelta.IsEmpty());
#if WITH_EDITOR
	check(!ChangeSet.InstanceEditorDataDelta.IsDelta() || ChangeSet.InstanceEditorDataDelta.IsEmpty());
#endif

	check(bLegacyReordered || ChangeSet.PostUpdateNumInstances == ChangeSet.InstanceIdIndexMap.GetMaxInstanceIndex());
	check(bLegacyReordered || ChangeSet.LegacyInstanceReorderTable.IsEmpty());

	LegacyInstanceReorderTable = MoveTemp(ChangeSet.LegacyInstanceReorderTable);

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
	ProxyData.Flags = ChangeSet.Flags;

	FStaticMeshInstanceData LegacyInstanceData(/*bInUseHalfFloat = */true);
	FReorderTableIndexRemap IndexRemap(LegacyInstanceReorderTable, ChangeSet.PostUpdateNumInstances);
	ApplyDataChanges(ChangeSet, IndexRemap, ChangeSet.PostUpdateNumInstances, ProxyData, LegacyInstanceData);
	InstanceSceneDataBuffers.EndWriteAccess(AccessTag);

	// Is there is a reorder table and it does not have the same number as the instances, some must be hidden
	if (bLegacyReordered && ChangeSet.PostUpdateNumInstances != LegacyInstanceReorderTable.Num())
	{
		TBitArray<> HiddenInstances;
		HiddenInstances.SetNum(ChangeSet.PostUpdateNumInstances, true);
		for (int32 InstanceIndex : LegacyInstanceReorderTable)
		{
			if (InstanceIndex != INDEX_NONE)
			{
				HiddenInstances[InstanceIndex]  = false;
			}
		}
		for (TConstSetBitIterator<> BitIt(HiddenInstances); BitIt; ++BitIt)
		{
			LegacyInstanceData.NullifyInstance(BitIt.GetIndex());
		}
	}

	// no need to provide CPU access sice we don't use this on the renderer any more, also no need to defer since we only create this data when actually needed.
	// TODO: strip out those flags & associated logic
	if (!LegacyInstanceBuffer)
	{
		LegacyInstanceBuffer = MakeUnique<FStaticMeshInstanceBuffer>(FeatureLevel, false);
	}
	LegacyInstanceBuffer->InitFromPreallocatedData(LegacyInstanceData);
	LegacyInstanceBuffer->SetFlushToGPUPending();

	ChangeMask = MoveTemp(ChangeSet.ChangeMask);
	InstanceIdIndexMap = MoveTemp(ChangeSet.InstanceIdIndexMap);

	IncStatCounters();
}

void FISMCInstanceDataSceneProxyNoGPUScene::BuildFromLegacyData(TUniquePtr<FStaticMeshInstanceData>&& InExternalLegacyData, const FRenderBounds & InstanceLocalBounds, TArray<int32>&& InLegacyInstanceReorderTable)
{
	ExternalLegacyData = MoveTemp(InExternalLegacyData);
	LegacyInstanceReorderTable = MoveTemp(InLegacyInstanceReorderTable);

	// NEW_INSTANCE_DATA_PATH_TODO: May not want to do this for every ISM, just those that actually have DF or anything else that will be accessed on the CPU?
	{
		FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
		FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
		ProxyData.InstanceLocalBounds = MakeArrayView(&InstanceLocalBounds, 1);
		UpdateInstancesTransforms(ProxyData, *ExternalLegacyData);
		InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
	}

	if (!LegacyInstanceBuffer)
	{
		LegacyInstanceBuffer = MakeUnique<FStaticMeshInstanceBuffer>(FeatureLevel, false);
	}	
	// Note: this passes ownership of the contained data
	LegacyInstanceBuffer->InitFromPreallocatedData(*ExternalLegacyData);
	LegacyInstanceBuffer->SetFlushToGPUPending();
}

void FISMCInstanceDataSceneProxyNoGPUScene::UpdatePrimitiveTransform(FISMInstanceUpdateChangeSet&& ChangeSet)
{
	check(LegacyInstanceBuffer && LegacyInstanceBuffer->GetNumInstances() == LegacyInstanceBuffer->GetNumInstances() || InstanceSceneDataBuffers.GetNumInstances() == 0);

	if (LegacyInstanceBuffer)
	{
		FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
		FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);
		ProxyData.PrimitiveToRelativeWorld = ChangeSet.PrimitiveToRelativeWorld;
		ProxyData.PrimitiveWorldSpaceOffset = ChangeSet.PrimitiveWorldSpaceOffset;
		UpdateInstancesTransforms(ProxyData, *LegacyInstanceBuffer->InstanceData.Get());
		InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
	}
}

void FISMCInstanceDataSceneProxyNoGPUScene::ReleaseStaticMeshInstanceBuffer()
{
	if (LegacyInstanceBuffer)
	{
		ENQUEUE_RENDER_COMMAND(FReleasePerInstanceRenderData)(UE::RenderCommandPipe::Scene, 
			[LegacyInstanceBufferRt = MoveTemp(LegacyInstanceBuffer)](FRHICommandList& RHICmdList) mutable
			{
				LegacyInstanceBufferRt->ReleaseResource();
				LegacyInstanceBufferRt.Reset();
			});
	}
}
