// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "SNodePanel.h"
#include "TickableEditorObject.h"
#include "Animation/AnimatedAttribute.h"

class SSchematicGraphPanel;

enum ESchematicGraphNodePlacementConstraint
{
	Free,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

enum ESchematicGraphVisibility
{
	Visible,
	FadedOut,
	Hidden
};

#define SCHEMATICGRAPHELEMENT_BODY(ClassName, SuperClass, BaseClass) \
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
	return Cast<T>((const BaseClass*) InNode); \
} \
template<typename T> \
friend T* Cast(ClassName* InNode) \
{ \
	return Cast<T>((BaseClass*) InNode); \
} \
template<typename T> \
friend const T* CastChecked(const ClassName* InNode) \
{ \
	return CastChecked<T>((const BaseClass*) InNode); \
} \
template<typename T> \
friend T* CastChecked(ClassName* InNode) \
{ \
	return CastChecked<T>((BaseClass*) InNode); \
}

#define SCHEMATICGRAPHNODE_BODY(ClassName, SuperClass) \
SCHEMATICGRAPHELEMENT_BODY(ClassName, SuperClass, FSchematicGraphNode)

#define SCHEMATICGRAPHLINK_BODY(ClassName, SuperClass) \
SCHEMATICGRAPHELEMENT_BODY(ClassName, SuperClass, FSchematicGraphNode)

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
	virtual void SetPositionOffset(const FVector2d& InPositionOffset) { PositionOffset = InPositionOffset; }
	virtual float GetScaleOffset() const { return ScaleOffset; }
	virtual void SetScaleOffset(float InScaleOffset) { ScaleOffset = InScaleOffset; }
	virtual FLinearColor GetColor() const { return Color; }
	virtual const FSlateBrush* GetBrush() const { return Brush; }
	virtual bool IsAutoScaleEnabled() const { return false; }
	virtual const FText& GetToolTip() const { return ToolTip; }
	virtual ESchematicGraphNodePlacementConstraint GetPlacement() const { return Placement; }
	virtual void SetPlacement(ESchematicGraphNodePlacementConstraint InPlacement) { Placement = InPlacement; }
	virtual ESchematicGraphVisibility GetVisibility() const { return Visibility; }
	virtual void SetVisibility(ESchematicGraphVisibility InVisibility) { Visibility = InVisibility; }
	virtual bool IsDragSupported() const { return bDragSupported; }
	virtual void SetDragSupported(bool InDragSupported) { bDragSupported = InDragSupported;}

	virtual FString GetDragDropDecoratorLabel() const;

protected:
	
	FGuid Guid = FGuid::NewGuid();
	bool bIsSelected = false;
	FVector2d Position = FVector2d::ZeroVector;
	FVector2d PositionOffset = FVector2d::ZeroVector;
	float ScaleOffset = 1.f;
	FLinearColor Color = FLinearColor::White;
	const FSlateBrush* Brush = nullptr;
	FText ToolTip = FText();
	ESchematicGraphNodePlacementConstraint Placement = ESchematicGraphNodePlacementConstraint::Free;
	ESchematicGraphVisibility Visibility = ESchematicGraphVisibility::Visible;
	bool bDragSupported = false;

	friend class FSchematicGraphModel;
};

class ANIMATIONEDITORWIDGETS_API FSchematicGraphLink
{
public:
	virtual ~FSchematicGraphLink() {}
	
	inline static const FName& Type = TEXT("FSchematicGraphLink");
	virtual const FName& GetType() const;
	virtual bool IsA(const FName& InType) const;
	template<typename NodeType>
	bool IsA() const
	{
		return IsA(NodeType::Type);
	}
	
	template<typename T>
	friend const T* Cast(const FSchematicGraphLink* InNode)
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
	friend T* Cast(FSchematicGraphLink* InNode)
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
	friend const T* CastChecked(const FSchematicGraphLink* InNode)
	{
		const T* Node = Cast<T>(InNode);
		check(Node);
		return Node;
	}

	template<typename T>
	friend T* CastChecked(FSchematicGraphLink* InNode)
	{
		T* Node = Cast<T>(InNode);
		check(Node);
		return Node;
	}
	
