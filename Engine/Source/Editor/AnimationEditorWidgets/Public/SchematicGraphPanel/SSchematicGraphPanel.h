// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "SchematicGraphModel.h"
#include "SNodePanel.h"
#include "TickableEditorObject.h"
#include "Animation/AnimatedAttribute.h"

class SSchematicGraphPanel;
class SSchematicGraphNode;

class FSchematicGraphNodeDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FSchematicGraphNodeDragDropOp, FDragDropOperation)

	DECLARE_DELEGATE_TwoParams(FOnEndDrag, SSchematicGraphNode*, const FDragDropOperation&);

	static TSharedRef<FSchematicGraphNodeDragDropOp> New(TArray<SSchematicGraphNode*> InSchematicGraphNodes, const TArray<FGuid>& InElements, FOnEndDrag InOnEndDragDelegate);

	virtual ~FSchematicGraphNodeDragDropOp() override;
	
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** @return true if this drag operation contains property paths */
	bool HasElements() const
	{
		return Elements.Num() > 0;
	}

	/** @return The property paths from this drag operation */
	const TArray<FGuid>& GetElements() const
	{
		return Elements;
	}

	FString GetJoinedDecoratorLabels() const;

private:

	/** Nodes being dragged */
	TArray<SSchematicGraphNode*> SchematicGraphNodes;

	/** Data for the property paths this item represents */
	TArray<FGuid> Elements;

	/** Delegate to call when this drag operation ends */
	FOnEndDrag OnEndDragDelegate;
};

class ANIMATIONEDITORWIDGETS_API SSchematicGraphNode : public SNodePanel::SNode
{
public:

	typedef TAnimatedAttribute<FVector2d> FVector2dAttribute;
	typedef TAnimatedAttribute<float> FFloatAttribute;
	typedef TAnimatedAttribute<FLinearColor> FLinearColorAttribute;
	
	DECLARE_DELEGATE_OneParam(FOnClicked, SSchematicGraphNode*);
	DECLARE_DELEGATE_TwoParams(FOnBeginDrag, SSchematicGraphNode*, const FDragDropOperation&);
	DECLARE_DELEGATE_TwoParams(FOnEndDrag, SSchematicGraphNode*, const FDragDropOperation&);
	DECLARE_DELEGATE_TwoParams(FOnDrop, SSchematicGraphNode*, const FDragDropEvent&);

	SLATE_BEGIN_ARGS(SSchematicGraphNode);
	SLATE_ARGUMENT(const FSchematicGraphNode*, NodeData)
	SLATE_ARGUMENT(TSharedPtr<FVector2dAttribute>, Position)
	SLATE_ARGUMENT(TSharedPtr<FVector2dAttribute>, Size)
	SLATE_ARGUMENT(TSharedPtr<FFloatAttribute>, Scale)
	SLATE_ATTRIBUTE(bool, EnableAutoScale)
	SLATE_ARGUMENT(TArray<TSharedPtr<FLinearColorAttribute>>, LayerColors)
	SLATE_ARGUMENT(TFunction<const FSlateBrush*(const FGuid&, int32)>, BrushGetter)
	SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_EVENT(FOnBeginDrag, OnBeginDrag)
	SLATE_EVENT(FOnEndDrag, OnEndDrag)
	SLATE_EVENT(FOnDrop, OnDrop)
	SLATE_ATTRIBUTE(FText, ToolTipText)
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
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual EVisibility GetNodeVisibility() const;
	virtual FVector2d GetPosition() const override;
	void EnablePositionAnimation(bool bEnabled = true);

	const FVector2d& GetOriginalSize() const { return OriginalSize; }

	const FSchematicGraphNode* GetNodeData() const { return NodeData; }
	const FGuid GetGuid() const;
	bool IsInteractive() const;

	const bool IsBeingDragged() const { return bIsBeingDragged; } 
	
private:

	bool bIsBeingDragged = false;
	static inline const FVector2d DefaultNodeSize = FVector2d(32.0,32.0);  
	FVector2d OriginalSize = DefaultNodeSize;
	static inline constexpr float ScaledUp = 1.25;
	static inline constexpr float ScaledDown = 0.75;

	FSchematicGraphNode* NodeData = nullptr;
	TSharedPtr<FVector2dAttribute> Position;
	TOptional<FVector2d> PositionDuringDrag;
	TOptional<FVector2d> OffsetDuringDrag;
	TSharedPtr<FVector2dAttribute> Size;
	TSharedPtr<FFloatAttribute> Scale;
	TAttribute<bool> EnableAutoScale;
	TOptional<float> AutoScale;
	TArray<TSharedPtr<FLinearColorAttribute>> LayerColors;
	TFunction<const FSlateBrush*(const FGuid&, int32)> BrushGetter;
	FOnClicked OnClickedDelegate;
	FOnBeginDrag OnBeginDragDelegate;
	FOnEndDrag OnEndDragDelegate;
	FOnDrop OnDropDelegate;
	SSchematicGraphPanel* SchematicGraphPanel = nullptr;

	friend class SSchematicGraphPanel;
	friend class FSchematicGraphModel;
};


/** Widget allowing editing of a control rig's structure */
class ANIMATIONEDITORWIDGETS_API SSchematicGraphPanel : public SNodePanel, public FTickableEditorObject
{
public:

