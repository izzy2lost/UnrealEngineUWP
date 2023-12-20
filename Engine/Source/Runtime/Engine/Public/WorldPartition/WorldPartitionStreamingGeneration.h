// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "OverrideVoidReturnInvoker.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/ActorDescContainerInstanceCollection.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstanceViewInterface.h"

// deprecated
class FWorldPartitionActorDescView;

class FStreamingGenerationActorDescViewMap;

class FStreamingGenerationActorDescView : public IWorldPartitionActorDescInstanceView
{
	friend class FStreamingGenerationActorDescViewMap;
	friend class FWorldPartitionStreamingGenerator;

protected:
	// Used for invalid reference error reporting
	FStreamingGenerationActorDescView(const FWorldPartitionActorDescInstance* InActorDescInstance)
		: ActorDescViewMap(nullptr)
		, ActorDescInstance(InActorDescInstance)
		, ParentView(nullptr)
		, bIsForcedNonSpatiallyLoaded(false)
		, bIsForcedNoRuntimeGrid(false)
		, bIsForcedNoDataLayers(false)
		, bIsForceNoHLODLayer(false)
		, bIsUnsaved(false)
	{
	}

public:
	FStreamingGenerationActorDescView(const FStreamingGenerationActorDescViewMap& InActorDescViewMap, const FWorldPartitionActorDescInstance* InActorDescInstance, bool bInUnsaved = false)
		: FStreamingGenerationActorDescView(InActorDescInstance)
	{
		ActorDescViewMap = &InActorDescViewMap;
		bIsUnsaved = bInUnsaved;
	}
		
	virtual ~FStreamingGenerationActorDescView() {}

	//~ Begin FWorldPartitionActorDescViewProxy	
	virtual const FGuid& GetGuid() const override { return ActorDescInstance->GetGuid(); }

	virtual FTopLevelAssetPath GetBaseClass() const override { return ActorDescInstance->GetBaseClass(); }
	virtual FTopLevelAssetPath GetNativeClass() const override { return ActorDescInstance->GetNativeClass(); }
	virtual UClass* GetActorNativeClass() const override { return ActorDescInstance->GetActorNativeClass(); }

	ENGINE_API virtual FName GetRuntimeGrid() const override;
	ENGINE_API virtual bool GetIsSpatiallyLoaded() const override;
	virtual bool GetActorIsEditorOnly() const override { return ActorDescInstance->GetActorIsEditorOnly(); }
	virtual bool GetActorIsRuntimeOnly() const override { return ActorDescInstance->GetActorIsRuntimeOnly(); }
	virtual bool IsRuntimeRelevant() const override { return ActorDescInstance->IsRuntimeRelevant(); }
	virtual bool IsEditorRelevant() const override { return ActorDescInstance->IsEditorRelevant(); }

	virtual bool IsUsingDataLayerAsset() const override { return ActorDescInstance->IsUsingDataLayerAsset(); }
	virtual const TArray<FName>& GetDataLayers() const override { return ActorDescInstance->GetDataLayers(); }

	virtual bool GetActorIsHLODRelevant() const override { return ActorDescInstance->GetActorIsHLODRelevant(); }
	ENGINE_API virtual FSoftObjectPath GetHLODLayer() const override;

	virtual const TArray<FName>& GetTags() const override { return ActorDescInstance->GetTags(); }
	virtual FName GetActorPackage() const override { return ActorDescInstance->GetActorPackage(); }
	virtual FSoftObjectPath GetActorSoftPath() const override { return ActorDescInstance->GetActorSoftPath(); }
	virtual FName GetActorLabel() const override { return ActorDescInstance->GetActorLabel(); }
	virtual FName GetActorName() const override { return ActorDescInstance->GetActorName(); }
	virtual FName GetFolderPath() const override { return ActorDescInstance->GetFolderPath(); }
	virtual const FGuid& GetFolderGuid() const override { return ActorDescInstance->GetFolderGuid(); }

