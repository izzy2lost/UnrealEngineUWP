// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowObject.h"
#include "UObject/ObjectPtr.h"
#include "Dataflow/DataflowSimulationProxy.h"
#include "Dataflow/DataflowSimulationContext.h"

#include "DataflowSimulationNodes.generated.h"

/**
 * Dataflow simulation property 
 */
USTRUCT(BlueprintType)
struct DATAFLOWSIMULATION_API FDataflowSimulationProperty
{
	GENERATED_BODY()

	/* Simulation proxy used to pass information in between nodes */
	FDataflowSimulationProxy* SimulationProxy;
};

/**
* FDataflowSimulationNode
*		Base class for simulation nodes within the Dataflow graph. 
* 
*		Simulation nodes are used to simulate data from the calling client. 
*/
USTRUCT()
struct FDataflowSimulationNode : public FDataflowNode
{
	GENERATED_BODY()

	FDataflowSimulationNode()
		: Super() { }

	FDataflowSimulationNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Super(Param,InGuid) {
	}

	/** FDataflowNode interface */
	virtual ~FDataflowSimulationNode() {}
	
	static FName StaticType() { return FName("FDataflowSimulationNode"); }

	virtual bool IsA(FName InType) const override 
	{ 
		return InType.ToString().Equals(StaticType().ToString()) 
			|| Super::IsA(InType); 
	}

	/** Evaluate simulation dispatch */
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Output) const override
	{
		if(Context.IsA(Dataflow::FDataflowSimulationContext::StaticType()))
		{
			Dataflow::FDataflowSimulationContext& SimulationContext = StaticCast<Dataflow::FDataflowSimulationContext&>(Context);
			EvaluateSimulation(SimulationContext, Output);
			
		}
	}

	protected :

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const {};
};

/**
* FDataflowInvalidNode
*		Base class for invalid nodes within the Dataflow graph. 
* 
*		Invalid nodes will be always invalidated while simulating
*/
USTRUCT()
struct FDataflowInvalidNode : public FDataflowSimulationNode
{
	GENERATED_BODY()

	FDataflowInvalidNode()
		: Super() { }

	FDataflowInvalidNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Super(Param,InGuid) {
	}

	/** FDataflowNode interface */
	virtual ~FDataflowInvalidNode() {}
	
	static FName StaticType() { return FName("FDataflowInvalidNode"); }

	virtual bool IsA(FName InType) const override 
	{ 
		return InType.ToString().Equals(StaticType().ToString()) 
			|| Super::IsA(InType); 
	}
};

/**
* FDataflowExecutionNode
*		Base class for the execute the dataflow simulation graph. 
* 
*		Execution nodes are used to pull the graph from the calling client. 
*/
USTRUCT()
struct FDataflowExecutionNode : public FDataflowSimulationNode
{
	GENERATED_BODY()

	FDataflowExecutionNode()
		: Super() { }

	FDataflowExecutionNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Super(Param,InGuid) {
	}

	/** FDataflowNode interface */
	virtual ~FDataflowExecutionNode() {}
	
	static FName StaticType() { return FName("FDataflowExecutionNode"); }

	virtual bool IsA(FName InType) const override 
	{ 
		return InType.ToString().Equals(StaticType().ToString()) 
			|| Super::IsA(InType); 
	}
};

/** Get the dataflow simulation time */
USTRUCT()
struct DATAFLOWSIMULATION_API FDataflowSimulationTime 
{
	GENERATED_BODY()

public:
	FDataflowSimulationTime(): DeltaTime(0.0f), CurrentTime(0.0f)
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
struct DATAFLOWSIMULATION_API FGetSimulationTimeDataflowNode : public FDataflowInvalidNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetSimulationTimeDataflowNode, "GetSimulationTime", "Physics|Common", UDataflow::SimulationTag)

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
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Main terminal node for flesh solver */
USTRUCT(meta = (DataflowSimulation))
struct FSimulationProxiesTerminalDataflowNode : public FDataflowExecutionNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSimulationProxiesTerminalDataflowNode, "SimulationProxiesTerminal", "Terminal|Common", UDataflow::SimulationTag)

public:
	
	FSimulationProxiesTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowExecutionNode(InParam, InGuid)
	{
		RegisterInputConnection(&SimulationProxies);
	}

	/** Physics solvers to evaluate  */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TArray<FDataflowSimulationProperty> SimulationProxies;
	
	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Get physics solvers from context */
USTRUCT(meta = (DataflowSimulation))
struct FGetPhysicsSolversDataflowNode : public FDataflowInvalidNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetPhysicsSolversDataflowNode, "GetPhysicsSolvers", "Physics|Solver", UDataflow::SimulationTag)

public:
	
	FGetPhysicsSolversDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowInvalidNode(InParam, InGuid)
	{
		RegisterOutputConnection(&PhysicsSolvers);
	}

	/** Physics solvers coming from the context and filtered with the groups */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	TArray<FDataflowSimulationProperty> PhysicsSolvers;

	/** Simulation groups to filter the output solvers properties */
	UPROPERTY(EditAnywhere, Category="Simulation")
	TArray<FString> SimulationGroups;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Filter simulation proxies from context */
USTRUCT(meta = (DataflowSimulation))
struct FFilterSimulationProxiesDataflowNode : public FDataflowSimulationNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFilterSimulationProxiesDataflowNode, "FilterSimulationProxies", "Physics|Common", UDataflow::SimulationTag)

public:
	
	FFilterSimulationProxiesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SimulationProxies);
		RegisterOutputConnection(&FilteredProxies);
	}

	/** Simulation proxies coming from the context and filtered with the groups */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	TArray<FDataflowSimulationProperty> SimulationProxies;

	/** Simulation proxies coming from the context and filtered with the groups */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowOutput))
	TArray<FDataflowSimulationProperty> FilteredProxies;

	/** Simulation groups to filter the output solvers properties */
	UPROPERTY(EditAnywhere, Category="Simulation")
	TArray<FString> SimulationGroups;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

/** Advance the simulation physics solver in time */
USTRUCT(meta = (DataflowSimulation))
struct FAdvancePhysicsSolversDataflowNode : public FDataflowSimulationNode
{
	GENERATED_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FAdvancePhysicsSolversDataflowNode, "AdvancePhysicsSolvers", "Physics|Solver", UDataflow::SimulationTag)

public:
	
	FAdvancePhysicsSolversDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowSimulationNode(InParam, InGuid)
	{
		RegisterInputConnection(&SimulationTime);
		RegisterInputConnection(&PhysicsSolvers);
		RegisterOutputConnection(&PhysicsSolvers, &PhysicsSolvers);
	}

	/** Delta time to use to advance the solver*/
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput))
	FDataflowSimulationTime SimulationTime = FDataflowSimulationTime(0.0f,0.0f);

	/** Physics solvers to advance in time */
	UPROPERTY(Transient, SkipSerialization, Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "PhysicsSolvers"))
	TArray<FDataflowSimulationProperty> PhysicsSolvers;

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FDataflowSimulationContext& SimulationContext, const FDataflowOutput* Output) const override;
};

namespace Dataflow
{
	void RegisterDataflowSimulationNodes();
}


