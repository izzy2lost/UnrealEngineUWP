// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughMesh.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodePassThroughMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Mesh");
	UEdGraphPin* PinMeshPin = CustomCreatePin(EGPD_Output, Schema->PC_PassThroughMesh, FName(*PinName));
	PinMeshPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodePassThroughMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Mesh.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MeshName"), FText::FromString(Mesh.GetAssetName()));

		return FText::Format(LOCTEXT("PassThrough Mesh_Title", "{MeshName}\nPassThrough Mesh"), Args);
	}
	else
	{
		return LOCTEXT("PassThrough Mesh", "PassThrough Mesh");
	}
}


FLinearColor UCustomizableObjectNodePassThroughMesh::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_PassThroughMesh);
}


FText UCustomizableObjectNodePassThroughMesh::GetTooltipText() const
{
	return LOCTEXT("PassThrough_Mesh_Tooltip", "Defines a pass-through Mesh. It will not be modified by Mutable in any way, just referenced as a UE asset. It's much cheaper than a Mutable Mesh, but you cannot make any operations on it, just switch it.");
}

#undef LOCTEXT_NAMESPACE
