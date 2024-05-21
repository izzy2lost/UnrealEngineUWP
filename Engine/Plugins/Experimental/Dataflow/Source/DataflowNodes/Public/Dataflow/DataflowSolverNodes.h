// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowObjectInterface.h"

#include "DataflowSolverNodes.generated.h"

/**
* FDataflowSimulationNode
*		Base class for simulation nodes within the Dataflow graph. 
* 
*		Simulation nodes are used to simulate data from the calling client. 
*/
USTRUCT()
struct FDataflowSimulationNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

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
		if(Context.IsA(Dataflow::FSimulationContext::StaticType()))
		{
			Dataflow::FSimulationContext& SimulationContext = StaticCast<Dataflow::FSimulationContext&>(Context);
			EvaluateSimulation(SimulationContext, Output);
			
		}
	}

	protected :

	/** Evaluate simulation node given a simulation context */
	virtual void EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const {};
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
	GENERATED_USTRUCT_BODY()

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
* FDataflowCacheNode
*		Base class for cache nodes within the Dataflow graph. 
* 
*		Cache nodes are used to provide an interface to cache data out from the calling client. 
*/
USTRUCT()
struct FDataflowCacheNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()

	FDataflowCacheNode()
		: Super() { }

	FDataflowCacheNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Super(Param,InGuid) {
	}

	/** FDataflowNode interface */
	virtual ~FDataflowCacheNode() {}
	
	static FName StaticType() { return FName("FDataflowCacheNode"); }

	virtual bool IsA(FName InType) const override 
	{ 
		return InType.ToString().Equals(StaticType().ToString()) 
			|| Super::IsA(InType); 
	}

	/** Return the cache asset that we are building */
	virtual TObjectPtr<UObject> GetCacheAsset(Dataflow::FSimulationContext& SimulationContext) const {return nullptr;} 
	
	/** Return the time range used to cache the datas */
	virtual FVector2f GetTimeRange(Dataflow::FSimulationContext& SimulationContext) const {return FVector2f(0,5.0f);}
};

/**
* FDataflowAnimationNode
*		Base class for animtion nodes within the Dataflow graph. 
* 
*		Animation nodes are used to update some animtion datas  from the calling client. 
*/
USTRUCT()
struct FDataflowAnimationNode : public FDataflowSimulationNode
{
	GENERATED_USTRUCT_BODY()

	FDataflowAnimationNode()
		: Super() { }

	FDataflowAnimationNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Super(Param,InGuid) {
	}

	/** FDataflowNode interface */
	virtual ~FDataflowAnimationNode() {}
	
	static FName StaticType() { return FName("FDataflowAnimationNode"); }

	virtual bool IsA(FName InType) const override 
	{ 
		return InType.ToString().Equals(StaticType().ToString()) 
			|| Super::IsA(InType); 
	}
	
	/** Set the animation time */
	virtual void SetAnimationTime(Dataflow::FSimulationContext& SimulationContext, const float AnimationTime) {}
};





