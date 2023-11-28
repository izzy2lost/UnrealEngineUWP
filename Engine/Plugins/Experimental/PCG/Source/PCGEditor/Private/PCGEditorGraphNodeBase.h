// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"


#include "PCGEditorGraphNodeBase.generated.h"

enum class EPCGChangeType : uint8;

struct FPCGStack;
class UPCGComponent;
class UPCGNode;
class UPCGPin;
class UToolMenu;

UCLASS()
class UPCGEditorGraphNodeBase : public UEdGraphNode
{
	GENERATED_BODY()

public:
	void Construct(UPCGNode* InPCGNode);

	// ~Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	// ~End UObject interface

	// ~Begin UEdGraphNode interface
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PrepareForCopying() override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual void ReconstructNode() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
	virtual void PostPasteNode() override;
	virtual FText GetTooltipText() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void OnUpdateCommentText(const FString& NewComment) override;
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible) override;
	virtual bool GetCanRenameNode() const override { return bCanRenameNode; }
	// ~End UEdGraphNode interface

	void OnUserAddDynamicInputPin();
	bool CanUserRemoveDynamicInputPin(UEdGraphPin* InPinToRemove);
	void OnUserRemoveDynamicInputPin(UEdGraphPin* InRemovedPin);
	
	UPCGNode* GetPCGNode() { return PCGNode; }
	const UPCGNode* GetPCGNode() const { return PCGNode; }
	void PostCopy();
	void RebuildAfterPaste();
	void PostPaste();

	void SetInspected(bool InIsInspecting) { bIsInspected = InIsInspecting; }
	bool GetInspected() const { return bIsInspected; }

	void SetOnActiveBranch(bool bInIsOnActiveBranch) { bIsOnActiveBranch = bInIsOnActiveBranch; }
	bool IsOnActiveBranch() const { return bIsOnActiveBranch; }

	/** Increase deferred reconstruct counter, calls to ReconstructNode will flag reconstruct to happen when count hits zero */
	void EnableDeferredReconstruct();
	/** Decrease deferred reconstruct counter, ReconstructNode will be called if counter hits zero and the node is flagged for reconstruction  */
	void DisableDeferredReconstruct();

	/** Pulls current errors/warnings state from PCG subsystem. */
	EPCGChangeType UpdateErrorsAndWarnings();

	/** If the currently inspected grid size is smaller than the grid size of this node, display transparent. */
	EPCGChangeType UpdateStructuralVisualization(class UPCGComponent* InComponentBeingDebugged, const FPCGStack* InStackBeingInspected);

	/** Puts node title on node body, reducing overall node size */
	bool ShouldDrawCompact() const;

	/** Returns custom compact node icon if available */
	bool GetCompactNodeIcon(FName& OutCompactNodeIcon) const;

	// Highlighted nodes draw with a light tint. Currently used to indicate nodes from hi-gen grids that are available for use.
	void SetIsHighlighted(bool bInIsHighlighted) { bIsHighlighted = bInIsHighlighted; }
	bool IsHighlighted() const { return bIsHighlighted; }

	bool CanUserAddRemoveDynamicInputPins() const { return bCanUserAddRemoveSourcePins; }

	/** Marks the node as re-nameable and provokes the node to update, placing the user in rename mode. */
	void EnterRenamingMode();

	/** Disables renaming, preventing the node from entering editing mode on subsequent updates. */
	void ExitRenamingMode();

	DECLARE_DELEGATE(FOnPCGEditorGraphNodeChanged);
	FOnPCGEditorGraphNodeChanged OnNodeChangedDelegate;

	DECLARE_DELEGATE(FOnPCGEditorGraphNodeRenameInitiated);
	FOnPCGEditorGraphNodeRenameInitiated OnNodeRenameInitiatedDelegate;

protected:
	static FEdGraphPinType GetPinType(const UPCGPin* InPin);

	/** Create PCG-side edges from editor pins/edges. */
	void RebuildEdgesFromPins();

	void OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType);
	void OnPickColor();
	void OnColorPicked(FLinearColor NewColor);
	void UpdateCommentBubblePinned();
	void UpdatePosition();

	void CreatePins(const TArray<UPCGPin*>& InInputPins, const TArray<UPCGPin*>& InOutputPins);

	// Custom logic to hide some pins to the user (by not creating a UI pin, even if the model pin exists).
	// Useful for deprecation
	virtual bool ShouldCreatePin(const UPCGPin* InPin) const;

	UPROPERTY()
	TObjectPtr<UPCGNode> PCGNode = nullptr;

	int32 DeferredReconstructCounter = 0;
	bool bDeferredReconstruct = false;
	bool bDisableReconstructFromNode = false;
	bool bIsInspected = false;
	bool bIsHighlighted = false;
	bool bCanUserAddRemoveSourcePins = false;
	bool bIsOnActiveBranch = true;
};
