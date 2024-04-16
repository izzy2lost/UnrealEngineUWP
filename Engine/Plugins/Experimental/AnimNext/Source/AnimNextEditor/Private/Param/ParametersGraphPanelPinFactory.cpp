// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersGraphPanelPinFactory.h"
#include "SGraphPinParam.h"
#include "SGraphPinParamName.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextEditorParam.h"

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
		URigVMPin* ModelPin = RigNode->GetModelPinFromPinPath(InPin->GetName());
		if (ModelPin)
		{
			if(InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && (InPin->PinType.PinSubCategoryObject.Get() == FAnimNextEditorParam::StaticStruct() || InPin->PinType.PinSubCategoryObject.Get() == FAnimNextParam::StaticStruct()))
			{
				const FString ParamTypeString = ModelPin->GetMetaData("AllowedParamType");
				FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
				return SNew(SGraphPinParam, InPin)
					.ModelPin(ModelPin)
					.GraphNode(RigNode)
					.FilterType(FilterType);
			}
			else if(ModelPin->GetCustomWidgetName() == "ParamName")
			{
				const FString ParamTypeString = ModelPin->GetMetaData("AllowedParamType");
				FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
				return SNew(SGraphPinParamName, InPin)
					.ModelPin(ModelPin)
					.GraphNode(RigNode)
					.FilterType(FilterType);
			}
		}
	}
	else if(UEdGraphNode* EdGraphNode = InPin->GetOwningNode())
	{
		if(InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && (InPin->PinType.PinSubCategoryObject.Get() == FAnimNextEditorParam::StaticStruct() || InPin->PinType.PinSubCategoryObject.Get() == FAnimNextParam::StaticStruct()))
		{
			const FString ParamTypeString = EdGraphNode->GetPinMetaData(InPin->GetFName(), "AllowedParamType");
			FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
			return SNew(SGraphPinParam, InPin)
				.FilterType(FilterType);
		}
		else if(EdGraphNode->GetPinMetaData(InPin->GetFName(), "CustomWidget") == "ParamName")
		{
			const FString ParamTypeString = EdGraphNode->GetPinMetaData(InPin->GetFName(), "AllowedParamType");
			FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
			return SNew(SGraphPinParamName, InPin)
				.FilterType(FilterType);
		}
	}

	return FRigVMEdGraphPanelPinFactory::CreatePin_Internal(InPin);
}

}
