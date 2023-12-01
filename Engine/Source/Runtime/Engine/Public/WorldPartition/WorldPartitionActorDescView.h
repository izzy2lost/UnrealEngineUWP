// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/Optional.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

#if WITH_EDITOR
class AActor;
class IStreamingGenerationErrorHandler;
class UActorDescContainer;
class UWorldPartition;
struct FWorldPartitionActorFilter;
enum class EWorldPartitionActorFilterType : uint8;

/**
 * A view on top of an actor descriptor, used to store information that can be potentially different than the actor
 * descriptor itself due to streaming generation logic, etc.
 */
class FWorldPartitionActorDescView
{
public:
	ENGINE_API FWorldPartitionActorDescView();
	ENGINE_API FWorldPartitionActorDescView(const FWorldPartitionActorDesc* InActorDesc);
	
	virtual ~FWorldPartitionActorDescView() {}

	/** Interface on top of FWorldPartitionActorDesc */
	ENGINE_API virtual const FGuid& GetGuid() const;
	ENGINE_API virtual FTopLevelAssetPath GetBaseClass() const;
	ENGINE_API virtual FTopLevelAssetPath GetNativeClass() const;
	ENGINE_API virtual UClass* GetActorNativeClass() const;
	ENGINE_API virtual FName GetRuntimeGrid() const;
	ENGINE_API virtual bool GetIsSpatiallyLoaded() const;
	ENGINE_API virtual bool GetActorIsEditorOnly() const;
	ENGINE_API virtual bool GetActorIsRuntimeOnly() const;
	ENGINE_API virtual bool GetActorIsHLODRelevant() const;
	ENGINE_API virtual FSoftObjectPath GetHLODLayer() const;
	ENGINE_API virtual bool HasResolvedDataLayerInstanceNames() const;
	ENGINE_API virtual const TArray<FName>& GetDataLayerInstanceNames() const;
	ENGINE_API virtual const TArray<FName>& GetTags() const;
	ENGINE_API virtual FName GetActorPackage() const;	
	ENGINE_API virtual FSoftObjectPath GetActorSoftPath() const;
	ENGINE_API virtual FName GetActorLabel() const;
	ENGINE_API virtual FBox GetEditorBounds() const;
	ENGINE_API virtual FBox GetRuntimeBounds() const;
	ENGINE_API virtual const TArray<FGuid>& GetReferences() const;
	ENGINE_API virtual const TArray<FGuid>& GetEditorReferences() const;
	ENGINE_API virtual FString ToString() const;
	ENGINE_API virtual const FGuid& GetParentActor() const;
	ENGINE_API virtual FName GetActorName() const;
	ENGINE_API virtual const FGuid& GetFolderGuid() const;
	ENGINE_API virtual FGuid GetContentBundleGuid() const;
	ENGINE_API virtual FName GetContainerPackage() const;
	ENGINE_API virtual bool IsContainerInstance() const;
	ENGINE_API virtual bool GetContainerInstance(FWorldPartitionActorDesc::FContainerInstance& OutContainerInstance) const;
	ENGINE_API virtual EWorldPartitionActorFilterType GetContainerFilterType() const;
	ENGINE_API virtual const FWorldPartitionActorFilter* GetContainerFilter() const;	
	ENGINE_API virtual void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;
	ENGINE_API virtual FName GetActorLabelOrName() const;
	ENGINE_API virtual AActor* GetActor() const;
	ENGINE_API virtual bool IsEditorOnlyReference(const FGuid& ReferenceGuid) const;
	ENGINE_API virtual bool GetProperty(FName PropertyName, FName* PropertyValue) const;
	ENGINE_API virtual bool HasProperty(FName PropertyName) const;

	/** Helper functions */
	const FWorldPartitionActorDesc* GetActorDesc() const { return ActorDesc; }
	ENGINE_API virtual const TArray<FName>& GetRuntimeDataLayerInstanceNames() const;

protected:
	const FWorldPartitionActorDesc* ActorDesc;
};
#endif
