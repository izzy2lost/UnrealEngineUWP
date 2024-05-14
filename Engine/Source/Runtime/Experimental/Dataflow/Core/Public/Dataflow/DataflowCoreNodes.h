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
	FDataflowAllTypes Value;

protected:
	virtual bool OnInputTypeChanged(const FDataflowInput* Input) override;
	virtual bool OnOutputTypeChanged(const FDataflowOutput* Input) override;

};

USTRUCT(meta=(Icon="GraphEditor.Branch_16x"))
struct FDataflowBranchNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowBranchNode, "Branch", "FlowControl", "")

public:
	FDataflowBranchNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const;

public:
	UPROPERTY(meta = (DataflowInput, DisplayName = "TrueValue"))
	FDataflowAllTypes TrueValue;

	UPROPERTY(meta = (DataflowInput, DisplayName = "FalseValue"))
	FDataflowAllTypes FalseValue;

	UPROPERTY(EditAnywhere, Category="Condition", meta = (DataflowInput, DisplayName = "Condition"))
	bool bCondition = true;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Result"))
	FDataflowAllTypes Result;

private:
	virtual bool OnInputTypeChanged(const FDataflowInput* Input) override;
	virtual bool OnOutputTypeChanged(const FDataflowOutput* Input) override;
};