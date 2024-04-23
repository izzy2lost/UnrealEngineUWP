// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGCollisionWrapperData.generated.h"

struct FBodyInstance;

struct PCG_API FPCGCollisionWrapper
{
	FPCGCollisionWrapper() = default;
	~FPCGCollisionWrapper();
	FPCGCollisionWrapper(const FPCGCollisionWrapper&) = delete;
	FPCGCollisionWrapper(FPCGCollisionWrapper&& Other) = default;
	FPCGCollisionWrapper& operator=(const FPCGCollisionWrapper&) = delete;
	FPCGCollisionWrapper& operator=(FPCGCollisionWrapper&& Other) = default;

	// Simple API - does both the prepare & create body instances in a single step
	bool Initialize(const IPCGAttributeAccessor* Accessor, const IPCGAttributeAccessorKeys* Keys);
	void Uninitialize();

	// Advanced API - allows to do async loading as we separate the mesh finding part from the body creation part
	bool Prepare(const IPCGAttributeAccessor* Accessor, const IPCGAttributeAccessorKeys* Keys, TArray<FSoftObjectPath>& MeshPathsToLoad);
	void CreateBodyInstances(const TArray<FSoftObjectPath>& MeshPaths);
	
	// Retrieves the body instance associated to the entry given by its index
	FBodyInstance* GetBodyInstance(int32 EntryIndex) const;

	TArray<FBodyInstance*> BodyInstances;
	TArray<int32> IndexToBodyInstance;
	bool bInitialized = false;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCollisionWrapperData : public UPCGSpatialData
{
	GENERATED_BODY()

public:
	/** Inititializes the collision wrapper on a point data based on the provided attribute selector */
	PCG_API bool Initialize(const UPCGPointData* InPointData, const FPCGAttributePropertyInputSelector& InCollisionSelector, bool bInUseComplexCollision);

	/** Advanced API for async loading */
	bool PreInitializeAndGatherMeshesEx(const UPCGPointData* InPointData, const FPCGAttributePropertyInputSelector& InCollisionSelector, bool bInUseComplexCollision, TArray<FSoftObjectPath>& OutMeshesToLoad);
	void FinalizeInitializationEx(const TArray<FSoftObjectPath>& InMeshPaths);

	// ~Begin UPCGData Interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Primitive; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	// ~End UPCGData Interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 3; }
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	// ~End UPCGSpatialData interface

	// For performance reasons, we keep a raw pointer to the point data in editor.
#if WITH_EDITOR
	inline const UPCGPointData* GetPointData() const { return RawPointData; }
#else
	inline const UPCGPointData* GetPointData() const { return PointData.Get(); }
#endif

private:
	UPROPERTY()
	TObjectPtr<const UPCGPointData> PointData;

	UPROPERTY()
	FPCGAttributePropertyInputSelector CollisionSelector;

	UPROPERTY()
	bool bUseComplexCollision = false;

	FPCGCollisionWrapper CollisionWrapper;

#if WITH_EDITOR
	const UPCGPointData* RawPointData = nullptr;
#endif
};