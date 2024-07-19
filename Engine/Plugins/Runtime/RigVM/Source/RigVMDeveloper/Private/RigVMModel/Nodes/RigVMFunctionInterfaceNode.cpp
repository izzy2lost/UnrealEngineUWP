// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionInterfaceNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionInterfaceNode)

uint32 URigVMFunctionInterfaceNode::GetStructureHash() const
{
	// Avoid hashing the template for library nodes
	return URigVMNode::GetStructureHash();
}

FLinearColor URigVMFunctionInterfaceNode::GetNodeColor() const
{
	if(URigVMGraph* RootGraph = GetRootGraph())
	{
		if(RootGraph->IsA<URigVMFunctionLibrary>())
		{
			return FLinearColor(FColor::FromHex("CB00FFFF"));
		}
	}
	return FLinearColor(FColor::FromHex("005DFFFF"));
}

bool URigVMFunctionInterfaceNode::IsDefinedAsVarying() const
{ 
	return true; 
}

FText URigVMFunctionInterfaceNode::GetToolTipText() const
{
	return FText::FromName(GetGraph()->GetOuter()->GetFName());
}

FText URigVMFunctionInterfaceNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	if(const URigVMPin* OuterPin = FindReferencedPin(InPin))
	{
		return OuterPin->GetToolTipText();
	}
	return Super::GetToolTipTextForPin(InPin);
}

FName URigVMFunctionInterfaceNode::GetDisplayNameForPin(const FString& InPinPath) const
{
	if(const URigVMPin* OuterPin = FindReferencedPin(InPinPath))
	{
		return OuterPin->GetDisplayName();
	}
	return Super::GetDisplayNameForPin(InPinPath);
}

FString URigVMFunctionInterfaceNode::GetCategoryForPin(const FString& InPinPath) const
{
	if(const URigVMPin* OuterPin = FindReferencedPin(InPinPath))
	{
		return OuterPin->GetCategory();
	}
	return Super::GetCategoryForPin(InPinPath);
}

int32 URigVMFunctionInterfaceNode::GetIndexInCategoryForPin(const FString& InPinPath) const
{
	if(const URigVMPin* OuterPin = FindReferencedPin(InPinPath))
	{
		return OuterPin->GetIndexInCategory();
	}
	return Super::GetIndexInCategoryForPin(InPinPath);
}

TArray<FString> URigVMFunctionInterfaceNode::GetPinCategories() const
{
	if(const URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(GetGraph()->GetOuter()))
	{
		return OuterNode->GetPinCategories();
	}
	return PinCategories;
}

const URigVMPin* URigVMFunctionInterfaceNode::FindReferencedPin(const URigVMPin* InPin) const
{
	return FindReferencedPin(InPin->GetSegmentPath(true));
}

const URigVMPin* URigVMFunctionInterfaceNode::FindReferencedPin(const FString& InPinPath) const
{
	if(const URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(GetGraph()->GetOuter()))
	{
		return OuterNode->FindPin(InPinPath);
	}
	return nullptr;
}
