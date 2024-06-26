// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCustomizableObjectNode.h"

#include "SCustomizableObjectNodePin.h"


void SCustomizableObjectNode::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
    GraphNode = InGraphNode;
    UpdateGraphNode();
}


TSharedPtr<SGraphPin> SCustomizableObjectNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	return SNew(SCustomizableObjectNodePin, Pin);
}
