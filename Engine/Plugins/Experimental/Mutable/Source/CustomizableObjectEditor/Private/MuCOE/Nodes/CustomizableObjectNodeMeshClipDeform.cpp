// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipDeform.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMeshClipDeform::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	
	UEdGraphPin* ClipMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Clip Shape"));
	ClipMeshPin->bDefaultValueIsIgnored = true;
	
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName("Modifier"));
	ClipMeshPin->bDefaultValueIsIgnored = true;
}


UEdGraphPin* UCustomizableObjectNodeMeshClipDeform::ClipShapePin() const
{
	return FindPin(TEXT("Clip Shape"), EGPD_Input);
}


UEdGraphPin* UCustomizableObjectNodeMeshClipDeform::OutputPin() const
{
	return FindPin(TEXT("Modifier"));
}


FText UCustomizableObjectNodeMeshClipDeform::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Clip_Deform_Mesh", "Clip Deform Mesh");
}


FText UCustomizableObjectNodeMeshClipDeform::GetTooltipText() const
{
	return LOCTEXT("Clip_Deform_Tooltip", "Defines a clip with mesh deformation based on a shape mesh and blend weights.");

}
#undef LOCTEXT_NAMESPACE
