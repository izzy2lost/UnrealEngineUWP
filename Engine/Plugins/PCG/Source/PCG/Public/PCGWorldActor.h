// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

#include "PCGWorldActor.generated.h"

class UPCGLandscapeCache;
namespace EEndPlayReason { enum Type : int; }

UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable)
class APCGWorldActor : public AActor
{
	GENERATED_BODY()

public:
	APCGWorldActor(const FObjectInitializer& ObjectInitializer);

	//~Begin AActor Interface
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Creates guids for unused grid sizes. */
	void CreateGridGuidsIfNecessary(const PCGHiGenGrid::FSizeArray& InGridSizes, bool bAreGridsSerialized);

	/** Returns the serialized grid GUIDs used for the partitioned actors, one per grid size. */
	void GetSerializedGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const;

	/** Returns the transient grid GUIDs used for the partitioned actors, one per grid size. */
	void GetTransientGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const;

	void MergeFrom(APCGWorldActor* OtherWorldActor);

#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	virtual bool IsUserManaged() const override { return false; }
	virtual bool ShouldExport() override { return false; }
	virtual bool ShouldImport(FStringView ActorPropString, bool IsMovingLevel) override { return false; }
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	//~End AActor Interface

	static APCGWorldActor* CreatePCGWorldActor(UWorld* InWorld);
#endif

	static inline constexpr uint32 DefaultPartitionGridSize = 25600; // 256m

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	/** Size of the PCG partition actor grid for non-hierarchical-generation graphs. */
	UPROPERTY(config, EditAnywhere, Category = GenerationSettings)
	uint32 PartitionGridSize;

	/** Contains all the PCG data required to query the landscape complete. Serialized in cooked builds only */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = CachedData, meta = (NoResetToDefault, DisplayName="Landscape Cache"))
	TObjectPtr<UPCGLandscapeCache> LandscapeCacheObject = nullptr;

	/** Disable creation of Partition Actors on the Z axis. Can improve performances if 3D partitioning is not needed. */
	UPROPERTY(config, EditAnywhere, Category = GenerationSettings)
	bool bUse2DGrid = true;

#if WITH_EDITORONLY_DATA
	/** Allows any currently active editor viewport to act as a Runtime Generation Source. */
	UPROPERTY(EditAnywhere, Category = RuntimeGeneration)
	bool bTreatEditorViewportAsGenerationSource = false;
#endif

private:
	void RegisterToSubsystem();
	void UnregisterFromSubsystem();

#if WITH_EDITOR
	void OnPartitionGridSizeChanged();
#endif

	/** GUIDs of the serialized partitioned actor grids, one per grid size. */
	UPROPERTY()
	TMap<uint32, FGuid> GridGuids;
	mutable FRWLock GridGuidsLock;

	/** GUIDs of the transient partitioned actor grids, one per grid size. */
	UPROPERTY(Transient)
	TMap<uint32, FGuid> TransientGridGuids;
	mutable FRWLock TransientGridGuidsLock;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Grid/PCGLandscapeCache.h"
#endif
