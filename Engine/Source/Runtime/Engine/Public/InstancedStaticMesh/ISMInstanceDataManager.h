// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISMInstanceDataSceneProxy.h"

class FPrimitiveSceneProxy;
class HHitProxy;

/**
 * Data descriptor representing the component state abstracting the UPrimitiveComponent, needs to be passed into the change flushing.
 * The intention is to decouple the manager from the UComponent or any other supplier of instance data & scene proxies.
 */
struct FInstanceUpdateComponentDesc
{
	FMatrix PrimitiveLocalToWorld;
	FRenderBounds StaticMeshBounds;
	FInstanceDataFlags Flags;
	FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;
	// Note: this must be supplied to allow calculating correct bounds for each instance, currently this is derived in the primitive proxy ctor, meaning it should be fetched from the proxy
	//       However, we may want to remove this coupling.
	// float AbsMaxDisplacement = 0.0f;

	// The number of instances that will be allocated in the proxy
	int32 NumProxyInstances = -1;
	// Number of instances in the source (e.g., the component)
	int32 NumSourceInstances = -1;

	// Callback to fill in the required change set use with delta-update capable onwing components.
	// TODO: move elsewhere?
	TFunction<void(FISMInstanceUpdateChangeSet &ChangeSet)> BuildChangeSet;
};

/**
 * Manager class that tracks changes to instance data within the component, and is responsible for dispatching updates of the proxy.
 * Tracks instance index changes to be able to maintain a persistent ID mapping for use on the render thread.
 * The ID mapping is not serialized and will be reset when the proxy is recreated.
 * Not responsible for storing the component representation of the instance data.
 * NOTE/TODO: This is tied to the ISM use-case, mostly because of legacy (HISM) interactions. Will be refactored and sub-classed or something.
 *            Also: Still somewhat tied to the UComponent, which also can be refactored a bit to make it more general.
 */
class FPrimitiveInstanceDataManager : public FInstanceIdIndexMap
{
public:
	ENGINE_API FPrimitiveInstanceDataManager(UPrimitiveComponent* InPrimitiveComponent);

	/**
	 * Current tracking state, 
	 */
	enum class ETrackingState
	{
		Initial, // In the initial state, there is no proxy and therefore changes do not need to be tracked, e.g., during initial setup of an ISM component.
		Tracked,
	};

	enum class EMode
	{
		Default,
		Legacy, // In this mode, we create a legacy supporting proxy.
		ExternalLegacyData // In this mode, it is illegal to call the incremental state tracking methods & updates can only be sent to the proxy if there has been and extranal one queued.
	};

	/**
	 */
	ENGINE_API void SetMode(EMode InMode);

	/**
	 */
	ENGINE_API EMode GetMode() const { return Mode; }

	void Add(int32 InInstanceAddAtIndex, bool bInsert);

	void RemoveAtSwap(int32 InstanceIndex);
	
	void RemoveAt(int32 InstanceIndex);

	void TransformChanged(int32 InstanceIndex);
	void TransformChanged(FPrimitiveInstanceId InstanceId);
	
	void TransformsChangedAll();

	void CustomDataChanged(int32 InstanceIndex);

	void BakedLightingDataChanged(int32 InstanceIndex);

	void BakedLightingDataChangedAll();

	void NumCustomDataChanged();

#if WITH_EDITOR
	void EditorDataChangedAll();
#endif

	void PrimitiveTransformChanged();

	ENGINE_API bool HasAnyInstanceChanges() const;

	void SerializeRenderData(FArchive& Ar, bool bCooked);

	bool FlushChanges(FInstanceUpdateComponentDesc &&ComponentData, bool bNewPrimitiveProxy);

	// 
	void PostLoad(int32 InNumInstances, TUniquePtr<FStaticMeshInstanceData> &&InStaticMeshInstanceData);

	/**
	 * Clear the ID/Index association and reset the mapping to identity & number of instances to the given number.
	 * Also clears the change tracking state.
	 */
	void ClearIdTracking(int32 InNumInstances);

	/**
	 * Clear all tracked changes (will result in a full update when next one is flushed)
	 */
	void ClearChangeTracking();

	int32 GetMaxInstanceId() const;

	int32 GetMaxInstanceIndex() const;

	/**
	 * Resize (grow) the tracking state for data changes.
	 * Returns true if the state is actually tracked, false otherwise.
	 */
	bool ResizeChangeTrackingState();

	void CreateExplicitIdentityMapping();


