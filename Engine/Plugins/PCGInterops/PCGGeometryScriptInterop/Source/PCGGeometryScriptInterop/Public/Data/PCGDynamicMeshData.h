// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGSpatialData.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshOctree3.h"

#include "Misc/SpinLock.h"

#include "PCGDynamicMeshData.generated.h"

struct FPCGContext;
class UDynamicMesh;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGGEOMETRYSCRIPTINTEROP_API UPCGDynamicMeshData : public UPCGSpatialData
{
	GENERATED_BODY()

public:
	UPCGDynamicMeshData(const FObjectInitializer& ObjectInitializer);
	
	void Initialize(UDynamicMesh* InMesh, FPCGContext* Context = nullptr, bool bCanTakeOwnership = false);
	void Initialize(UE::Geometry::FDynamicMesh3&& InMesh, FPCGContext* Context = nullptr);
	
	UFUNCTION(BlueprintCallable, Category="DynamicMesh", meta = (DisplayName = "Initialize"))
	void K2_Initialize(UDynamicMesh* InMesh, FPCGContext& Context) { Initialize(InMesh, &Context); }
	
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::DynamicMesh; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 3; }
	virtual FBox GetBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// TODO needs an implementation to support projection
	//virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	//~End UPCGSpatialData interface

	const UE::Geometry::FDynamicMeshOctree3& GetDynamicMeshOctree() const;
	
	UDynamicMesh* GetMutableDynamicMesh() { bDynamicMeshBoundsAreDirty = true; bDynamicMeshOctreeIsDirty = true; return DynamicMesh; }
	const UDynamicMesh* GetDynamicMesh() const { return DynamicMesh; }

protected:
	// ~Begin UPCGSpatialData interface
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
public:
	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds) const override;
	//~End UPCGSpatialData interface

private:
	// const but will set the mutable CachedBounds
	void ResetBounds() const;

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category="DynamicMesh")
	TObjectPtr<UDynamicMesh> DynamicMesh;

	mutable UE::Geometry::FDynamicMeshOctree3 DynamicMeshOctree;
	mutable bool bDynamicMeshOctreeIsDirty = true;
	mutable FCriticalSection DynamicMeshOctreeLock;
	
	mutable FBox CachedBounds = FBox(EForceInit::ForceInit);
	mutable bool bDynamicMeshBoundsAreDirty = true;
	mutable UE::FSpinLock DynamicMeshBoundsLock;
};
