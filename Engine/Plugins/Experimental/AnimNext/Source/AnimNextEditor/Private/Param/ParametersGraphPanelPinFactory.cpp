// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersGraphPanelPinFactory.h"
#include "SGraphPinParamName.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"

namespace UE::AnimNext::Editor
{

FName FParametersGraphPanelPinFactory::GetFactoryName() const
{
	return TEXT("ParametersGraphPanelPinFactory");
}

TSharedPtr<SGraphPin> FParametersGraphPanelPinFactory::CreatePin_Internal(UEdGraphPin* InPin) const
{
	if (URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(InPin->GetOwningNode()))
	{
		URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(RigNode->GetGraph());

		URigVMPin* ModelPin = RigNode->GetModelPinFromPinPath(InPin->GetName());
		if (ModelPin)
		{
			if(ModelPin->GetCustomWidgetName() == "ParamName")
			{
				return SNew(SGraphPinParamName, InPin)
					.ModelPin(ModelPin)
					.GraphNode(RigNode);
			}
		}
	}

	return FRigVMEdGraphPanelPinFactory::CreatePin_Internal(InPin);
}

}
