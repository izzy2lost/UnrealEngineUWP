// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/WorldPartitionActorDescViewMap.h"

class FStreamingGenerationActorDescCollection : public TActorDescContainerCollection<TObjectPtr<const UActorDescContainer>>
{
public:
	FStreamingGenerationActorDescCollection() = default;
	ENGINE_API FStreamingGenerationActorDescCollection(std::initializer_list<TObjectPtr<const UActorDescContainer>> ActorDescContainerArray);
	ENGINE_API FStreamingGenerationActorDescCollection(const TArray<const UActorDescContainer*>& ActorDescContainers);

	ENGINE_API UWorld* GetWorld() const;

	// @todo_ow : Remove once conversion to ExternalDataLayer is complete. 
	// It is present to handle content bundles streaming generation via the same code path. 
	ENGINE_API FGuid GetContentBundleGuid() const;

	ENGINE_API const UActorDescContainer* GetMainActorDescContainer() const;
	ENGINE_API FName GetMainContainerPackageName() const;
	ENGINE_API TArrayView<const UActorDescContainer* const> GetExternalDataLayerContainers();

	ENGINE_API virtual void OnCollectionChanged() override;

private:
	ENGINE_API void SortCollection();

	static constexpr int MainContainerIdx = 0;
	static constexpr int ExternalDataLayerContainerStartIdx = MainContainerIdx + 1;
};

class FStreamingGenerationActorDescView : public FWorldPartitionActorDescView
{
public:
	ENGINE_API FStreamingGenerationActorDescView();
	ENGINE_API FStreamingGenerationActorDescView(const FWorldPartitionActorDesc* InActorDesc);

	//~ Begin FWorldPartitionActorDescView interface
	ENGINE_API virtual FName GetRuntimeGrid() const override;
	ENGINE_API virtual bool GetIsSpatiallyLoaded() const override;
	ENGINE_API virtual FSoftObjectPath GetHLODLayer() const override;
	ENGINE_API virtual const TArray<FName>& GetDataLayerInstanceNames() const override;
	ENGINE_API virtual const TArray<FGuid>& GetReferences() const override;
	ENGINE_API virtual const TArray<FGuid>& GetEditorReferences() const override;
	//~ End FWorldPartitionActorDescView interface

	ENGINE_API void SetParentView(const FWorldPartitionActorDescView* InParentView);
	ENGINE_API void SetDataLayerInstanceNames(const TArray<FName>& InDataLayerInstanceNames);
	ENGINE_API void SetForcedNonSpatiallyLoaded();
	ENGINE_API void SetForcedNoRuntimeGrid();
	ENGINE_API void SetForcedNoDataLayers();
	ENGINE_API void SetRuntimeDataLayerInstanceNames(const TArray<FName>& InRuntimeDataLayerInstanceNames);
	ENGINE_API void SetRuntimeReferences(const TArray<FGuid>& InRuntimeReferences);
	ENGINE_API void SetEditorReferences(const TArray<FGuid>& InEditorReferences);		
	ENGINE_API void SetForcedNoHLODLayer();
	ENGINE_API void SetRuntimeHLODLayer(const FSoftObjectPath& InHLODLayer);

	ENGINE_API const TArray<FName>& GetRuntimeDataLayerInstanceNames() const;

	bool operator==(const FWorldPartitionActorDescView& Other) const
	{
		return GetGuid() == Other.GetGuid();
	}

	friend uint32 GetTypeHash(const FWorldPartitionActorDescView& Key)
	{
		return GetTypeHash(Key.GetGuid());
	}

protected:
	const FWorldPartitionActorDescView* ParentView;
	bool bIsForcedNonSpatiallyLoaded;
	bool bIsForcedNoRuntimeGrid;
	bool bIsForcedNoDataLayers;
	bool bIsForceNoHLODLayer;
	TOptional<TArray<FName>> ResolvedDataLayerInstanceNames;
	TOptional<TArray<FName>> RuntimeDataLayerInstanceNames;
	TOptional<TArray<FGuid>> RuntimeReferences;
	TOptional<FSoftObjectPath> RuntimedHLODLayer;
	TArray<FGuid> EditorReferences;
};

class FStreamingGenerationActorDescViewMap : public TActorDescViewMap<FStreamingGenerationActorDescView> {};

#endif // WITH_EDITOR
