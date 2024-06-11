// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraphNode.h"

#include "AnimNextEdGraphNode.generated.h"

// EdGraphNode representation for AnimNext nodes
// A node can hold a trait stack or a trait entry
UCLASS(MinimalAPI)
class UAnimNextEdGraphNode : public URigVMEdGraphNode
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// UEdGraphNode implementation
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	//////////////////////////////////////////////////////////////////////////
	// URigVMEdGraphNode implementation
	virtual void ConfigurePin(UEdGraphPin* EdGraphPin, const URigVMPin* ModelPin) const override;

	//////////////////////////////////////////////////////////////////////////
	// Our implementation

	// Returns whether this node is a trait stack or not
	ANIMNEXTUNCOOKEDONLY_API bool IsTraitStack() const;

private:
	// Populates the SubMenu with entries for each trait that can be added through the context menu
	void BuildAddTraitContextMenu(class UToolMenu* SubMenu);
};
