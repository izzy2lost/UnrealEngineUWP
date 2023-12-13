// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "SNodePanel.h"
#include "TickableEditorObject.h"
#include "Animation/AnimatedAttribute.h"

enum ESchematicGraphNodePlacementConstraint
{
	Free,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

enum ESchematicGraphNodeVisibility
{
	Visible,
	FadedOut,
	Hidden
};

#define SCHEMATICGRAPHNODE_BODY(ClassName, SuperClass) \
inline static const FName& Type = TEXT(#ClassName); \
virtual const FName& GetType() const override { return ClassName::Type; } \
virtual bool IsA(const FName& InType) const \
{ \
	if(ClassName::Type == InType) \
	{ \
		return true; \
	} \
	return SuperClass::IsA(InType); \
} \
template<typename T> \
friend const T* Cast(const ClassName* InNode) \
{ \
	return Cast<T>((const FSchematicGraphNode*) InNode); \
} \
template<typename T> \
friend T* Cast(ClassName* InNode) \
{ \
	return Cast<T>((FSchematicGraphNode*) InNode); \
} \
template<typename T> \
friend const T* CastChecked(const ClassName* InNode) \
{ \
	return CastChecked<T>((const FSchematicGraphNode*) InNode); \
} \
template<typename T> \
friend T* CastChecked(ClassName* InNode) \
{ \
	return CastChecked<T>((FSchematicGraphNode*) InNode); \
} \

class ANIMATIONEDITORWIDGETS_API FSchematicGraphNode
{
public:
	virtual ~FSchematicGraphNode() {}
	
	inline static const FName& Type = TEXT("FSchematicGraphNode");
	virtual const FName& GetType() const;
	virtual bool IsA(const FName& InType) const;
	template<typename NodeType>
	bool IsA() const
	{
		return IsA(NodeType::Type);
	}
	
	template<typename T>
	friend const T* Cast(const FSchematicGraphNode* InNode)
	{
		if(InNode)
		{
			if(InNode->IsA<T>())
			{
				return static_cast<const T*>(InNode);
			}
		}
		return nullptr;
	}

	template<typename T>
	friend T* Cast(FSchematicGraphNode* InNode)
	{
		if(InNode)
		{
			if(InNode->IsA<T>())
			{
				return static_cast<T*>(InNode);
			}
		}
		return nullptr;
	}

	template<typename T>
	friend const T* CastChecked(const FSchematicGraphNode* InNode)
	{
		const T* Node = Cast<T>(InNode);
		check(Node);
		return Node;
	}

	template<typename T>
	friend T* CastChecked(FSchematicGraphNode* InNode)
	{
		T* Node = Cast<T>(InNode);
		check(Node);
		return Node;
	}
	
	const FGuid& GetGuid() const { return Guid; }
	bool IsSelected() const { return bIsSelected; }
	void SetSelected(bool bSelected = true) { bIsSelected = bSelected; }
	virtual FVector2d GetPosition() const { return Position; }
	virtual FVector2d GetPositionOffset() const { return PositionOffset; }
	virtual FLinearColor GetColor() const { return Color; }
	virtual const FSlateBrush* GetBrush() const { return Brush; }
	virtual const FText& GetToolTip() const { return ToolTip; }
	virtual ESchematicGraphNodePlacementConstraint GetPlacement() const { return Placement; }
	virtual void SetPlacement(ESchematicGraphNodePlacementConstraint InPlacement) { Placement = InPlacement; }
	virtual ESchematicGraphNodeVisibility GetVisibility() const { return Visibility; }
	virtual void SetVisibility(ESchematicGraphNodeVisibility InVisibility) { Visibility = InVisibility; }

	virtual FString GetDragDropDecoratorLabel() const;

protected:
	
	FGuid Guid = FGuid::NewGuid();
	bool bIsSelected = false;
	FVector2d Position = FVector2d::ZeroVector;
	FVector2d PositionOffset = FVector2d::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	const FSlateBrush* Brush = nullptr;
	FText ToolTip = FText();
	ESchematicGraphNodePlacementConstraint Placement = ESchematicGraphNodePlacementConstraint::Free;
	ESchematicGraphNodeVisibility Visibility = ESchematicGraphNodeVisibility::Visible;

	friend class FSchematicGraphModel;
};

DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeAdded, const FSchematicGraphNode*);
DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeRemoved, const FSchematicGraphNode*);
DECLARE_EVENT(FSchematicGraph, FOnGraphReset);

class ANIMATIONEDITORWIDGETS_API FSchematicGraphModel
{
public:

	virtual ~FSchematicGraphModel() {}
	
	virtual void Reset();
	
	template<typename NodeType = FSchematicGraphNode>
	NodeType* AddNode(bool bNotify = true)
	{
		const TSharedPtr<FSchematicGraphNode> NewNode = MakeShareable(new NodeType);
		Nodes.Add(NewNode);
		NodeByGuid.Add(NewNode->GetGuid(), NewNode);

		if (bNotify && OnNodeAddedDelegate.IsBound())
		{
			OnNodeAddedDelegate.Broadcast(NewNode.Get());
		}
		return static_cast<NodeType*>(NewNode.Get());
	}

	template<typename NodeType = FSchematicGraphNode>
	const NodeType* FindNode(const FGuid& InGuid) const
	{
		if(const TSharedPtr<FSchematicGraphNode>* ExistingNode = NodeByGuid.Find(InGuid))
		{
			return Cast<NodeType>(ExistingNode->Get());
		};
		return nullptr;
	}

