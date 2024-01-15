// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationSolverConfigNode.generated.h"

/** Solver properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationSolverConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationSolverConfigNode, "SimulationSolverConfig", "Cloth", "Cloth Simulation Solver Config")

public:
	/**
	 * The number of time step dependent solver iterations. This sets iterations at 60fps.
	 * NumIterations can never be bigger than MaxNumIterations.
	 * At lower fps up to MaxNumIterations may be used instead. At higher fps as low as one single iteration might be used.
	 * Higher number of iterations will increase the stiffness of all constraints and improve convergence, but will also increase the CPU cost of the simulation.
	 */
	UPROPERTY(EditAnywhere, Category = Simulation, meta = (UIMin = "1", UIMax = "10", ClampMin = "0", ClampMax = "100"))
	int32 NumIterations = 1;

	/**
	 * The maximum number of solver iterations.
	 * This is the upper limit of the number of iterations set in solver, when the frame rate is lower than 60fps.
	 */
	UPROPERTY(EditAnywhere, Category = Simulation, meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "100"))
	int32 MaxNumIterations = 6;

	/**
	 * The number of solver substeps.
	 * This will increase the precision of the collision inputs and help with constraint resolutions but will increase the CPU cost.
	 */
	UPROPERTY(EditAnywhere, Category = Simulation, meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "100"))
	int32 NumSubsteps = 1;


	/**
	* Enable setting separate SelfCollisionSubsteps. Otherwise, self collisions will be detected every substep.
	*/
	UPROPERTY(EditAnywhere, Category = Simulation, meta = (InlineEditConditionToggle))
	bool bEnableNumSelfCollisionSubsteps = false;

	/**
	 * Set a separate number of self collision substeps. Lower this number to increase speed at the expense of lower self collision accuracy.
	 * Actual value always clamped between [1, NumSubsteps].
	 */
	UPROPERTY(EditAnywhere, Category = Simulation, meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "100", EditCondition = "bEnableNumSelfCollisionSubsteps"))
	int32 NumSelfCollisionSubsteps = 1;

	/**
	 * Enable the higher accuracy force-based solver (experimental).
	 */
	UPROPERTY(EditAnywhere, Category = Experimental)
	bool bEnableForceBasedSolver = false;
	
	/**
	 * Number of Newton iterations for force-based solver. Prototype only--very few constraints support this.
	 */
	UPROPERTY(EditAnywhere, Category = Experimental, meta = (UIMin = "1", UIMax = "10", ClampMin = "0", ClampMax = "100", EditCondition = "bEnableForceBasedSolver"))
	int32 NumNewtonIterations = 1;

	/**
	 * Max number of CG Iterations per linear solve
	 */
	UPROPERTY(EditAnywhere, Category = Experimental, meta = (UIMin = "1", UIMax = "100", ClampMin = "1", EditCondition = "bEnableForceBasedSolver"))
	int32 MaxNumCGIterations = 50;

	/**
	 * CG Tolerance
	 */
	UPROPERTY(EditAnywhere, Category = Experimental, meta = (ClampMin = "0", EditCondition = "bEnableForceBasedSolver"))
	float CGResidualTolerance = 1e-4;

	/**
	 * Solve quasistaticly (no inertia) 
	 */
	UPROPERTY(EditAnywhere, Category = Experimental, meta = (EditCondition = "bEnableForceBasedSolver"))
	bool bDoQuasistatics = false;

	FChaosClothAssetSimulationSolverConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;
};