	using FVector2dAttribute = SSchematicGraphNode::FVector2dAttribute;
	using FFloatAttribute = SSchematicGraphNode::FFloatAttribute;
	using FLinearColorAttribute = SSchematicGraphNode::FLinearColorAttribute;

	DECLARE_DELEGATE_TwoParams(FOnNodeClicked, SSchematicGraphPanel*, SSchematicGraphNode*);
	DECLARE_DELEGATE_ThreeParams(FOnBeginDrag, SSchematicGraphPanel*, SSchematicGraphNode*, const FDragDropOperation&);
	DECLARE_DELEGATE_ThreeParams(FOnEndDrag, SSchematicGraphPanel*, SSchematicGraphNode*, const FDragDropOperation&);
	DECLARE_DELEGATE_ThreeParams(FOnDrop, SSchematicGraphPanel*, SSchematicGraphNode*, const FDragDropEvent&);
	
	SLATE_BEGIN_ARGS(SSchematicGraphPanel) {}
	SLATE_ARGUMENT(bool, IsOverlay)
	SLATE_ARGUMENT(FSchematicGraphModel*, GraphData)
	SLATE_ARGUMENT(int32, PaddingLeft)
	SLATE_ARGUMENT(int32, PaddingRight)
	SLATE_ARGUMENT(int32, PaddingTop)
	SLATE_ARGUMENT(int32, PaddingBottom)
	SLATE_ARGUMENT(int32, PaddingInterNode)
	SLATE_EVENT(FOnNodeClicked, OnNodeClicked)
	SLATE_EVENT(FOnBeginDrag, OnBeginDrag)
	SLATE_EVENT(FOnEndDrag, OnEndDrag)
	SLATE_EVENT(FOnDrop, OnDrop)
	SLATE_END_ARGS()

	struct FSchematicLinkWidgetInfo
	{
		TSharedPtr<FFloatAttribute> Minimum;
		TSharedPtr<FFloatAttribute> Maximum;
		TSharedPtr<FLinearColorAttribute> Color;
		TSharedPtr<FFloatAttribute> Thickness;
	};

	virtual ~SSchematicGraphPanel() override
	{
		OnNodeClickedDelegate.Unbind();
		OnDropDelegate.Unbind();
	}

	void SetSchematicGraph(FSchematicGraphModel* InGraphData);
	void Construct(const FArguments& InArgs);

	void RebuildPanel();
	void AddNode(const FSchematicGraphNode* InNodeToAdd);
	void RemoveNode(const FSchematicGraphNode* InNodeToRemove);
	const SSchematicGraphNode* FindNode(const FGuid& InGuid) const;
	void AddLink(const FSchematicGraphLink* InLinkToAdd);
	void RemoveLink(const FSchematicGraphLink* InLinkToRemove);
	const FSchematicLinkWidgetInfo* FindLink(const FGuid& InGuid) const;
	
	// SNodePanel interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void RemoveAllNodes() override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// End of SNodePanel interface
	
	TSharedRef<SSchematicGraphNode> GetChild(int32 ChildIndex) const;

	// FTickableEditorObject Interface
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool IsTickable() const override { return true; }
	// End of FTickableEditorObject interface

	void OnNodeClicked(SSchematicGraphNode* Node);
	void OnBeginDragEvent(SSchematicGraphNode* Node, const FDragDropOperation& InDragDropEvent);
	void OnEndDragEvent(SSchematicGraphNode* Node, const FDragDropOperation& InDragDropEvent);
	void OnDropEvent(SSchematicGraphNode* Node, const FDragDropEvent& InDragDropEvent);
	FReply HandleNodeDragDetected(FGuid Guid, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	virtual FVector2d GetPositionForNode(FGuid InNodeGuid) const;
	virtual float GetScaleForNode(FGuid InNodeGuid, bool bIncludeScaleOffset) const;
	virtual bool IsAutoScaleEnabledForNode(FGuid InNodeGuid) const;
	virtual float GetMinimumLinkDistanceForNode(FGuid InLinkGuid, bool bIncludeScale = true) const;

	void UpdateAutoScalingForNodes();

	bool bIsDragDropping = false;
	bool bIsOverlay;
	int32 PaddingLeft = 0;
	int32 PaddingRight = 0;
	int32 PaddingTop = 0;
	int32 PaddingBottom = 0;
	int32 PaddingInterNode = 0;
	TSharedPtr<FFloatAttribute> FadeBackgroundAlpha;
	FSchematicGraphModel* GraphData;
	FOnNodeClicked OnNodeClickedDelegate;
	FOnBeginDrag OnBeginDragDelegate;
	FOnEndDrag OnEndDragDelegate;
	FOnDrop OnDropDelegate;
	TArray<FGuid> NodesTopLeft;
	TArray<FGuid> NodesTopRight;
	TArray<FGuid> NodesBottomLeft;
	TArray<FGuid> NodesBottomRight;
	TMap<FGuid, TSharedPtr<SSchematicGraphNode>> NodeByGuid;

	TMap<FGuid, TSharedPtr<FSchematicLinkWidgetInfo>> LinkByGuid;

	mutable TMap<FGuid, FVector2d> NodeCenterByGuid;
	mutable TArray<bool> NodeVisibilityByIndex;
	mutable TMap<FGuid, bool> NodeVisibilityByGuid;
};

#endif