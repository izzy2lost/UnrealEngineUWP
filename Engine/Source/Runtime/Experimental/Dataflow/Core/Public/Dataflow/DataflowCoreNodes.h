// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include "DataflowCoreNodes.generated.h"

struct FDataflowOutput;

USTRUCT()
struct FDataflowReRouteNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowReRouteNode, "ReRouteNode", "Core", "")

public:
	FDataflowReRouteNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const;

public:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Value", DisplayName = "Value"))
	FDataflowAnyType Value;
};

USTRUCT()
struct FDataflowBranchNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowBranchNode, "Branch", "FlowControl", "")

public:
	FDataflowBranchNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const;

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "TrueValue"))
	FDataflowAnyType TrueValue;

	UPROPERTY(meta = (DataflowInput, DisplayName = "FalseValue"))
	FDataflowAnyType FalseValue;

	UPROPERTY(meta = (DataflowInput, DisplayName = "Condition"))
	bool bCondition = true;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Result"))
	FDataflowAnyType Result;
};