// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodeGraphProvider.h"
#include "IOptimusNodePinRouter.h"
#include "OptimusNode.h"

#include "OptimusNode_SubGraphReference.generated.h"


class UOptimusNodeSubGraph;


UCLASS(Hidden)
class UOptimusNode_SubGraphReference :
	public UOptimusNode,
	public IOptimusNodePinRouter,
	public IOptimusNodeGraphProvider
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

	UOptimusNodeGraph* GetNodeGraphToShow() override;

	UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const;
	
protected:
	friend class UOptimusNodeGraph;

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
