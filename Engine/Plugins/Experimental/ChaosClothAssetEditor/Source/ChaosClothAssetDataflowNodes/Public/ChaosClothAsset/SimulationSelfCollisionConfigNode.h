// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationSelfCollisionConfigNode.generated.h"

/** Self-collision repulsion forces (point-face) properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationSelfCollisionConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationSelfCollisionConfigNode, "SimulationSelfCollisionConfig", "Cloth", "Cloth Simulation Self Collision Config")

public:
	/** The radius of the spheres used in self collision (i.e., offset per side. total thickness of cloth is 2x this value). */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000", EditCondition = "bUseSelfCollisions"))
	float SelfCollisionThickness = 0.5f;

	/** The stiffness of the springs used to control self collision. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	float SelfCollisionStiffness = 0.5f;

	/** Friction coefficient for cloth - cloth interaction. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "10"))
	float SelfCollisionFriction = 0.0f;

	/** Enable self intersection resolution. This will try to fix any cloth intersections that are not handled by collision repulsions. */
	UPROPERTY(EditAnywhere, Category = "Experimental")
	bool bUseSelfIntersections = false;

	/** Do global intersection analysis to determine the correct normals for the collision springs */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections"))
	bool bUseGlobalIntersectionAnalysis = true;

	/** Do a step of contour minimization at the beginning of the timestep. */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections"))
	bool bUseContourMinimization = true;

	/** Number of post timestep contour minimization steps to do. (Expensive!)*/
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (ClampMin = "0", EditCondition = "bUseSelfIntersections"))
	int32 NumContourMinimizationPostSteps = 0;

	/** Use global contour gradients when doing post timestep contour minimization */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections && NumContourMinimizationPostSteps > 0"))
	bool bUseGlobalPostStepContours = true;

	FChaosClothAssetSimulationSelfCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const override;
};
