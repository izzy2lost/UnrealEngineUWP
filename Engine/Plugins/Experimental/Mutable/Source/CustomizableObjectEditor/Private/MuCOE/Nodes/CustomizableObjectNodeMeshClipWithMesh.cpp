// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithMesh.h"

#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeModifierClipWithMesh::UCustomizableObjectNodeModifierClipWithMesh() :
	CustomizableObjectToClipWith(nullptr)
{
	// We allow this node to be shared
	Transform = FTransform::Identity;

	// When initialize we don't use materials neither tags
	bUseMaterials = false;
	bUseTags = false;
}


void UCustomizableObjectNodeModifierClipWithMesh::BeginPostDuplicate(bool bDuplicateForPIE)
{
	Super::BeginPostDuplicate(bDuplicateForPIE);

	if (UCustomizableObjectGraph* Graph = Cast<UCustomizableObjectGraph>(GetGraph()))
	{
		TArray<FGuid> NotifiedNodes;
		for (const FGuid& NodeToClipWithId : ArrayMaterialNodeToClipWithID)
		{
			FGuid NodeToClipAlreadyUpdated = Graph->RequestNotificationForNodeIdChange(NodeToClipWithId, NodeGuid);

			if (NodeToClipWithId != NodeToClipAlreadyUpdated)
			{
				NotifiedNodes.Add(NodeToClipAlreadyUpdated);
			}
		}

		ArrayMaterialNodeToClipWithID = NotifiedNodes;
	}
}


void UCustomizableObjectNodeModifierClipWithMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ClipMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Clip Mesh"));
	ClipMeshPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* OutputPin_p = CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName("Modifier"));
}


void UCustomizableObjectNodeModifierClipWithMesh::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPinsMode)
{
	Super::ReconstructNode(RemapPinsMode);

	if (!CustomizableObjectToClipWith)
	{
		CustomizableObjectToClipWith = Cast<UCustomizableObject>(GetOutermostObject());
	}
}


FText UCustomizableObjectNodeModifierClipWithMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Clip_Mesh_With_Mesh", "Clip Mesh With Mesh");
}


void UCustomizableObjectNodeModifierClipWithMesh::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

	if (Editor.IsValid())
	{
		Editor->UpdateGraphNodeProperties();
	}
}


UEdGraphPin* UCustomizableObjectNodeModifierClipWithMesh::OutputPin() const
{
	return FindPin(TEXT("Modifier"));
}


UEdGraphPin* UCustomizableObjectNodeModifierClipWithMesh::ClipMeshPin() const
{
	return FindPin(TEXT("Clip Mesh"));
}


void UCustomizableObjectNodeModifierClipWithMesh::UpdateReferencedNodeId(const FGuid& NewGuid)
{
	ArrayMaterialNodeToClipWithID.Add(NewGuid);
}


FText UCustomizableObjectNodeModifierClipWithMesh::GetTooltipText() const
{
	return LOCTEXT("Clip_Mesh_Mesh_Tooltip", "Removes the part of a material that is completely enclosed in a mesh volume.\nIt only removes the faces that fall completely inside the cutting volume, along with the vertices and edges that define only faces that are deleted.");
}

#undef LOCTEXT_NAMESPACE
