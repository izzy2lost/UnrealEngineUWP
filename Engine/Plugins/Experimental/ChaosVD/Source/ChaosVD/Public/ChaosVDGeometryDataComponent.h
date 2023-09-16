// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ImplicitFwd.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Chaos/ImplicitObject.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "HAL/Platform.h"
#include "UObject/Interface.h"
#include "UObject/StrongObjectPtr.h"

#include "ChaosVDGeometryDataComponent.generated.h"

class IChaosVDGeometryDataComponent;
class UMeshComponent;
class UMaterialInstanceDynamic;

namespace Chaos
{
	class FImplicitObject;
}

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDMeshReadyDelegate, IChaosVDGeometryDataComponent&)

UINTERFACE()
class UChaosVDGeometryDataComponent : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface with a default implementation for any Geometry component that
 * contains CVD data
 */
class IChaosVDGeometryDataComponent
{
	GENERATED_BODY()

public:

	/** Returns the Geometry ID used to identify the geometry data this component represents */
	virtual uint32 GetGeometryID() const PURE_VIRTUAL(IChaosVDGeometryDataComponent::GetGeometryID, return 0;);

	/** Sets the Geometry ID used to identify the geometry data this component represents */
	virtual void SetGeometryID(uint32 ID) PURE_VIRTUAL(IChaosVDGeometryDataComponent::SetGeometryID);

	/** True if the mesh this component represents is ready for use */
	virtual bool IsMeshReady() const  PURE_VIRTUAL(IChaosVDGeometryDataComponent::IsMeshReady, return false;);

	/** Sets if the mesh this component represents is ready for use or not */
	virtual void SetIsMeshReady(bool bIsReady) PURE_VIRTUAL(IChaosVDGeometryDataComponent::SetIsMeshReady);
	
	/** Triggers when the mesh this component represents is ready */
	virtual FChaosVDMeshReadyDelegate* OnMeshReady() PURE_VIRTUAL(IChaosVDGeometryDataComponent::OnMeshReady, return nullptr;);

	/** Stores a shader ptr to the root implicit object of the implicit object this component represents  */
	virtual void SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject) PURE_VIRTUAL(IChaosVDGeometryDataComponent::SetRootImplicitObject);

	/** Stores a shader ptr to the implicit object of the implicit object this component represents  */
	virtual void SetImplicitObject(const Chaos::FImplicitObject* InImplicitObject) PURE_VIRTUAL(IChaosVDGeometryDataComponent::SetImplicitObject);

	/** Returns a ptr to the CVD Collision Data */
	virtual FChaosVDShapeCollisionData* GetCollisionData() PURE_VIRTUAL(IChaosVDGeometryDataComponent::GetCollisionData, return nullptr;);

	/** Updates the visibility of this component based on the stored CVD data*/
	virtual void UpdateVisibility() PURE_VIRTUAL(IChaosVDGeometryDataComponent::UpdateVisibility);

	/** Updates the colors of this component based on the stored CVD data */
	virtual void UpdateColors() PURE_VIRTUAL(IChaosVDGeometryDataComponent::UpdateColors);

	/** Finds and updates the Shape data using the provided array as source*/
	virtual void UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray) PURE_VIRTUAL(IChaosVDGeometryDataComponent::UpdateDataFromShapeArray);
};

UENUM()
enum class EChaosVDMaterialType
{
	SimOnlyMaterial,
	QueryOnlyMaterial
};

/** Base Implementation for a GeometryDataComponent */
class FChaosVDGeometryDataComponentBase
{
	
protected:

	void UpdateVisibility_Internal(const FChaosVDShapeCollisionData& InCollisionData, UMeshComponent* MeshComponent);
	
	void UpdateDataFromShapeArray_Internal(const TArray<FChaosVDShapeCollisionData>& InShapeArray, FChaosVDShapeCollisionData& CollisionDataToUpdate, UMeshComponent* MeshComponent);

	void UpdateColors_Internal(UMeshComponent* MeshComponent);

	void SetImplicitObject_Internal(const Chaos::FImplicitObject* InImplicitObject);

	UMaterialInstanceDynamic* GetCachedMaterialInstance(EChaosVDMaterialType Type);

	const Chaos::FImplicitObject* GetImplicitObject() const;
	
	uint32 GeometryID = 0;
	
	bool bIsMeshReady = false;

	FLinearColor CurrentGeometryColor = FLinearColor(ForceInitToZero);

	FChaosVDMeshReadyDelegate MeshReadyDelegate;

	TMap<const Chaos::FImplicitObject*, uint32> CachedGeometryHashes;

	Chaos::FConstImplicitObjectPtr RootImplicitObject = nullptr;

	const Chaos::FImplicitObject* ImplicitObject = nullptr;

	// TODO: Remove the use of strong objt ptr
	TMap<EChaosVDMaterialType, TStrongObjectPtr<UMaterialInstanceDynamic>> MaterialInstancesByID;
};
