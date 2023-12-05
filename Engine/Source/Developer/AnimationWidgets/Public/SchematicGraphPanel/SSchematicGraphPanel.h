// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#pragma once

#include "CoreMinimal.h"
#include "SNodePanel.h"
#include "TickableEditorObject.h"
#include "Animation/AnimatedAttribute.h"

struct ANIMATIONWIDGETS_API FSchematicGraphNode
{
	FString Name;
	bool bIsSelected = false;
};

DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeAdded, FSchematicGraphNode*);
DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeRemoved, FSchematicGraphNode*);
DECLARE_EVENT(FSchematicGraph, FOnGraphReset);

class ANIMATIONWIDGETS_API FSchematicGraph
{
public:
	TArray<FSchematicGraphNode*> Nodes;
	TMap<FSchematicGraphNode*, TArray<FSchematicGraphNode*>> Links;

	void Reset();
	bool AddNode(const FString& InName);
	bool RenameNode(const FString& InOldName, const FString& InNewName);
	bool RemoveNode(const FString& InName);

	FOnNodeAdded OnNodeAddedDelegate;
	FOnNodeAdded& OnNodeAdded() { return OnNodeAddedDelegate; }
	FOnNodeRemoved OnNodeRemovedDelegate;
	FOnNodeRemoved& OnNodeRemoved() { return OnNodeRemovedDelegate; }
	FOnGraphReset OnGraphResetDelegate;
	FOnGraphReset& OnGraphReset() { return OnGraphResetDelegate; }
};

class ANIMATIONWIDGETS_API SSchematicGraphNode : public SNodePanel::SNode
{
public:
	DECLARE_DELEGATE_OneParam(FOnClicked, SSchematicGraphNode*);
	DECLARE_DELEGATE_TwoParams(FOnDrop, SSchematicGraphNode*, const FDragDropEvent&);

	SLATE_BEGIN_ARGS(SSchematicGraphNode){}
	SLATE_ARGUMENT(FSchematicGraphNode*, NodeData)
	SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_EVENT(FOnDrop, OnDrop)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual FVector2d GetPosition() const override;

	void SetPosition(const FVector2d& InPosition, bool bImmediately = false)
	{
		bImmediately ?
			Position->SetValueAndStop(FVector2d(InPosition))
		: Position->Set(InPosition);
	}
	
	FVector2d OriginalSize = FVector2d(50.0,50.0);
	float ScaledUp = 1.25;
	float ScaledDown = 0.75;

	FOnClicked OnClickedDelegate;
	FOnDrop OnDropDelegate;
	FSchematicGraphNode* NodeData;
	TSharedPtr<TAnimatedAttribute<FVector2d>> Position;
	TSharedPtr<TAnimatedAttribute<FVector2d>> Size;
	TSharedPtr<TAnimatedAttribute<float>> Scale;
	FSlateBrush Brush;
};


/** Widget allowing editing of a control rig's structure */
class ANIMATIONWIDGETS_API SSchematicGraphPanel : public SNodePanel, public FTickableEditorObject
{
public:
	
	DECLARE_DELEGATE_TwoParams(FUpdateNodeWidget, SSchematicGraphPanel*, TSharedPtr<SSchematicGraphNode>);
	DECLARE_DELEGATE_TwoParams(FOnNodeClicked, SSchematicGraphPanel*, SSchematicGraphNode*);
	DECLARE_DELEGATE_ThreeParams(FOnDrop, SSchematicGraphPanel*, SSchematicGraphNode*, const FDragDropEvent&);
	
	SLATE_BEGIN_ARGS(SSchematicGraphPanel) {}
	SLATE_ARGUMENT(bool, IsOverlay)
	SLATE_ARGUMENT(FSchematicGraph*, GraphData)
	SLATE_EVENT(FUpdateNodeWidget, OnUpdateNodeWidget)
	SLATE_EVENT(FOnNodeClicked, OnNodeClicked)
	SLATE_EVENT(FOnDrop, OnDrop)
	SLATE_END_ARGS()

	~SSchematicGraphPanel(){}

	void SetSchematicGraph(FSchematicGraph* InGraphData);
	void Construct(const FArguments& InArgs);

	void RebuildPanel();
	void AddNode(FSchematicGraphNode* NodeToAdd);
	void RemoveNode(FSchematicGraphNode* NodeToRemove);

	// SNodePanel interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	// End of SNodePanel interface
	
	TSharedRef<SSchematicGraphNode> GetChild(int32 ChildIndex) const;

	// FTickableEditorObject Interface
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool IsTickable() const override { return true; }
	// End of FTickableEditorObject interface

	void OnNodeClicked(SSchematicGraphNode* Node);
	void OnDropEvent(SSchematicGraphNode* Node, const FDragDropEvent& InDragDropEvent);

	void SetFadeBackground(bool bInFade) { FadeBackgroundAlpha->Set(bInFade ? 0.5f : 0.f); }

	bool bIsOverlay;
	TSharedPtr<TAnimatedAttribute<float>> FadeBackgroundAlpha;
	FSchematicGraph* GraphData;
	FUpdateNodeWidget UpdateNodeWidgetDelegate;
	FOnNodeClicked OnNodeClickedDelegate;
	FOnDrop OnDropDelegate;
};

#endif