	const FGuid& GetGuid() const { return Guid; }
	static uint32 GetLinkHash(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid)
	{
		return HashCombine(GetTypeHash(InSourceNodeGuid), GetTypeHash(InTargetNodeGuid));
	}
	uint32 GetLinkHash() const { return GetLinkHash(SourceNodeGuid, TargetNodeGuid); }
	const FGuid& GetSourceNodeGuid() const { return SourceNodeGuid; }
	const FGuid& GetTargetNodeGuid() const { return TargetNodeGuid; }
	virtual float GetMinimum() const { return Minimum; }
	virtual float GetMaximum() const { return Maximum; }
	virtual FVector2d GetSourceNodeOffset() const { return SourceNodeOffset; }
	virtual FVector2d GetTargetNodeOffset() const { return TargetNodeOffset; }
	virtual FLinearColor GetColor() const { return Color; }
	virtual float GetThickness() const { return Thickness; }
	virtual const FSlateBrush* GetBrush() const { return Brush; }
	virtual const FText& GetToolTip() const { return ToolTip; }
	virtual ESchematicGraphVisibility GetVisibility() const { return Visibility; }
	virtual void SetVisibility(ESchematicGraphVisibility InVisibility) { Visibility = InVisibility; }

protected:
	
	FGuid Guid = FGuid::NewGuid();
	FGuid SourceNodeGuid = FGuid();
	FGuid TargetNodeGuid = FGuid();
	float Minimum = 0.f;
	float Maximum = 1.f;
	FVector2d SourceNodeOffset = FVector2d::ZeroVector;
	FVector2d TargetNodeOffset = FVector2d::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 1.f;
	const FSlateBrush* Brush = nullptr;
	FText ToolTip = FText();
	ESchematicGraphVisibility Visibility = ESchematicGraphVisibility::Visible;

	friend class FSchematicGraphModel;
};

DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeAdded, const FSchematicGraphNode*);
DECLARE_EVENT_OneParam(FSchematicGraph, FOnNodeRemoved, const FSchematicGraphNode*);
DECLARE_EVENT_OneParam(FSchematicGraph, FOnLinkAdded, const FSchematicGraphLink*);
DECLARE_EVENT_OneParam(FSchematicGraph, FOnLinkRemoved, const FSchematicGraphLink*);
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
	const NodeType* FindNode(const FGuid& InNodeGuid) const
	{
		if(const TSharedPtr<FSchematicGraphNode>* ExistingNode = NodeByGuid.Find(InNodeGuid))
		{
			return Cast<NodeType>(ExistingNode->Get());
		};
		return nullptr;
	}

	template<typename NodeType = FSchematicGraphNode>
	const NodeType* FindNodeChecked(const FGuid& InNodeGuid) const
	{
		if(const TSharedPtr<FSchematicGraphNode>* ExistingNode = NodeByGuid.Find(InNodeGuid))
		{
			check(ExistingNode->Get()->IsA(NodeType::Type));
			return static_cast<NodeType*>(ExistingNode->Get());
		};
		return nullptr;
	}

	virtual bool RemoveNode(const FGuid& InNodeGuid);

	const TArray<TSharedPtr<FSchematicGraphNode>>& GetNodes() const { return Nodes; }

	FVector2d GetPositionForNode(const FGuid& InNodeGuid) const;
	virtual FVector2d GetPositionForNode(const FSchematicGraphNode* InNode) const;
	FVector2d GetPositionOffsetForNode(const FGuid& InNodeGuid) const;
	virtual FVector2d GetPositionOffsetForNode(const FSchematicGraphNode* InNode) const;
	bool GetPositionAnimationEnabledForNode(const FGuid& InNodeGuid) const;
	virtual bool GetPositionAnimationEnabledForNode(const FSchematicGraphNode* InNode) const;
	FVector2d GetSizeForNode(const FGuid& InNodeGuid) const;
	virtual FVector2d GetSizeForNode(const FSchematicGraphNode* InNode) const;
	float GetScaleForNode(const FGuid& InNodeGuid, bool bIncludeScaleOffset) const;
	virtual float GetScaleForNode(const FSchematicGraphNode* InNode, bool bIncludeScaleOffset) const;
	float GetScaleOffsetForNode(const FGuid& InNodeGuid) const;
	virtual float GetScaleOffsetForNode(const FSchematicGraphNode* InNode) const;
	float GetMinimumLinkDistanceForNode(const FGuid& InNodeGuid) const;
	virtual float GetMinimumLinkDistanceForNode(const FSchematicGraphNode* InNode) const;
	bool IsAutoScaleEnabledForNode(const FGuid& InNodeGuid) const;
	virtual bool IsAutoScaleEnabledForNode(const FSchematicGraphNode* InNode) const;
	FLinearColor GetColorForNode(const FGuid& InNodeGuid) const;
	virtual FLinearColor GetColorForNode(const FSchematicGraphNode* InNode) const;
	const FSlateBrush* GetBrushForNode(const FGuid& InNodeGuid) const;
	virtual const FSlateBrush* GetBrushForNode(const FSchematicGraphNode* InNode) const;
	const FText GetToolTipForNode(const FGuid& InNodeGuid) const;
	virtual const FText GetToolTipForNode(const FSchematicGraphNode* InNode) const;
	ESchematicGraphNodePlacementConstraint GetPlacementForNode(const FGuid& InNodeGuid) const;
	virtual ESchematicGraphNodePlacementConstraint GetPlacementForNode(const FSchematicGraphNode* InNode) const;
	ESchematicGraphVisibility GetVisibilityForNode(const FGuid& InNodeGuid) const;
	virtual ESchematicGraphVisibility GetVisibilityForNode(const FSchematicGraphNode* InNode) const;
	bool IsDragSupportedForNode(const FGuid& InNodeGuid) const;
	virtual bool IsDragSupportedForNode(const FSchematicGraphNode* InNode) const;

	template<typename LinkType = FSchematicGraphLink>
	LinkType* AddLink(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid, bool bNotify = true)
	{
		const TSharedPtr<FSchematicGraphLink> NewLink = MakeShareable(new LinkType);
		NewLink->SourceNodeGuid = InSourceNodeGuid;
		NewLink->TargetNodeGuid = InTargetNodeGuid;
		Links.Add(NewLink);
		LinkByGuid.Add(NewLink->GetGuid(), NewLink);
		LinkByHash.Add(NewLink->GetLinkHash(), NewLink);
		NodeGuidToLinkGuids.FindOrAdd(InSourceNodeGuid).Get<0>().Add(NewLink->GetGuid());
		NodeGuidToLinkGuids.FindOrAdd(InTargetNodeGuid).Get<1>().Add(NewLink->GetGuid());

		if (bNotify && OnLinkAddedDelegate.IsBound())
		{
			OnLinkAddedDelegate.Broadcast(NewLink.Get());
		}
		return static_cast<LinkType*>(NewLink.Get());
	}

	template<typename LinkType = FSchematicGraphLink>
	const LinkType* FindLink(const FGuid& InLinkGuid) const
	{
		if(const TSharedPtr<FSchematicGraphLink>* ExistingLink = LinkByGuid.Find(InLinkGuid))
		{
			return Cast<LinkType>(ExistingLink->Get());
		};
		return nullptr;
	}

	template<typename LinkType = FSchematicGraphLink>
	const LinkType* FindLinkChecked(const FGuid& InLinkGuid) const
	{
		if(const TSharedPtr<FSchematicGraphLink>* ExistingLink = LinkByGuid.Find(InLinkGuid))
		{
			check(ExistingLink->Get()->IsA(LinkType::Type));
			return static_cast<LinkType*>(ExistingLink->Get());
		};
		return nullptr;
	}

	template<typename LinkType = FSchematicGraphLink>
	const LinkType* FindLink(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid) const
	{
		const uint32 LinkHash = FSchematicGraphLink::GetLinkHash(InSourceNodeGuid, InTargetNodeGuid);
		if(const TSharedPtr<FSchematicGraphLink>* ExistingLink = LinkByHash.Find(LinkHash))
		{
			return Cast<LinkType>(ExistingLink->Get());
		};
		return nullptr;
	}

	template<typename LinkType = FSchematicGraphLink>
	const LinkType* FindLinkChecked(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid) const
	{
		const uint32 LinkHash = FSchematicGraphLink::GetLinkHash(InSourceNodeGuid, InTargetNodeGuid);
		if(const TSharedPtr<FSchematicGraphLink>* ExistingLink = LinkByHash.Find(LinkHash))
		{
			check(ExistingLink->Get()->IsA(LinkType::Type));
			return Cast<LinkType>(ExistingLink->Get());
		};
		return nullptr;
	}

	bool IsLinkedTo(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid) const;
	TArray<const FSchematicGraphLink*> FindLinksOnNode(const FGuid& InNodeGuid) const;
	TArray<const FSchematicGraphLink*> FindLinksOnSource(const FGuid& InSourceNodeGuid) const;
	TArray<const FSchematicGraphLink*> FindLinksOnTarget(const FGuid& InTargetNodeGuid) const;

	virtual bool RemoveLink(const FGuid& InLinkGuid);

	const TArray<TSharedPtr<FSchematicGraphLink>>& GetLinks() const { return Links; }

	float GetMinimumForLink(const FGuid& InLinkGuid) const;
	virtual float GetMinimumForLink(const FSchematicGraphLink* InLink) const;
	float GetMaximumForLink(const FGuid& InLinkGuid) const;
	virtual float GetMaximumForLink(const FSchematicGraphLink* InLink) const;
	FVector2d GetSourceNodeOffsetForLink(const FGuid& InLinkGuid) const;
	FVector2d GetSourceNodeOffsetForLink(const FSchematicGraphLink* InLink) const;
	FVector2d GetTargetNodeOffsetForLink(const FGuid& InLinkGuid) const;
	FVector2d GetTargetNodeOffsetForLink(const FSchematicGraphLink* InLink) const;
	FLinearColor GetColorForLink(const FGuid& InLinkGuid) const;
	virtual FLinearColor GetColorForLink(const FSchematicGraphLink* InLink) const;
	float GetThicknessForLink(const FGuid& InLinkGuid) const;
	virtual float GetThicknessForLink(const FSchematicGraphLink* InLink) const;
	const FSlateBrush* GetBrushForLink(const FGuid& InLinkGuid) const;
	virtual const FSlateBrush* GetBrushForLink(const FSchematicGraphLink* InLink) const;
	const FText GetToolTipForLink(const FGuid& InLinkGuid) const;
	virtual const FText GetToolTipForLink(const FSchematicGraphLink* InLink) const;
	ESchematicGraphVisibility GetVisibilityForLink(const FGuid& InLinkGuid) const;
	virtual ESchematicGraphVisibility GetVisibilityForLink(const FSchematicGraphLink* InLink) const;

	FOnNodeAdded& OnNodeAdded() { return OnNodeAddedDelegate; }
	FOnNodeRemoved& OnNodeRemoved() { return OnNodeRemovedDelegate; }
	FOnLinkAdded& OnLinkAdded() { return OnLinkAddedDelegate; }
	FOnLinkRemoved& OnLinkRemoved() { return OnLinkRemovedDelegate; }
	FOnGraphReset& OnGraphReset() { return OnGraphResetDelegate; }

	virtual bool GetForwardedNodeForDrag(FGuid& InOutGuid) const { return false; }

