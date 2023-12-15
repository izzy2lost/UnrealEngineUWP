// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "RemeshNode.generated.h"

/** Remesh the cloth surface(s) to get the specified mesh resolution(s).
 *  NOTE: Weight Maps, Skinning Data, Self Collision Spheres, and Long Range Attachment Constraints will be reconstructed on the output mesh, however all other Selections will be removed
 */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetRemeshNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetRemeshNode, "Remesh", "Cloth", "Cloth Remesh")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh")
	bool bRemeshSim = true;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta=(UIMin = "1", UIMax = "200", ClampMin = "1", EditCondition = "bRemeshSim"))
	int32 TargetPercentSim = 100;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshSim"))
	int32 IterationsSim = 10;

	UPROPERTY(EditAnywhere, Category = "Sim Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshSim"))
	double SmoothingSim = 0.25;

	UPROPERTY(EditAnywhere, Category = "Render Mesh")
	bool bRemeshRender = false;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta=(UIMin = "1", UIMax = "200", ClampMin = "1", EditCondition = "bRemeshRender"))
	int32 TargetPercentRender = 100;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0", UIMax = "20", ClampMin = "0", ClampMax = "100", EditCondition = "bRemeshRender"))
	int32 IterationsRender = 10;

	UPROPERTY(EditAnywhere, Category = "Render Mesh", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bRemeshRender"))
	double SmoothingRender = 0.25;

	FChaosClothAssetRemeshNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

private:

	void EmptySimSelections(const TSharedRef<FManagedArrayCollection>& ClothCollection) const;

	void RemeshSimMesh(const TSharedRef<const FManagedArrayCollection>& ClothCollection,
		const TSharedRef<FManagedArrayCollection>& OutClothCollection) const;

	void RebuildTopologyDependentSimData(const TSharedRef<const FManagedArrayCollection>& ClothCollection,
		const TSharedRef<FManagedArrayCollection>& OutClothCollection) const;

	void EmptyRenderSelections(const TSharedRef<FManagedArrayCollection>& ClothCollection) const;

	void RemeshRenderPattern(const TSharedRef<const FManagedArrayCollection>& ClothCollection,
		int32 PatternIndex,
		const TSharedRef<FManagedArrayCollection>& OutClothCollection) const;

};