	virtual FBox GetEditorBounds() const override { return ActorDescInstance->GetEditorBounds(); }
	virtual FBox GetRuntimeBounds() const override { return ActorDescInstance->GetRuntimeBounds(); }

	virtual bool GetProperty(FName PropertyName, FName* PropertyValue) const override { return ActorDescInstance->GetProperty(PropertyName, PropertyValue); }
	virtual bool HasProperty(FName PropertyName) const override { return ActorDescInstance->HasProperty(PropertyName); }

	ENGINE_API virtual const TArray<FGuid>& GetReferences() const override;
	virtual const TArray<FGuid>& GetEditorOnlyReferences() const override { return ActorDescInstance->GetEditorOnlyReferences(); }
	virtual bool IsEditorOnlyReference(const FGuid& ReferenceGuid) const override { return ActorDescInstance->IsEditorOnlyReference(ReferenceGuid); }

	virtual const FGuid& GetParentActor() const override { return ActorDescInstance->GetParentActor(); }

	virtual FGuid GetContentBundleGuid() const override { return ActorDescInstance->GetContentBundleGuid(); }

	virtual bool IsChildContainerInstance() const override { return ActorDescInstance->IsChildContainerInstance(); }
	virtual FName GetChildContainerPackage() const override { return ActorDescInstance->GetChildContainerPackage(); }
	virtual EWorldPartitionActorFilterType GetChildContainerFilterType() const override { return ActorDescInstance->GetChildContainerFilterType(); }
	virtual const FWorldPartitionActorFilter* GetChildContainerFilter() const override { return ActorDescInstance->GetChildContainerFilter(); }
	virtual bool GetChildContainerInstance(FWorldPartitionActorDesc::FContainerInstance& OutContainerInstance) const override { return ActorDescInstance->GetChildContainerInstance(OutContainerInstance); }

	virtual bool IsMainWorldOnly() const override { return ActorDescInstance->IsMainWorldOnly(); }
	virtual bool IsListedInSceneOutliner() const override { return ActorDescInstance->IsListedInSceneOutliner(); }
	virtual const FGuid& GetSceneOutlinerParent() const override { return ActorDescInstance->GetSceneOutlinerParent(); }

	virtual void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const override { GetActorDesc()->CheckForErrors(this, ErrorHandler); }

	virtual FString ToString(FWorldPartitionActorDesc::EToStringMode Mode) const override { return ActorDescInstance->ToString(Mode); }
	virtual const FWorldPartitionActorDesc* GetActorDesc() const override { return ActorDescInstance->GetActorDesc(); }

	virtual bool HasResolvedDataLayerInstanceNames() const override { return ActorDescInstance->HasResolvedDataLayerInstanceNames(); }
	ENGINE_API const TArray<FName>& GetDataLayerInstanceNames() const override;

	virtual AActor* GetActor(bool bEvenIfPendingKill = true, bool bEvenIfUnreachable = false) const override { return ActorDescInstance->GetActor(bEvenIfPendingKill, bEvenIfUnreachable); }
	virtual bool IsLoaded(bool bEvenIfPendingKill = false) const override { return ActorDescInstance->IsLoaded(bEvenIfPendingKill); }

	virtual UActorDescContainerInstance* GetContainerInstance() const override { return ActorDescInstance->GetContainerInstance(); }
	//~ End

	bool IsUnsaved() const { return bIsUnsaved; }
	const FStreamingGenerationActorDescViewMap& GetActorDescViewMap() const { return *ActorDescViewMap; }

	ENGINE_API const TArray<FGuid>& GetEditorReferences() const;
	ENGINE_API const TArray<FName>& GetRuntimeDataLayerInstanceNames() const;

	ENGINE_API void SetForcedNonSpatiallyLoaded();
	ENGINE_API void SetForcedNoRuntimeGrid();
	ENGINE_API void SetForcedNoDataLayers();
	ENGINE_API void SetRuntimeDataLayerInstanceNames(const TArray<FName>& InRuntimeDataLayerInstanceNames);
	ENGINE_API void SetRuntimeReferences(const TArray<FGuid>& InRuntimeReferences);
	ENGINE_API void SetEditorReferences(const TArray<FGuid>& InEditorReferences);
	ENGINE_API void SetDataLayerInstanceNames(const TArray<FName>& InDataLayerInstanceNames);
	ENGINE_API void SetParentView(const FStreamingGenerationActorDescView* InParentView);