	template<typename NodeType = FSchematicGraphNode>
	const NodeType* FindNodeChecked(const FGuid& InGuid) const
	{
		if(const TSharedPtr<FSchematicGraphNode>* ExistingNode = NodeByGuid.Find(InGuid))
		{
			check(ExistingNode->Get()->IsA(NodeType::Type));
			return static_cast<NodeType*>(ExistingNode->Get());
		};
		return nullptr;
	}

	virtual bool RemoveNode(const FGuid& InGuid);

	const TArray<TSharedPtr<FSchematicGraphNode>>& GetNodes() const { return Nodes; }

	FVector2d GetPositionForNode(const FGuid& InGuid) const;
	virtual FVector2d GetPositionForNode(const FSchematicGraphNode* InNode) const;
	FVector2d GetPositionOffsetForNode(const FGuid& InGuid) const;
	virtual FVector2d GetPositionOffsetForNode(const FSchematicGraphNode* InNode) const;
	bool GetPositionAnimationEnabledForNode(const FGuid& InNodeGuid) const;
	virtual bool GetPositionAnimationEnabledForNode(const FSchematicGraphNode* InNode) const;
	FVector2d GetSizeForNode(const FGuid& InNodeGuid) const;
	virtual FVector2d GetSizeForNode(const FSchematicGraphNode* InNode) const;
	float GetScaleForNode(const FGuid& InNodeGuid) const;
	virtual float GetScaleForNode(const FSchematicGraphNode* InNode) const;
	FLinearColor GetColorForNode(const FGuid& InGuid) const;
	virtual FLinearColor GetColorForNode(const FSchematicGraphNode* InNode) const;
	const FSlateBrush* GetBrushForNode(const FGuid& InGuid) const;
	virtual const FSlateBrush* GetBrushForNode(const FSchematicGraphNode* InNode) const;
	const FText GetToolTipForNode(const FGuid& InGuid) const;
	virtual const FText GetToolTipForNode(const FSchematicGraphNode* InNode) const;
	ESchematicGraphNodePlacementConstraint GetPlacementForNode(const FGuid& InGuid) const;
	virtual ESchematicGraphNodePlacementConstraint GetPlacementForNode(const FSchematicGraphNode* InNode) const;
	ESchematicGraphNodeVisibility GetVisibilityForNode(const FGuid& InGuid) const;
	virtual ESchematicGraphNodeVisibility GetVisibilityForNode(const FSchematicGraphNode* InNode) const;

	FOnNodeAdded& OnNodeAdded() { return OnNodeAddedDelegate; }
	FOnNodeRemoved& OnNodeRemoved() { return OnNodeRemovedDelegate; }
	FOnGraphReset& OnGraphReset() { return OnGraphResetDelegate; }

protected:

	TArray<TSharedPtr<FSchematicGraphNode>> Nodes;
	TMap<FGuid, TSharedPtr<FSchematicGraphNode>> NodeByGuid;
	TMap<FGuid, TArray<FGuid>> Links;

	FOnNodeAdded OnNodeAddedDelegate;
	FOnNodeRemoved OnNodeRemovedDelegate;
	FOnGraphReset OnGraphResetDelegate;
};

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
	SLATE_ARGUMENT(TSharedPtr<FLinearColorAttribute>, Color)
	SLATE_ATTRIBUTE(const FSlateBrush*, Brush)
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

	virtual FVector2d GetPosition() const override;
	void EnablePositionAnimation(bool bEnabled = true);

	const FVector2d& GetOriginalSize() const { return OriginalSize; }

	const FSchematicGraphNode* GetNodeData() const { return NodeData; }
	const FGuid GetGuid() const;
	bool IsInteractive() const;

	const bool IsBeingDragged() const { return bIsBeingDragged; } 
	
private:

	bool bIsBeingDragged = false;
	FVector2d OriginalSize = FVector2d(50.0,50.0);
	float ScaledUp = 1.25;
	float ScaledDown = 0.75;

	const FSchematicGraphNode* NodeData = nullptr;
	TSharedPtr<FVector2dAttribute> Position;
	TOptional<FVector2d> PositionDuringDrag;
	TOptional<FVector2d> OffsetDuringDrag;
	TSharedPtr<FVector2dAttribute> Size;
	TSharedPtr<FFloatAttribute> Scale;
	TSharedPtr<FLinearColorAttribute> Color;
	TAttribute<const FSlateBrush*> Brush;
	FOnClicked OnClickedDelegate;
	FOnBeginDrag OnBeginDragDelegate;
	FOnEndDrag OnEndDragDelegate;
	FOnDrop OnDropDelegate;

	friend class SSchematicGraphPanel;
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
	
	// SNodePanel interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void RemoveAllNodes() override;
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

	virtual FVector2d GetPositionForNode(FGuid InNodeGuid) const;
	virtual FVector2d GetSizeForNode(FGuid InNodeGuid) const;
	virtual float GetScaleForNode(FGuid InNodeGuid) const;
	virtual FLinearColor GetColorForNode(FGuid InNodeGuid) const;
	virtual const FSlateBrush* GetBrushForNode(FGuid InNodeGuid) const;
	virtual FText GetToolTipForNode(FGuid InNodeGuid) const;

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
};

#endif