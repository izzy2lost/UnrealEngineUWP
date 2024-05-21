// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "ChaosLog.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "Chaos/CacheCollection.h"
#include "Dataflow/DataflowSolverNodes.h"
#include "Components/SkeletalMeshComponent.h"

#include "ChaosFleshSimulationNodes.generated.h"

/**
 * UDataflowSkeletalMesh component
 */
UCLASS()
class CHAOSFLESHNODES_API UDataflowSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
public:
	UDataflowSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer);
	~UDataflowSkeletalMeshComponent();

	/** Flip space buffer if needed */
	void FlipSpaceBuffer();
};

/** Get the dataflow simulation time */
USTRUCT()
struct FDataflowSimulationTime 
{
	GENERATED_BODY()

public:
	FDataflowSimulationTime() : DeltaTime(0.0f), CurrentTime(0.0f)
	{}
	
	FDataflowSimulationTime(const float InDeltaTime, const float InCurrentTime) : DeltaTime(InDeltaTime), CurrentTime(InCurrentTime)
	{}
	
	/** Delta time in seconds property coming from the context */
	UPROPERTY(Transient, SkipSerialization)
	float DeltaTime = 0.0f;

	/** Current time in seconds property coming from the context */
	UPROPERTY(Transient, SkipSerialization)
	float CurrentTime = 0.0f;
};

/** Get the context simulation time */
USTRUCT(meta = (DataflowSimulation))
struct FGetSimulationTimeDataflowNode : public FDataflowInvalidNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetSimulationTimeDataflowNode, "GetSimulationTime", "Simulation|Update|Common", "")

public:
	
	FGetSimulationTimeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowInvalidNode(InParam, InGuid)
	{
		RegisterOutputConnection(&SimulationTime);
	}
	
	/** Simulation time property coming from the context */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	FDataflowSimulationTime SimulationTime = FDataflowSimulationTime(0.0f, 0.0f);

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Bind the skeletal mesh to the matching flesh component within the flesh solver */
USTRUCT(meta = (DataflowCache))
struct FBindFleshToSkeletonDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBindFleshToSkeletonDataflowNode, "BindFleshToSkeleton", "Simulation|Update|Flesh", "")

public:
	
	FBindFleshToSkeletonDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SkeletalMesh);
		RegisterInputConnection(&FleshSolver);
		RegisterOutputConnection(&FleshSolver, &FleshSolver);
	}

	/** Flesh solver from which we will bind one flesh component to the skeleton mesh */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "FleshSolver"))
	TObjectPtr<UDeformableSolverComponent> FleshSolver = nullptr;

	/** Skeleton mesh used for binding*/
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;
	
	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Main terminal node for flesh solver */
USTRUCT(meta = (DataflowCache))
struct FFleshSolverTerminalDataflowNode : public FDataflowCacheNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFleshSolverTerminalDataflowNode, "FleshSolverTerminal", "Simulation|Terminal|Flesh", "")

public:
	
	FFleshSolverTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowCacheNode(InParam, InGuid)
	{
		RegisterInputConnection(&FleshSolver);
		RegisterInputConnection(&TimeRange);
	}

	/** Cache asset that will store the cache datas */
	UPROPERTY(VisibleAnywhere, Category="Caching")
	mutable TObjectPtr<UChaosCacheCollection> CacheAsset = nullptr;

	/** Time range used for caching */
	UPROPERTY(EditAnywhere, Category="Caching", Meta = (DataflowInput))
	FVector2f TimeRange = FVector2f(0.0f, 5.0f);

	/** Flesh solver that will be cached */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TObjectPtr<UDeformableSolverComponent> FleshSolver = nullptr;
	
	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
	
	/** Get the cache asset linked to that terminal cache */
	virtual TObjectPtr<UObject> GetCacheAsset(Dataflow::FSimulationContext& SimulationContext) const override
	{
		return Cast<UObject>(GetValue<TObjectPtr<UChaosCacheCollection>>(SimulationContext, &CacheAsset));
	}
	
	/** Return the time range used to cache the datas */
	virtual FVector2f GetTimeRange(Dataflow::FSimulationContext& SimulationContext) const override
	{
		return GetValue<FVector2f>(SimulationContext, &TimeRange);
	}
};

/** Create flesh solver from settings */
USTRUCT(meta = (DataflowFlesh))
struct FCreateFleshSolverDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateFleshSolverDataflowNode, "CreateFleshSolver", "Simulation|Setup|Flesh", "")

public:
	
	FCreateFleshSolverDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterOutputConnection(&FleshSolver);
	}

	/** Flesh solver built from the settings */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	TObjectPtr<UDeformableSolverComponent> FleshSolver = nullptr;

	/** Solver timing properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverTimingGroup SolverTiming;

	/** Solver evolution properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverEvolutionGroup SolverEvolution;

	/** Solver collisions properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverCollisionsGroup SolverCollisions;

	/** Solver constraints properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverConstraintsGroup SolverConstraints;

	/** Solver forces properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverForcesGroup SolverForces;

	/** Solver debugging properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverDebuggingGroup SolverDebugging;

	/** Solver muscle activation properties */
	UPROPERTY(EditAnywhere, Category="Solver")
	FSolverMuscleActivationGroup SolverMuscleActivation;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Create flesh skeleton from settings */
