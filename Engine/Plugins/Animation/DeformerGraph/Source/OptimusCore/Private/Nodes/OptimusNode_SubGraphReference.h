// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodeGraphProvider.h"
#include "IOptimusNodePinRouter.h"
#include "IOptimusNodeSubGraphReferencer.h"
#include "OptimusNode.h"

#include "OptimusNode_SubGraphReference.generated.h"


class UOptimusNodeSubGraph;


UCLASS(Hidden)
class OPTIMUSCORE_API UOptimusNode_SubGraphReference :
	public UOptimusNode,
	public IOptimusNodePinRouter,
	public IOptimusNodeGraphProvider,
	public IOptimusNodeSubGraphReferencer
{
	GENERATED_BODY()

public:
	UOptimusNode_SubGraphReference();

	// UOptimusNode overrides
	FName GetNodeCategory() const override { return NAME_None; }
	FText GetDisplayName() const override;
	void ConstructNode() override;

	// UObject overrides
	void PostLoad() override;
	void BeginDestroy() override;

	// IOptimusNodePinRouter implementation
	FOptimusRoutedNodePin GetPinCounterpart(
		UOptimusNodePin* InNodePin,
		const FOptimusPinTraversalContext& InTraversalContext
	) const override;

	// IOptimusNodeGraphProvider
	UOptimusNodeGraph* GetNodeGraphToShow() override;

	// IOptimusNodeSubGraphReferencer
	UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const override;

protected:
	friend class UOptimusNodeGraph;
	friend class UOptimusDeformer;

	void SubscribeToSubGraph();
	void UnsubscribeFromSubGraph() const;

	void AddPinForNewBinding(FName InBindingArrayPropertyName);
	void RemoveStalePins(FName InBindingArrayPropertyName);
	void OnBindingMoved(FName InBindingArrayPropertyName);
	void RecreateBindingPins(FName InBindingArrayPropertyName);
	void SyncPinsToBindings(FName InBindingArrayPropertyName);

	TArray<UOptimusNodePin*> GetBindingPinsByDirection(EOptimusNodePinDirection InDirection);
	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node.
	 */
	UPROPERTY()
	TWeakObjectPtr<UOptimusNodeSubGraph> SubGraph;

	UPROPERTY()
	TWeakObjectPtr<UOptimusNodePin> DefaultComponentPin;

	
};
