// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/DataLayer/DataLayerInstanceNames.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UWorldPartition;
class UActorDescContainerInstance;
#endif // WITH_EDITOR

class FWorldPartitionActorDescInstance
{
#if WITH_EDITOR
	friend struct FWorldPartitionHandleImpl;
	friend struct FWorldPartitionReferenceImpl;
	friend struct FWorldPartitionActorDescUtils;
	friend class FWorldPartitionActorDesc;
	friend class UActorDescContainerInstance;
	friend class FWorldPartitionLoadingContext;
	friend struct FWorldPartitionActorDescUnitTestAcccessor;
	friend class UDataLayerManager;
	friend class UWorldPartition;
	friend class IWorldPartitionActorLoaderInterface;

protected:
	ENGINE_API FWorldPartitionActorDescInstance();
public:
	ENGINE_API FWorldPartitionActorDescInstance(UActorDescContainerInstance* InContainerInstance, FWorldPartitionActorDesc* InActorDesc);
	virtual ~FWorldPartitionActorDescInstance() {}

	virtual const FGuid& GetGuid() const { return GetActorDesc()->GetGuid(); }
	virtual FTopLevelAssetPath GetBaseClass() const { return GetActorDesc()->GetBaseClass(); }
	virtual FTopLevelAssetPath GetNativeClass() const { return GetActorDesc()->GetNativeClass(); }
	virtual UClass* GetActorNativeClass() const { return GetActorDesc()->GetActorNativeClass(); }
	
	virtual FName GetRuntimeGrid() const { return GetActorDesc()->GetRuntimeGrid(); }
	virtual bool GetIsSpatiallyLoaded() const { return !GetForceNonSpatiallyLoaded() && GetActorDesc()->GetIsSpatiallyLoaded(); }
	virtual bool GetActorIsEditorOnly() const { return GetActorDesc()->GetActorIsEditorOnly(); }
	virtual bool GetActorIsRuntimeOnly() const { return GetActorDesc()->GetActorIsRuntimeOnly(); }
	ENGINE_API virtual bool IsRuntimeRelevant() const;
	ENGINE_API virtual bool IsEditorRelevant() const;

	virtual bool IsUsingDataLayerAsset() const { return GetActorDesc()->IsUsingDataLayerAsset(); }
	virtual TArray<FName> GetDataLayers() const { return GetActorDesc()->GetDataLayers(); }

	virtual bool GetActorIsHLODRelevant() const { return GetActorDesc()->GetActorIsHLODRelevant(); }
	virtual FSoftObjectPath GetHLODLayer() const { return GetActorDesc()->GetHLODLayer(); }

	virtual const TArray<FName>& GetTags() const { return GetActorDesc()->GetTags(); }
	virtual FName GetActorPackage() const { return GetActorDesc()->GetActorPackage(); }
	ENGINE_API virtual FSoftObjectPath GetActorSoftPath() const;
	virtual FName GetActorLabel() const { return GetActorDesc()->GetActorLabel(); }
	ENGINE_API virtual FName GetActorName() const;
	virtual FName GetFolderPath() const { return GetActorDesc()->GetFolderPath(); }
	virtual const FGuid& GetFolderGuid() const { return GetActorDesc()->GetFolderGuid(); }

	virtual FBox GetEditorBounds() const { return GetActorDesc()->GetEditorBounds(); }
	virtual FBox GetRuntimeBounds() const { return GetActorDesc()->GetRuntimeBounds(); }

	virtual bool GetProperty(FName PropertyName, FName* PropertyValue) const { return GetActorDesc()->GetProperty(PropertyName, PropertyValue); }
	virtual bool HasProperty(FName PropertyName) const { return GetActorDesc()->HasProperty(PropertyName); }

	virtual const TArray<FGuid>& GetReferences() const { return GetActorDesc()->GetReferences(); }
	virtual const TArray<FGuid>& GetEditorOnlyReferences() const { return GetActorDesc()->GetEditorOnlyReferences(); }
	virtual bool IsEditorOnlyReference(const FGuid& ReferenceGuid) const { return GetActorDesc()->IsEditorOnlyReference(ReferenceGuid); }
	
	virtual const FGuid& GetParentActor() const { return GetActorDesc()->GetParentActor(); }
		
	virtual FGuid GetContentBundleGuid() const { return GetActorDesc()->GetContentBundleGuid(); }
	virtual const FSoftObjectPath& GetExternalDataLayerAsset() const { return GetActorDesc()->GetExternalDataLayerAsset(); }

	virtual bool IsChildContainerInstance() const { return ChildContainerInstance || GetActorDesc()->IsChildContainerInstance(); }
	virtual FName GetChildContainerPackage() const { return GetActorDesc()->GetChildContainerPackage(); }
	virtual EWorldPartitionActorFilterType GetChildContainerFilterType() const { return GetActorDesc()->GetChildContainerFilterType(); }
	virtual const FWorldPartitionActorFilter* GetChildContainerFilter() const { return GetActorDesc()->GetChildContainerFilter(); }
	virtual bool GetChildContainerInstance(FWorldPartitionActorDesc::FContainerInstance& OutContainerInstance) const { return GetActorDesc()->GetChildContainerInstance(this, OutContainerInstance); }
			