protected:

	TArray<TSharedPtr<FSchematicGraphNode>> Nodes;
	TMap<FGuid, TSharedPtr<FSchematicGraphNode>> NodeByGuid;
	TArray<TSharedPtr<FSchematicGraphLink>> Links;
	TMap<FGuid, TSharedPtr<FSchematicGraphLink>> LinkByGuid;
	TMap<uint32, TSharedPtr<FSchematicGraphLink>> LinkByHash;
	TMap<FGuid, TTuple<TArray<FGuid>, TArray<FGuid>>> NodeGuidToLinkGuids;

	FOnNodeAdded OnNodeAddedDelegate;
	FOnNodeRemoved OnNodeRemovedDelegate;
	FOnLinkAdded OnLinkAddedDelegate;
	FOnLinkRemoved OnLinkRemovedDelegate;
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
	SLATE_ATTRIBUTE(bool, EnableAutoScale)
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
	TSharedPtr<FLinearColorAttribute> Color;
	TAttribute<const FSlateBrush*> Brush;
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
	virtual FVector2d GetSizeForNode(FGuid InNodeGuid) const;
	virtual float GetScaleForNode(FGuid InNodeGuid, bool bIncludeScaleOffset) const;
	virtual bool IsAutoScaleEnabledForNode(FGuid InNodeGuid) const;
	virtual float GetMinimumLinkDistanceForNode(FGuid InLinkGuid, bool bIncludeScale = true) const;
	virtual FLinearColor GetColorForNode(FGuid InNodeGuid) const;
	virtual const FSlateBrush* GetBrushForNode(FGuid InNodeGuid) const;
	virtual FText GetToolTipForNode(FGuid InNodeGuid) const;
	virtual ESchematicGraphVisibility GetVisibilityForNode(FGuid InNodeGuid) const;
	virtual bool IsDragSupportedForNode(FGuid InNodeGuid) const;

	virtual float GetMinimumForLink(FGuid InLinkGuid) const;
	virtual float GetMaximumForLink(FGuid InLinkGuid) const;
	virtual FLinearColor GetColorForLink(FGuid InLinkGuid) const;
	virtual float GetThicknessForLink(FGuid InLinkGuid) const;
	virtual const FSlateBrush* GetBrushForLink(FGuid InLinkGuid) const;
	virtual FText GetToolTipForLink(FGuid InLinkGuid) const;
	virtual ESchematicGraphVisibility GetVisibilityForLink(FGuid InLinkGuid) const;

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