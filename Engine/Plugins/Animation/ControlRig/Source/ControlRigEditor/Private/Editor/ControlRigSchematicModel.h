// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularRigModel.h"
#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "Rigs/RigHierarchyDefines.h"
#include "ControlRigBlueprint.h"

class FControlRigEditor;
class UControlRig;
class URigHierarchy;
struct FRigBaseElement;

/** Model for the schematic views */
struct FControlRigSchematicModel : FSchematicGraph
{
	~FControlRigSchematicModel();
	
	void SetEditor(const TSharedRef<FControlRigEditor>& InEditor);

	void OnSetObjectBeingDebugged(UObject* InObject);
	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);
	void HandleModularRigModified(EModularRigNotification InNotification, const FRigModuleReference* InModule);

	void HandleUpdateSchematicNodes(SSchematicGraphPanel* InPanel, TSharedPtr<SSchematicGraphNode> InNode);
	void HandleSchematicNodeClicked(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode);
	void HandleSchematicBeginDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation);
	void HandleSchematicEndDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation);
	void HandleSchematicDrop(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropEvent& InDragDropEvent);

private:
	
	TWeakPtr<FControlRigEditor> ControlRigEditor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UControlRig> ControlRigBeingDebuggedPtr;

	TArray<FString> TemporaryNodes;
};