	virtual bool IsMainWorldOnly() const { return GetActorDesc()->IsMainWorldOnly(); }
	virtual bool IsListedInSceneOutliner() const { return GetActorDesc()->IsListedInSceneOutliner(); }
	virtual const FGuid& GetSceneOutlinerParent() const { return GetActorDesc()->GetSceneOutlinerParent(); }
			
	ENGINE_API virtual const FWorldPartitionActorDesc* GetActorDesc() const { check(ActorDesc); return ActorDesc; }
	
	virtual bool HasResolvedDataLayerInstanceNames() const { return ResolvedDataLayerInstanceNames.IsSet(); }
	ENGINE_API virtual const FDataLayerInstanceNames& GetDataLayerInstanceNames() const;

	ENGINE_API virtual bool IsLoaded(bool bEvenIfPendingKill = false) const;
	ENGINE_API virtual AActor* GetActor(bool bEvenIfPendingKill = true, bool bEvenIfUnreachable = false) const;
	
	ENGINE_API virtual FString ToString(FWorldPartitionActorDesc::EToStringMode Mode = FWorldPartitionActorDesc::EToStringMode::Compact) const;
	
	virtual UActorDescContainerInstance* GetContainerInstance() const { return ContainerInstance; }

	inline FName GetActorLabelOrName() const { return GetActorLabel().IsNone() ? GetActorName() : GetActorLabel(); }

	inline UActorDescContainerInstance* GetChildContainerInstance() const { return ChildContainerInstance; }

	ENGINE_API virtual TWeakObjectPtr<AActor>* GetActorPtr(bool bEvenIfPendingKill = true, bool bEvenIfUnreachable = false) const;
	ENGINE_API virtual bool IsValid() const;
				
	inline void SetForceNonSpatiallyLoadded(bool bForce) { bIsForcedNonSpatiallyLoaded = bForce; }
	inline bool GetForceNonSpatiallyLoaded() const { return bIsForcedNonSpatiallyLoaded; }

	inline void SetUnloadedReason(FText* InUnloadedReason) { UnloadedReason = InUnloadedReason; }

	FName GetDisplayClassName() const { return GetActorDesc()->GetDisplayClassName(); }
	
	ENGINE_API const FText& GetUnloadedReason() const;
		
	ENGINE_API AActor* Load();
	ENGINE_API void Unload();

protected:
	UWorldPartition* GetLoadedChildWorldPartition() const { return GetActorDesc()->GetLoadedChildWorldPartition(this); }
	ENGINE_API void UpdateActorDesc(FWorldPartitionActorDesc* InActorDesc);
	void Invalidate();
		
	inline uint32 IncSoftRefCount() const
	{
		return ++SoftRefCount;
	}

	inline uint32 DecSoftRefCount() const
	{
		check(SoftRefCount > 0);
		return --SoftRefCount;
	}

	inline uint32 IncHardRefCount() const
	{
		return ++HardRefCount;
	}

	inline uint32 DecHardRefCount() const
	{
		check(HardRefCount > 0);
		return --HardRefCount;
	}

	inline uint32 GetSoftRefCount() const
	{
		return SoftRefCount;
	}

	inline uint32 GetHardRefCount() const
	{
		return HardRefCount;
	}

	inline void SetDataLayerInstanceNames(const FDataLayerInstanceNames& InDataLayerInstanceNames) { ResolvedDataLayerInstanceNames = InDataLayerInstanceNames; }

	virtual ENGINE_API void RegisterChildContainerInstance();
	virtual ENGINE_API void UnregisterChildContainerInstance();
	virtual ENGINE_API void UpdateChildContainerInstance();

protected:
	TObjectPtr<UActorDescContainerInstance>		ContainerInstance;

	mutable uint32								SoftRefCount;
	mutable uint32								HardRefCount;
	TOptional<FDataLayerInstanceNames>			ResolvedDataLayerInstanceNames;
	bool										bIsForcedNonSpatiallyLoaded;
	mutable FText*								UnloadedReason;

	// Instancing Path if set
	TOptional<FSoftObjectPath>					ActorPath;

	mutable TWeakObjectPtr<AActor>				ActorPtr;
	FWorldPartitionActorDesc*					ActorDesc;

	TObjectPtr<UActorDescContainerInstance>		ChildContainerInstance;

#if DO_CHECK
private:
	struct FRegisteringUnregisteringGuard
	{
		FRegisteringUnregisteringGuard(FWorldPartitionActorDescInstance* InActorDescInstance)
			:ActorDescInstance(InActorDescInstance)
		{
			check(!ActorDescInstance->bIsRegisteringOrUnregistering);
			ActorDescInstance->bIsRegisteringOrUnregistering = true;
		}

		~FRegisteringUnregisteringGuard()
		{
			check(ActorDescInstance->bIsRegisteringOrUnregistering);
			ActorDescInstance->bIsRegisteringOrUnregistering = false;
		}

		FRegisteringUnregisteringGuard(const FRegisteringUnregisteringGuard&) = delete;
		FRegisteringUnregisteringGuard& operator=(const FRegisteringUnregisteringGuard&) = delete;

	private:
		FWorldPartitionActorDescInstance* ActorDescInstance;
	};
	bool bIsRegisteringOrUnregistering = false;
#endif // DO_CHECK

#endif // WITH_EDITOR
};