	ENGINE_API void SetForcedNoHLODLayer();
	ENGINE_API void SetRuntimeHLODLayer(const FSoftObjectPath& InHLODLayer);

	bool operator==(const FStreamingGenerationActorDescView& Other) const
	{
		return GetGuid() == Other.GetGuid();
	}

	friend uint32 GetTypeHash(const FStreamingGenerationActorDescView& Key)
	{
		return GetTypeHash(Key.GetGuid());
	}

private:
	const FStreamingGenerationActorDescViewMap* ActorDescViewMap;
	const FWorldPartitionActorDescInstance* ActorDescInstance;
		
	const FStreamingGenerationActorDescView* ParentView;

	bool bIsForcedNonSpatiallyLoaded;
	bool bIsForcedNoRuntimeGrid;
	bool bIsForcedNoDataLayers;
	bool bIsForceNoHLODLayer;
	bool bIsUnsaved;
	TOptional<TArray<FName>> ResolvedDataLayerInstanceNames;
	TOptional<TArray<FName>> RuntimeDataLayerInstanceNames;
	TOptional<TArray<FGuid>> RuntimeReferences;
	TOptional<FSoftObjectPath> RuntimedHLODLayer;
	TArray<FGuid> EditorReferences;
};

class FStreamingGenerationActorDescViewMap
{
	friend class FWorldPartitionStreamingGenerator;

private:
	template <class Func>
	void ForEachActorDescView(Func InFunc)
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		for (TUniquePtr<FStreamingGenerationActorDescView>& ActorDescView : ActorDescViewList)
		{
			if (!Invoker(*ActorDescView))
			{
				return;
			}
		}
	}

	FStreamingGenerationActorDescView* FindByGuid(const FGuid& InGuid)
	{
		if (FStreamingGenerationActorDescView** ActorDescViewPtr = ActorDescViewsByGuid.Find(InGuid))
		{
			return *ActorDescViewPtr;
		}
		return nullptr;
	}

	FStreamingGenerationActorDescView& FindByGuidChecked(const FGuid& InGuid)
	{
		return *ActorDescViewsByGuid.FindChecked(InGuid);
	}

public:
	ENGINE_API FStreamingGenerationActorDescViewMap();

	// Non-copyable but movable
	FStreamingGenerationActorDescViewMap(const FStreamingGenerationActorDescViewMap&) = delete;
	FStreamingGenerationActorDescViewMap(FStreamingGenerationActorDescViewMap&&) = default;

	FStreamingGenerationActorDescViewMap& operator=(const FStreamingGenerationActorDescViewMap&) = delete;
	FStreamingGenerationActorDescViewMap& operator=(FStreamingGenerationActorDescViewMap&&) = default;

	ENGINE_API FStreamingGenerationActorDescView* Emplace(const FGuid& InActorGuid, const FStreamingGenerationActorDescView& InActorDescView);
	ENGINE_API FStreamingGenerationActorDescView* Emplace(const FWorldPartitionActorDescInstance* InActorDescInstance);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.4, "Use FStreamingGenerationActorDescView version instead")
	ENGINE_API FWorldPartitionActorDescView* Emplace(const FGuid& InActorGuid, const FWorldPartitionActorDescView& InActorDescView) { return nullptr; }

	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance version instead")
	ENGINE_API class FWorldPartitionActorDescView* Emplace(const FWorldPartitionActorDesc* InActorDesc) { return nullptr; }

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
	FORCEINLINE int32 Num() const
	{
		return ActorDescViewList.Num();
	}

	template <class Func>
	void ForEachActorDescView(Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		for (const TUniquePtr<FStreamingGenerationActorDescView>& ActorDescView : ActorDescViewList)
		{
			if (!Invoker(*ActorDescView))
			{
				return;
			}
		}
	}

	const FStreamingGenerationActorDescView* FindByGuid(const FGuid& InGuid) const
	{
		if (const FStreamingGenerationActorDescView* const* ActorDescViewPtr = ActorDescViewsByGuid.Find(InGuid))
		{
			return *ActorDescViewPtr;
		}
		return nullptr;
	}

	const FStreamingGenerationActorDescView& FindByGuidChecked(const FGuid& InGuid) const
	{
		return *ActorDescViewsByGuid.FindChecked(InGuid);
	}

	template <class ClassType>
	TArray<const FStreamingGenerationActorDescView*> FindByExactNativeClass() const
	{
		return FindByExactNativeClass(ClassType::StaticClass());
	}

	ENGINE_API TArray<const FStreamingGenerationActorDescView*> FindByExactNativeClass(UClass* InExactNativeClass) const;

	const TMap<FGuid, FStreamingGenerationActorDescView*>& GetActorDescViewsByGuid() const { return ActorDescViewsByGuid; }

