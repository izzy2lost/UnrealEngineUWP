// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDStaticMeshComponent.generated.h"

/** CVD version of a Static Mesh Component that holds additional CVD data */
UCLASS(HideCategories=("Transform"), MinimalAPI)
class UChaosVDStaticMeshComponent : public UStaticMeshComponent, public IChaosVDGeometryDataComponent, public FChaosVDGeometryDataComponentBase
{
	GENERATED_BODY()
public:
	UChaosVDStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{		
	}

	// BEGIN IChaosVDGeometryDataComponent Interface
	virtual FChaosVDShapeCollisionData* GetCollisionData() override { return &CollisionData; }
	
	virtual uint32 GetGeometryID() const override { return GeometryID; }
	
	virtual void  SetGeometryID(uint32 ID) override { GeometryID = ID; }

	virtual bool IsMeshReady() const override { return bIsMeshReady; }
	
	virtual void SetIsMeshReady(bool bIsReady) override { bIsMeshReady = bIsReady; }

	virtual FChaosVDMeshReadyDelegate* OnMeshReady() override { return &MeshReadyDelegate; }
	
	virtual void SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject) override;
	
	virtual void UpdateVisibility() override;
	
	virtual void UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray) override;;
	// END IChaosVDGeometryDataComponent Interface

protected:
	
	UPROPERTY(VisibleAnywhere, Category="Geometry Data", meta=(FullyExpand = true))
	FChaosVDShapeCollisionData CollisionData;
};
