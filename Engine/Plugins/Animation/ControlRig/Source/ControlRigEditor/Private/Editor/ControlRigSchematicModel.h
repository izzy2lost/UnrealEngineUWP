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

/** Node for the schematic view */
class FControlRigSchematicRigElementKeyNode : public FSchematicGraphNode
{
public:

	SCHEMATICGRAPHNODE_BODY(FControlRigSchematicRigElementKeyNode, FSchematicGraphNode)

	virtual ~FControlRigSchematicRigElementKeyNode() override {}
	
	const FRigElementKey& GetKey() const { return Key; }
	virtual FString GetDragDropDecoratorLabel() const override;
	virtual bool IsAutoScaleEnabled() const override { return true; }
	
protected:

	FRigElementKey Key;

	friend class FControlRigSchematicModel;
};

/** Model for the schematic views */
class FControlRigSchematicModel : public FSchematicGraphModel
{
public:
	virtual ~FControlRigSchematicModel() override;
	
	void SetEditor(const TSharedRef<FControlRigEditor>& InEditor);

	virtual void Reset() override;
	FControlRigSchematicRigElementKeyNode* AddElementKeyNode(const FRigElementKey& InKey, bool bNotify = true);
	const FControlRigSchematicRigElementKeyNode* FindElementKeyNode(const FRigElementKey& InKey) const;
	bool ContainsElementKeyNode(const FRigElementKey& InKey) const;
	virtual bool RemoveNode(const FGuid& InGuid) override;
	bool RemoveElementKeyNode(const FRigElementKey& InKey);

	void OnSetObjectBeingDebugged(UObject* InObject);
	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);
	void HandleModularRigModified(EModularRigNotification InNotification, const FRigModuleReference* InModule);

	virtual FVector2d GetPositionForNode(const FSchematicGraphNode* InNode) const override;
	virtual bool GetPositionAnimationEnabledForNode(const FSchematicGraphNode* InNode) const override;
	virtual const FSlateBrush* GetBrushForNode(const FSchematicGraphNode* InNode) const override;
	virtual FLinearColor GetColorForNode(const FSchematicGraphNode* InNode) const override;
	virtual const FText GetToolTipForNode(const FSchematicGraphNode* InNode) const override;
	virtual ESchematicGraphNodePlacementConstraint GetPlacementForNode(const FSchematicGraphNode* InNode) const override;

private:

	void HandleSchematicNodeClicked(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode);
	void HandleSchematicBeginDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation);
	void HandleSchematicEndDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation);
	void HandleSchematicDrop(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropEvent& InDragDropEvent);
	bool IsConnectorResolved(const FRigElementKey& InConnectorKey, FRigElementKey* OutKey = nullptr) const;
	
	TWeakPtr<FControlRigEditor> ControlRigEditor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UControlRig> ControlRigBeingDebuggedPtr;

	TArray<FGuid> TemporaryNodeGuids;
	TMap<FRigElementKey, FGuid> RigElementKeyToGuid;

	friend class FControlRigEditor;
};