protected:
	TArray<TUniquePtr<FStreamingGenerationActorDescView>> ActorDescViewList;

	TMap<FGuid, FStreamingGenerationActorDescView*> ActorDescViewsByGuid;
	TMultiMap<FName, const FStreamingGenerationActorDescView*> ActorDescViewsByClass;
};

class FStreamingGenerationContainerInstanceCollection : public TActorDescContainerInstanceCollection<TObjectPtr<const UActorDescContainerInstance>>
{
public:
	FStreamingGenerationContainerInstanceCollection() = default;
	ENGINE_API FStreamingGenerationContainerInstanceCollection(std::initializer_list<TObjectPtr<const UActorDescContainerInstance>> ActorDescContainerInstanceArray);
	ENGINE_API FStreamingGenerationContainerInstanceCollection(const TArray<const UActorDescContainerInstance*>& ActorDescContainerInstances);

	ENGINE_API UWorld* GetWorld() const;

	// @todo_ow : Remove once conversion to ExternalDataLayer is complete. 
	// It is present to handle content bundles streaming generation via the same code path. 
	ENGINE_API FGuid GetContentBundleGuid() const;

	ENGINE_API const UActorDescContainerInstance* GetMainContainer() const;
	ENGINE_API FName GetMainContainerPackageName() const;
	ENGINE_API TArrayView<const UActorDescContainerInstance* const> GetExternalDataLayerContainers();

	ENGINE_API virtual void OnCollectionChanged() override;

private:
	ENGINE_API void SortCollection();

	static constexpr int MainContainerIdx = 0;
	static constexpr int ExternalDataLayerContainerStartIdx = MainContainerIdx + 1;
};

class UE_DEPRECATED(5.4, "Use FStreamingGenerationContainerInstanceCollection instead") FStreamingGenerationActorDescCollection : public TActorDescContainerCollection<TObjectPtr<const UActorDescContainer>>
{
public:
	FStreamingGenerationActorDescCollection() = default;
	FStreamingGenerationActorDescCollection(std::initializer_list<TObjectPtr<const UActorDescContainer>> ActorDescContainerArray) {}
	FStreamingGenerationActorDescCollection(const TArray<const UActorDescContainer*>& ActorDescContainers) {}

	UWorld* GetWorld() const { return nullptr; }
	FGuid GetContentBundleGuid() const { return FGuid(); }
	const UActorDescContainer* GetMainActorDescContainer() const { return nullptr; }
	FName GetMainContainerPackageName() const { return NAME_None; }
	TArrayView<const UActorDescContainer* const> GetExternalDataLayerContainers() { return TArrayView<const UActorDescContainer*>(); }
	virtual void OnCollectionChanged() override {}
};

using FActorDescViewMap UE_DEPRECATED(5.4, "Use FStreamingGenerationActorDescViewMap instead") = FStreamingGenerationActorDescViewMap;

#endif // WITH_EDITOR