	ETrackingState GetState() const { return TrackingState; }

	ENGINE_API TSharedPtr<FISMCInstanceDataSceneProxy, ESPMode::ThreadSafe> GetOrCreateProxy(FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel);

	void Invalidate(int32 InNumInstances);
	
	void ValidateMapping() const;

	struct FExternalUpdateData
	{
		int32 NumCustomDataFloats = 0; 
		int32 NumInstances = -1;
		TUniqueFunction<void(FISMCInstanceDataSceneProxy &Proxy, const FRenderBounds &InstanceLocalBounds)> UpdateProxy;
	};
	/**
	 * Call to mark the manager as needing a full rebuild & having an external driver for this.
	 * The BuildUpdateData function may not be called, as this only happens if the proxy is actually created.
	 * BuildUpdateData shares lifetime with the component so can safely capture the this pointer.
	 * The FExternalUpdateData produced by BuildUpdateData is responsible for capturing data needed to update the Proxy, 
	 * This will be moved through to the updater task and executed there, should thus not reference the component.
	 * Calling this also invalidates any ID/Index mapping as the instance data management is completely up to the external source.
	 */
	ENGINE_API void MarkForRebuildFromExternal(int32 InNumInstances, TUniqueFunction<FExternalUpdateData(TArray<TRefCountPtr<HHitProxy>> &OutHitProxies)> &&BuildUpdateData);

	/**
	 */
	ENGINE_API SIZE_T GetAllocatedSize() const;

private:
	template <typename TaskLambdaType>
	static void BeginUpdateTask(FInstanceDataUpdateTaskInfo &InstanceDataUpdateTaskInfo, TaskLambdaType &&TaskLambda, const FInstanceDataBufferHeader &InInstanceDataBufferHeader);

	template <typename TaskLambdaType>
	void DispatchUpdateTask(bool bUnattached, const FInstanceDataBufferHeader &InstanceDataBufferHeader, TaskLambdaType &&TaskLambda);

	inline void MarkChangeHelper(FChangeBitArray& TrackingArray, int32 InstanceIndex);
	inline void MarkChangeHelper(FChangeBitArray& TrackingArray, FPrimitiveInstanceId InstanceId);

	void MarkComponentRenderInstancesDirty();

	bool HasIdentityMapping() const;

	bool ShouldTrackIds() const;

	void MarkRemoved(FPrimitiveInstanceId InstanceId);

	void MarkIndexChanged(FPrimitiveInstanceId InstanceId);

	void InitChangeSet(const union FChangeDesc &ChangeDesc, const FInstanceUpdateComponentDesc &ComponentData, FISMInstanceUpdateChangeSet &ChangeSet);

	EMode Mode = EMode::Default;
	ETrackingState TrackingState = ETrackingState::Initial;

	// Id allocation tracking
	TBitArray<> ValidInstanceIdMask;
	int32 IdSearchStartIndex = 0;

	// Change set.
	FChangeBitArray AddedInstances;
	FChangeBitArray RemovedInstances;
	FChangeBitArray TransformChangedInstances;
	FChangeBitArray CustomDataChangedInstances;
	FChangeBitArray BakedLightingDataChangedInstances;
	bool bNumCustomDataChanged = false;
	bool bBakedLightingDataChanged = false;
	bool bTransformChangedAllInstances = false;
#if WITH_EDITOR
	bool bAnyEditorDataChanged = false;
#endif	
	bool bPrimitiveTransformChanged = false;
	FChangeBitArray IndexChangeInstances;

	TSharedPtr<FISMCInstanceDataSceneProxy, ESPMode::ThreadSafe> Proxy;
	TWeakObjectPtr<UPrimitiveComponent> PrimitiveComponent = nullptr;

	bool bComponentMarkedDirty = false;
	bool bEnableTracking = false;

	TUniqueFunction<FExternalUpdateData(TArray<TRefCountPtr<HHitProxy>> &OutHitProxies)> ExternalBuildUpdateData;

	// Used for serialized legacy data to drive the non-GPU scene rendering path.
	TUniquePtr<FStaticMeshInstanceData> LegacyStaticMeshInstanceData;

	// Must track this to detect changes 
	// TODO: make event driven and save the storage?
	FMatrix PrimitiveLocalToWorld;
	FInstanceDataFlags Flags;
	int32 NumCustomDataFloats = 0;
	float AbsMaxDisplacement = 0.0f;
	FRenderBounds StaticMeshBounds;
};
