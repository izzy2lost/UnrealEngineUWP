// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "AnimNextGraph_EdGraphNode.h"
#include "AnimNextGraph_EdGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraphSchema : public URigVMEdGraphSchema
{
	GENERATED_BODY()

	// URigVMEdGraphSchema interface
	virtual TSubclassOf<URigVMEdGraphNode> GetGraphNodeClass(const URigVMEdGraph* InGraph) const override { return UAnimNextGraph_EdGraphNode::StaticClass(); }
	virtual bool IsStructEditable(UStruct* InStruct) const override;

	// UEdGraphSchema interface
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
};