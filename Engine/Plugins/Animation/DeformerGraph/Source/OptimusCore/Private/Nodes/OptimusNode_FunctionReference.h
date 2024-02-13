// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IOptimusNodeGraphProvider.h"
#include "IOptimusNodePinRouter.h"
#include "IOptimusNodeSubGraphReferencer.h"
#include "OptimusFunctionNodeGraph.h"
#include "OptimusFunctionNodeGraphHeader.h"
#include "OptimusNode.h"

#include "OptimusNode_FunctionReference.generated.h"


class UOptimusFunctionNodeGraph;

UCLASS(Hidden)
class UOptimusNode_FunctionReference :
	public UOptimusNode,
	public IOptimusNodePinRouter,
	public IOptimusNodeGraphProvider,
	public IOptimusNodeSubGraphReferencer
{
	GENERATED_BODY()

public:
	// UOptimusNode overrides
	FName GetNodeCategory() const override;
	FText GetDisplayName() const override;
	void ConstructNode() override;

	// UObject
	void PostLoad() override;
	
	// IOptimusNodePinRouter implementation
	FOptimusRoutedNodePin GetPinCounterpart(
		UOptimusNodePin* InNodePin,
		const FOptimusPinTraversalContext& InTraversalContext
	) const override;

	// IOptimusNodeGraphProvider
	UOptimusNodeGraph* GetNodeGraphToShow() override;
	
	// IOptimusNodeSubGraphReferencer
	UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const override;

	FSoftObjectPath GetSerializedGraphPath() const;

	void SetSerializedGraphPath(const FSoftObjectPath& InNewGraphPath);
	
protected:
	friend class UOptimusNodeGraph;
	friend class FOptimusCoreModule;

	
	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node.
	 */
	UPROPERTY()
	TSoftObjectPtr<UOptimusFunctionNodeGraph> FunctionGraph;

	UPROPERTY()
	TWeakObjectPtr<UOptimusNodePin> DefaultComponentPin;
};