USTRUCT(meta = (DataflowFlesh))
struct FCreateFleshSkeletonDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateFleshSkeletonDataflowNode, "CreateFleshSkeleton", "Simulation|Setup|Flesh", "")

public:
	
	FCreateFleshSkeletonDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&FleshComponent);
		RegisterOutputConnection(&SkeletalMesh);
	}

	/** Skeletal mesh component to be built */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;

	/** Flesh component from which the skeletal mesh will be constructed */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TObjectPtr<UFleshComponent> FleshComponent = nullptr;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};


/** Set a specified animation onto the skeleton mesh */
USTRUCT(meta = (DataflowFlesh))
struct FSetSkeletonAnimationDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSetSkeletonAnimationDataflowNode, "SetSkeletonAnimation", "Simulation|Setup|Skeleton", "")

public:
	
	FSetSkeletonAnimationDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SkeletalMesh);
		RegisterInputConnection(&AnimationAsset);
		RegisterOutputConnection(&SkeletalMesh, &SkeletalMesh);
	}

	/** Skeletal mesh component to be built */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "SkeletalMesh"))
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;

	/** Animation asset to be used  */
	UPROPERTY(EditAnywhere, Category="Animation", Meta = (DataflowInput))
	TObjectPtr<UAnimationAsset> AnimationAsset = nullptr;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Get the skeleton animation time range */
USTRUCT(meta = (DataflowFlesh))
struct FGetAnimationTimeRangeDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetAnimationTimeRangeDataflowNode, "GetAnimationTimeRange", "Simulation|Setup|Skeleton", "")

public:
	
	FGetAnimationTimeRangeDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SkeletalMesh);
		RegisterOutputConnection(&TimeRange);
	}

	/** Skeletal mesh component yo update at a given time */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;

	/** Time range in the skeletal mesh animation instance */
	UPROPERTY(Transient, SkipSerialization,  Meta = (DataflowOutput))
	FVector2f TimeRange = FVector2f(0.0f,5.0f);

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Update skeleton animation at a given time */
USTRUCT(meta = (DataflowFlesh))
struct FUpdateSkeletonAnimationDataflowNode : public FDataflowAnimationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FUpdateSkeletonAnimationDataflowNode, "UpdateSkeletonAnimation", "Simulation|Update|Skeleton", "")

public:
	
	FUpdateSkeletonAnimationDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowAnimationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SimulationTime);
		RegisterInputConnection(&SkeletalMesh);
		RegisterOutputConnection(&SkeletalMesh);
	}

	/** Skeletal mesh component yo update at a given time */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput))
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;

	/** Simulation time used to load the animation */
	UPROPERTY(Transient, SkipSerialization,  Meta = (DataflowInput))
	FDataflowSimulationTime SimulationTime = FDataflowSimulationTime(0.0f,0.0f);

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;

	/** Set the animation time */
	virtual void SetAnimationTime(Dataflow::FSimulationContext& SimulationContext, const float AnimationTime) override;
};

/** Create a flesh component given a flesh asset */
USTRUCT(meta = (DataflowFlesh))
struct FCreateFleshComponentDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateFleshComponentDataflowNode, "CreateFleshComponent", "Simulation|Setup|Flesh", "")

public:
	
	FCreateFleshComponentDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&FleshAsset);
		RegisterOutputConnection(&FleshComponent);
	}

	/** Flesh component built from the asset */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	TObjectPtr<UFleshComponent> FleshComponent = nullptr;

	/** Flesh asset */
	UPROPERTY(EditAnywhere, Category="Flesh", Meta = (DataflowInput))
	TObjectPtr<UFleshAsset> FleshAsset = nullptr;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Add flesh component to a flesh solver */
USTRUCT(meta = (DataflowFlesh))
struct FAddFleshToSolverDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAddFleshToSolverDataflowNode, "AddFleshToSolver", "Simulation|Setup|Flesh", "")

public:
	
	FAddFleshToSolverDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&FleshComponent);
		RegisterInputConnection(&FleshSolver);
		RegisterOutputConnection(&FleshSolver, &FleshSolver);
	}

	/** Flesh component to add to the solver */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TObjectPtr<UFleshComponent> FleshComponent = nullptr;

	/** Flesh solver */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "FleshSolver"))
	TObjectPtr<UDeformableSolverComponent> FleshSolver = nullptr;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Advance the flesh solver in time */
USTRUCT(meta = (DataflowFlesh))
struct FAdvanceFleshSolverDataflowNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAdvanceFleshSolverDataflowNode, "AdvanceFleshSolver", "Simulation|Update|Flesh", "")

public:
	
	FAdvanceFleshSolverDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SimulationTime);
		RegisterInputConnection(&FleshSolver);
		RegisterOutputConnection(&FleshSolver, &FleshSolver);
	}

	/** Delta time to use to advance the solver*/
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	FDataflowSimulationTime SimulationTime = FDataflowSimulationTime(0.0f,0.0f);

	/** Flesh solver */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "FleshSolver"))
	TObjectPtr<UDeformableSolverComponent> FleshSolver = nullptr;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;

private :
	/** Time offset to store the difference in between the delta time and the fixed time step */
	mutable float TimeOffset = 0.0f;
};

namespace Dataflow
{
	void RegisterChaosCommonSimulationNodes();
	void RegisterChaosFleshSimulationNodes();
	void RegisterChaosSkeletonSimulationNodes();
}
