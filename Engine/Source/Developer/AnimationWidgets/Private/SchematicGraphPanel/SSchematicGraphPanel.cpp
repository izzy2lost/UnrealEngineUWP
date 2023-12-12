// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "DragAndDrop/AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "SSchematicGraphPanel"

const FName& FSchematicGraphNode::GetType() const
{
	return FSchematicGraphNode::Type;
}

bool FSchematicGraphNode::IsA(const FName& InType) const
{
	return Type == InType;
}

FString FSchematicGraphNode::GetDragDropDecoratorLabel() const
{
	return GetGuid().ToString();
}

void FSchematicGraphModel::Reset()
{
	Nodes.Reset();
	Links.Reset();

	if (OnGraphResetDelegate.IsBound())
	{
		OnGraphReset().Broadcast();
	}
}

bool FSchematicGraphModel::RemoveNode(const FGuid& InGuid)
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		if (OnNodeRemovedDelegate.IsBound())
		{
			OnNodeRemovedDelegate.Broadcast(Node);
		}
		const FGuid Guid = Node->GetGuid();
		NodeByGuid.Remove(Node->GetGuid());
		Nodes.RemoveAll([Guid](const TSharedPtr<FSchematicGraphNode>& ExistingNode) -> bool
		{
			return ExistingNode->GetGuid() == Guid;
		});
		return true;
	}
	return false;
}

FVector2d FSchematicGraphModel::GetPositionForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		return GetPositionForNode(Node);
	}
	return FVector2d::ZeroVector;
}

FVector2d FSchematicGraphModel::GetPositionForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetPosition();
}

FVector2d FSchematicGraphModel::GetPositionOffsetForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		return GetPositionOffsetForNode(Node);
	}
	return FVector2d::ZeroVector;
}

FVector2d FSchematicGraphModel::GetPositionOffsetForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetPositionOffset();
}

bool FSchematicGraphModel::GetPositionAnimationEnabledForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetPositionAnimationEnabledForNode(Node);
	}
	return false;
}

bool FSchematicGraphModel::GetPositionAnimationEnabledForNode(const FSchematicGraphNode* InNode) const
{
	return true;
}

FVector2d FSchematicGraphModel::GetSizeForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetSizeForNode(Node);
	}
	return FVector2d::ZeroVector;
}

FVector2d FSchematicGraphModel::GetSizeForNode(const FSchematicGraphNode* InNode) const
{
	static const FVector2d DefaultSize = FVector2d(50.0,50.0);
	return DefaultSize;
}

float FSchematicGraphModel::GetScaleForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetScaleForNode(Node);
	}
	return 1.f;
}

float FSchematicGraphModel::GetScaleForNode(const FSchematicGraphNode* InNode) const
{
	return 1.f;
}

FLinearColor FSchematicGraphModel::GetColorForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		const FLinearColor Color = GetColorForNode(Node);
		if(GetVisibilityForNode(Node) == ESchematicGraphNodeVisibility::FadedOut)
		{
			return Color * 0.5f;
		}
		return Color;
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetColorForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetColor();
}

const FSlateBrush* FSchematicGraphModel::GetBrushForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		if(const FSlateBrush* Brush = GetBrushForNode(Node))
		{
			return Brush;
		}
	}

	static const FSlateBrush* DefaultBrush = SSchematicGraphNode::FArguments()._Brush.Get();
	return DefaultBrush;
}

const FSlateBrush* FSchematicGraphModel::GetBrushForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetBrush();
}

const FText FSchematicGraphModel::GetToolTipForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		return GetToolTipForNode(Node);
	}
	return FText();
}

const FText FSchematicGraphModel::GetToolTipForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetToolTip();
}

ESchematicGraphNodePlacementConstraint FSchematicGraphModel::GetPlacementForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		return GetPlacementForNode(Node);
	}
	return ESchematicGraphNodePlacementConstraint::Free;
}

ESchematicGraphNodePlacementConstraint FSchematicGraphModel::GetPlacementForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetPlacement();
}

ESchematicGraphNodeVisibility FSchematicGraphModel::GetVisibilityForNode(const FGuid& InGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InGuid))
	{
		return GetVisibilityForNode(Node);
	}
	return ESchematicGraphNodeVisibility::Visible;
}

ESchematicGraphNodeVisibility FSchematicGraphModel::GetVisibilityForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetVisibility();
}

TSharedRef<FSchematicGraphNodeDragDropOp> FSchematicGraphNodeDragDropOp::New(TArray<SSchematicGraphNode*> InSchematicGraphNodes, const TArray<FGuid>& InElements, FSchematicGraphNodeDragDropOp::FOnEndDrag InOnEndDragDelegate)
{
	TSharedRef<FSchematicGraphNodeDragDropOp> Operation = MakeShared<FSchematicGraphNodeDragDropOp>();
	Operation->SchematicGraphNodes = InSchematicGraphNodes;
	Operation->Elements = InElements;
	Operation->OnEndDragDelegate = InOnEndDragDelegate;
	Operation->Construct();
	return Operation;
}

FSchematicGraphNodeDragDropOp::~FSchematicGraphNodeDragDropOp()
{
	if (OnEndDragDelegate.IsBound())
	{
		for (SSchematicGraphNode* Node : SchematicGraphNodes)
		{
			OnEndDragDelegate.Execute(Node, *this);
		}
	}
}

TSharedPtr<SWidget> FSchematicGraphNodeDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedDecoratorLabels()))
		];
}

FString FSchematicGraphNodeDragDropOp::GetJoinedDecoratorLabels() const
{
	TArray<FString> DecoratorLabels;
	for(const SSchematicGraphNode* GraphNode : SchematicGraphNodes)
	{
		if(const FSchematicGraphNode* Node = GraphNode->GetNodeData())
		{
			DecoratorLabels.Add(Node->GetDragDropDecoratorLabel());
		}
	}
	return FString::Join(DecoratorLabels, TEXT(","));
}

SSchematicGraphNode::FArguments::FArguments()
: _NodeData(nullptr)
, _Brush(nullptr)
{
	static const FSchematicGraphNode EmptyNodeData;
	_NodeData = &EmptyNodeData;
	static const FSlateBrush* WhiteTexture = FAppStyle::GetBrush("WhiteTexture");
	_Brush = WhiteTexture;
}

void SSchematicGraphNode::Construct(const FArguments& InArgs)
{
	if(InArgs._NodeData)
	{
		NodeData = InArgs._NodeData;
	}
	OnClickedDelegate = InArgs._OnClicked;
	OnBeginDragDelegate = InArgs._OnBeginDrag;
	OnEndDragDelegate = InArgs._OnEndDrag;
	OnDropDelegate = InArgs._OnDrop;

	Position = InArgs._Position;
	Size = InArgs._Size;
	Scale = InArgs._Scale;
	Color = InArgs._Color;
	if(InArgs._Brush.IsSet() || InArgs._Brush.IsBound())
	{
		Brush = InArgs._Brush;
	}
	OriginalSize = Size->Get();

	if(InArgs._ToolTipText.IsBound() || InArgs._ToolTipText.IsSet())
	{
		SetToolTipText(InArgs._ToolTipText);
	}
}

FVector2D SSchematicGraphNode::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return OriginalSize*LayoutScaleMultiplier;
}

int32 SSchematicGraphNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NewLayerId = SNodePanel::SNode::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	NewLayerId++;

	const FVector2d CurSize = Size->Get() * Scale->Get();
	const FVector2d SizeOffset = (CurSize-OriginalSize)*-0.5;

	FSlateDrawElement::MakeBox(
				OutDrawElements,
				NewLayerId,
				AllottedGeometry.ToPaintGeometry(CurSize, FSlateLayoutTransform(SizeOffset)),
				Brush.Get(),
				ESlateDrawEffect::None,
				Color.IsValid() ? Color->Get() : FLinearColor::White
				);
	
	return NewLayerId;
}

void SSchematicGraphNode::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(!IsInteractive())
	{
		return;
	}
	SNode::OnMouseEnter(MyGeometry, MouseEvent);
	Scale->Set(ScaledUp);
}

void SSchematicGraphNode::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if(!IsInteractive())
	{
		return;
	}
	SNode::OnMouseLeave(MouseEvent);
	Scale->Set(1.f);
}

FReply SSchematicGraphNode::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(!IsInteractive())
	{
		return FReply::Unhandled();
	}
	SNode::OnDragOver(MyGeometry, DragDropEvent);
	Scale->Set(ScaledDown);
	return FReply::Handled();
}

void SSchematicGraphNode::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if(!IsInteractive())
	{
		return;
	}
	SNode::OnDragLeave(DragDropEvent);
	Scale->Set(1.f);
}

FReply SSchematicGraphNode::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(!IsInteractive())
	{
		return FReply::Unhandled();
	}

	// Avoid dropping onto itself
	const TSharedPtr<FSchematicGraphNodeDragDropOp> SchematicDragDropOp = DragDropEvent.GetOperationAs<FSchematicGraphNodeDragDropOp>();
	if (SchematicDragDropOp)
	{
		if (!SchematicDragDropOp->GetElements().IsEmpty())
		{
			if (SchematicDragDropOp->GetElements().ContainsByPredicate([this](const FGuid& Guid)
			{
				return Guid == NodeData->GetGuid();
			}))
			{
				return FReply::Unhandled();
			}
		}
	}
	
	SNode::OnDrop(MyGeometry, DragDropEvent);
	OnDropDelegate.ExecuteIfBound(this, DragDropEvent);
	OnEndDragDelegate.ExecuteIfBound(this, *DragDropEvent.GetOperation().Get());
	return FReply::Unhandled();
}

FReply SSchematicGraphNode::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TArray<FGuid> DraggedElements = {GetGuid()};
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && DraggedElements.Num() > 0)
	{
		bIsBeingDragged = true;

		const FVector2f AbsoluteMousePosition = MouseEvent.GetScreenSpacePosition();
		const FVector2d LocalMousePosition =  (AbsoluteMousePosition - MyGeometry.GetAbsolutePosition()) / MyGeometry.GetAccumulatedLayoutTransform().GetScale();
		OffsetDuringDrag = -LocalMousePosition;
		
		const TSharedRef<FSchematicGraphNodeDragDropOp> DragDropOp = FSchematicGraphNodeDragDropOp::New({this}, MoveTemp(DraggedElements), OnEndDragDelegate);
		OnBeginDragDelegate.ExecuteIfBound(this, DragDropOp.Get());
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}
	
	return FReply::Unhandled();
}

FReply SSchematicGraphNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SNode::OnMouseButtonDown(MyGeometry, MouseEvent);
	if (MouseEvent.GetPressedButtons().Contains(EKeys::LeftMouseButton))
	{
		OnClickedDelegate.ExecuteIfBound(this);
	}
	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FVector2d SSchematicGraphNode::GetPosition() const
{
	return Position->Get() - (OriginalSize*0.5);
}

void SSchematicGraphNode::EnablePositionAnimation(bool bEnabled)
{
	Position->EnableInterpolation(bEnabled);
}

const FGuid SSchematicGraphNode::GetGuid() const
{
	return GetNodeData()->GetGuid();
}

bool SSchematicGraphNode::IsInteractive() const
{
	if(NodeData)
	{
		return NodeData->GetVisibility() == ESchematicGraphNodeVisibility::Visible;
	}
	return true;
}

void SSchematicGraphPanel::SetSchematicGraph(FSchematicGraphModel* InGraphData)
{
	if (GraphData)
	{
		GraphData->OnNodeAdded().RemoveAll(this);
		GraphData->OnNodeRemoved().RemoveAll(this);
		GraphData->OnGraphReset().RemoveAll(this);
	}
	
	GraphData = InGraphData;
	if (GraphData)
	{
		GraphData->OnNodeAdded().AddSP(this, &SSchematicGraphPanel::AddNode);
		GraphData->OnNodeRemoved().AddSP(this, &SSchematicGraphPanel::RemoveNode);
		GraphData->OnGraphReset().AddSP(this, &SSchematicGraphPanel::RebuildPanel);
	}
}

void SSchematicGraphPanel::Construct(const FArguments& InArgs)
{
	GraphData = InArgs._GraphData;
	bIsOverlay = InArgs._IsOverlay;
	PaddingLeft = InArgs._PaddingLeft;
	PaddingRight = InArgs._PaddingRight;
	PaddingTop = InArgs._PaddingTop;
	PaddingBottom = InArgs._PaddingBottom;
	PaddingInterNode = InArgs._PaddingInterNode;
	OnNodeClickedDelegate = InArgs._OnNodeClicked;
	OnBeginDragDelegate = InArgs._OnBeginDrag;
	OnEndDragDelegate = InArgs._OnEndDrag;
	OnDropDelegate = InArgs._OnDrop;

	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::CubicEaseOut, 0.35f);
	FadeBackgroundAlpha = FFloatAttribute::CreateWithGetter(FloatInterpSettings, FFloatAttribute::FGetter::CreateLambda([this]() -> float
	{
		return bIsDragDropping ? 0.5f : 0.f;
	}));
	
	SNodePanel::Construct();

	SetVisibility(bIsOverlay ? EVisibility::SelfHitTestInvisible : EVisibility::Visible);
	if (GraphData)
	{
		GraphData->OnNodeAdded().AddSP(this, &SSchematicGraphPanel::AddNode);
		GraphData->OnNodeRemoved().AddSP(this, &SSchematicGraphPanel::RemoveNode);
		GraphData->OnGraphReset().AddSP(this, &SSchematicGraphPanel::RebuildPanel);
	}
}

void SSchematicGraphPanel::RebuildPanel()
{
	RemoveAllNodes();

	if (GraphData)
	{
		for (const TSharedPtr<FSchematicGraphNode>& Node : GraphData->GetNodes())
		{
			AddNode(Node.Get());
		}
	}
}

void SSchematicGraphPanel::AddNode(const FSchematicGraphNode* InNodeToAdd)
{
	TEasingAttributeInterpolator<FVector2d>::FSettings Vector2DInterpSettings(EEasingInterpolatorType::CubicEaseOut, 0.2f);
	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::CubicEaseOut, 0.2f);
	TEasingAttributeInterpolator<FLinearColor>::FSettings ColorInterpSettings(EEasingInterpolatorType::CubicEaseOut, 0.2f);

	const FGuid Guid = InNodeToAdd->GetGuid();
	const auto Position = FVector2dAttribute::CreateWithGetter(Vector2DInterpSettings, FVector2dAttribute::FGetter::CreateSP(this, &SSchematicGraphPanel::GetPositionForNode, Guid));
	const auto Size = FVector2dAttribute::CreateWithGetter(Vector2DInterpSettings, FVector2dAttribute::FGetter::CreateSP(this, &SSchematicGraphPanel::GetSizeForNode, Guid));
	const auto Scale = FFloatAttribute::CreateWithGetter(FloatInterpSettings, FFloatAttribute::FGetter::CreateSP(this, &SSchematicGraphPanel::GetScaleForNode, Guid), 0.f);
	const auto Color = FLinearColorAttribute::CreateWithGetter(ColorInterpSettings, FLinearColorAttribute::FGetter::CreateSP(this, &SSchematicGraphPanel::GetColorForNode, Guid));

	const TSharedRef<SSchematicGraphNode> NewNode = SNew(SSchematicGraphNode)
														.Position(Position)
														.Size(Size)
														.Scale(Scale)
														.Color(Color)
														.Brush(this, &SSchematicGraphPanel::GetBrushForNode, InNodeToAdd->GetGuid())
														.ToolTipText(this, &SSchematicGraphPanel::GetToolTipForNode, InNodeToAdd->GetGuid())
														.OnClicked_Raw(this, &SSchematicGraphPanel::OnNodeClicked)
														.OnBeginDrag_Raw(this, &SSchematicGraphPanel::OnBeginDragEvent)
														.OnEndDrag_Raw(this, &SSchematicGraphPanel::OnEndDragEvent)
														.OnDrop_Raw(this, &SSchematicGraphPanel::OnDropEvent)
														.NodeData(InNodeToAdd);
	SNodePanel::AddGraphNode(NewNode);
	NodeByGuid.Add(Guid, NewNode.ToSharedPtr());
}

void SSchematicGraphPanel::RemoveNode(const FSchematicGraphNode* InNodeToRemove)
{
	const FGuid GuidToRemove = InNodeToRemove->GetGuid();
	NodeByGuid.Remove(GuidToRemove);
	
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = GetChild(Iter);
		if (Child->GetGuid() == GuidToRemove)
		{
			Children.RemoveAt(Iter);
			break;
		}
	}
	for (int32 Iter = 0; Iter != VisibleChildren.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = StaticCastSharedRef<SSchematicGraphNode>(VisibleChildren[Iter]);
		if (Child->GetGuid() == GuidToRemove)
		{
			VisibleChildren.RemoveAt(Iter);
			break;
		}
	}
}

const SSchematicGraphNode* SSchematicGraphPanel::FindNode(const FGuid& InGuid) const
{
	if(const TSharedPtr<SSchematicGraphNode>* FoundNodePtr = NodeByGuid.Find(InGuid))
	{
		return FoundNodePtr->Get();
	}
	return nullptr;
}

void SSchematicGraphPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	SNodePanel::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

int32 SSchematicGraphPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 NodeLayerId = LayerId + 1;
    int32 MaxLayerId = NodeLayerId;
    	
	if (!bIsOverlay)
	{
		const FSlateBrush* DefaultBackground = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
		PaintBackgroundAsLines(DefaultBackground, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		MaxLayerId++;
	}

	const float BackgroundAlpha = FadeBackgroundAlpha->Get();
	if (BackgroundAlpha > 0.f)
	{
		const FSlateBrush* Brush = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
		const FLinearColor TransparentGrey = FLinearColor(0.5, 0.5, 0.5, BackgroundAlpha);
		FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayerId,
				AllottedGeometry.ToPaintGeometry(),
				Brush,
				ESlateDrawEffect::None,
				TransparentGrey
				);
		MaxLayerId++;
	}
	
	

	if (!GraphData)
	{
		return MaxLayerId; 
	}

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildNodes(AllottedGeometry, ArrangedChildren);

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.

	const FPaintArgs NewArgs = Args.WithNewParent(this);

	// Draw the child nodes
	{
		for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
		{
			FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
			TSharedRef<SSchematicGraphNode> ChildNode = StaticCastSharedRef<SSchematicGraphNode>(CurWidget.Widget);
			
			// Examine node to see what layers we should be drawing in
			const int32 ChildLayerId = NodeLayerId;

			const bool bNodeIsVisible = FSlateRect::DoRectanglesIntersect( CurWidget.Geometry.GetLayoutBoundingRect(), MyCullingRect );

			if (bNodeIsVisible)
			{
				const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, MyCullingRect, OutDrawElements, ChildLayerId, InWidgetStyle, true );
				MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId + 1 );
			}
		}
	}

	// Draw the software cursor
	++MaxLayerId;
	PaintSoftwareCursor(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayerId);

	return MaxLayerId;
}

void SSchematicGraphPanel::RemoveAllNodes()
{
	NodeByGuid.Reset();
	SNodePanel::RemoveAllNodes();
}

TSharedRef<SSchematicGraphNode> SSchematicGraphPanel::GetChild(int32 ChildIndex) const
{
	return StaticCastSharedRef<SSchematicGraphNode>(Children[ChildIndex]);
}

TStatId SSchematicGraphPanel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(SSchematicGraphPanel, STATGROUP_Tickables);
}

void SSchematicGraphPanel::Tick(float DeltaTime)
{
	bIsDragDropping = false;
	FSlateApplication& Application = FSlateApplication::Get();
	if (Application.IsDragDropping())
	{
		TSharedPtr<FDragDropOperation> DragDropOp = Application.GetDragDroppingContent();
		if (DragDropOp.IsValid())
		{
			bIsDragDropping = true;
		}
	}

	NodesTopLeft.Reset();
	NodesTopRight.Reset();
	NodesBottomLeft.Reset();
	NodesBottomRight.Reset();

	FVector2D Size = GetCachedGeometry().Size;
	for (int32 i=0; i<Children.Num(); ++i)
	{
		TSharedRef<SSchematicGraphNode> Node = GetChild(i);

		// update the animation state of the node
		Node->EnablePositionAnimation(GraphData->GetPositionAnimationEnabledForNode(Node->GetGuid()));

		// collect the nodes displayed in the corners
		switch(GraphData->GetPlacementForNode(Node->GetNodeData()))
		{
			case ESchematicGraphNodePlacementConstraint::Free:
			{
				break;
			}
			case ESchematicGraphNodePlacementConstraint::TopLeft:
			{
				NodesTopLeft.Add(Node->GetGuid());
				break;
			}
			case ESchematicGraphNodePlacementConstraint::TopRight:
			{
				NodesTopRight.Add(Node->GetGuid());
				break;
			}
			case ESchematicGraphNodePlacementConstraint::BottomLeft:
			{
				NodesBottomLeft.Add(Node->GetGuid());
				break;
			}
			case ESchematicGraphNodePlacementConstraint::BottomRight:
			{
				NodesBottomRight.Add(Node->GetGuid());
				break;
			}
		}

		if (Node->IsBeingDragged())
		{
			if (bIsDragDropping)
			{
				const FVector2f AbsoluteMousePosition = FSlateApplication::Get().GetCursorPos();
				const FGeometry& Geometry = GetTickSpaceGeometry();

				const FVector2d LocalMousePosition =  (AbsoluteMousePosition - Geometry.GetAbsolutePosition()) / Geometry.GetAccumulatedLayoutTransform().GetScale();
				const FVector2d HalfOriginalSize = Node->GetOriginalSize() * 0.5;
				Node->PositionDuringDrag = LocalMousePosition + HalfOriginalSize;
			}
			else if (DeltaTime > 0.f)
			{
				Node->PositionDuringDrag.Reset();
				Node->OffsetDuringDrag.Reset();
				Node->bIsBeingDragged = false;
			}
		}
	}
}

void SSchematicGraphPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SNodePanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Tick(InDeltaTime);
}

void SSchematicGraphPanel::OnNodeClicked(SSchematicGraphNode* Node)
{
	OnNodeClickedDelegate.ExecuteIfBound(this, Node);
}

void SSchematicGraphPanel::OnBeginDragEvent(SSchematicGraphNode* Node, const FDragDropOperation& InDragDropOp)
{
	OnBeginDragDelegate.ExecuteIfBound(this, Node, InDragDropOp);
}

void SSchematicGraphPanel::OnEndDragEvent(SSchematicGraphNode* Node, const FDragDropOperation& InDragDropOp)
{
	OnEndDragDelegate.ExecuteIfBound(this, Node, InDragDropOp);
}

void SSchematicGraphPanel::OnDropEvent(SSchematicGraphNode* Node, const FDragDropEvent& InDragDropEvent)
{
	OnDropDelegate.ExecuteIfBound(this, Node, InDragDropEvent);
}

FVector2d SSchematicGraphPanel::GetPositionForNode(FGuid InNodeGuid) const
{
	const SSchematicGraphNode* NodeWidget = FindNode(InNodeGuid);
	if(GraphData && NodeWidget)
	{
		if(NodeWidget->PositionDuringDrag.IsSet())
		{
			return NodeWidget->PositionDuringDrag.GetValue() + NodeWidget->OffsetDuringDrag.Get(FVector2d::ZeroVector);
		}
		
		if(const FSchematicGraphNode* Node = GraphData->FindNode(InNodeGuid))
		{
			FVector2d Position = GraphData->GetPositionOffsetForNode(Node);

			const FVector2d OriginalSize = NodeWidget->GetOriginalSize();
			const FVector2D Size = GetCachedGeometry().Size;
			const FVector2d HalfOriginalSize = OriginalSize * 0.5;

			switch(GraphData->GetPlacementForNode(Node))
			{
				case ESchematicGraphNodePlacementConstraint::Free:
				{
					Position += GraphData->GetPositionForNode(Node);
					break;
				}
				case ESchematicGraphNodePlacementConstraint::TopLeft:
				{
					const int32 NodeIndexInCorner = NodesTopLeft.IndexOfByKey(InNodeGuid);
					check(NodeIndexInCorner != INDEX_NONE);
					Position += FVector2d(PaddingLeft + HalfOriginalSize.X, PaddingTop + HalfOriginalSize.Y + ((OriginalSize.Y + PaddingInterNode) * NodeIndexInCorner));
					break;
				}
				case ESchematicGraphNodePlacementConstraint::TopRight:
				{
					const int32 NodeIndexInCorner = NodesTopRight.IndexOfByKey(InNodeGuid);
					check(NodeIndexInCorner != INDEX_NONE);
					Position += FVector2d(Size.X - PaddingRight - HalfOriginalSize.X, PaddingTop + HalfOriginalSize.Y + ((OriginalSize.Y + PaddingInterNode) * NodeIndexInCorner));
					break;
				}
				case ESchematicGraphNodePlacementConstraint::BottomLeft:
				{
					const int32 NodeIndexInCorner = NodesBottomLeft.IndexOfByKey(InNodeGuid);
					check(NodeIndexInCorner != INDEX_NONE);
					Position += FVector2d(PaddingLeft + HalfOriginalSize.X, Size.Y - PaddingBottom - HalfOriginalSize.Y - ((OriginalSize.Y + PaddingInterNode)  * NodeIndexInCorner));
					break;
				}
				case ESchematicGraphNodePlacementConstraint::BottomRight:
				{
					const int32 NodeIndexInCorner = NodesBottomRight.IndexOfByKey(InNodeGuid);
					check(NodeIndexInCorner != INDEX_NONE);
					Position += FVector2d(Size.X - PaddingRight - HalfOriginalSize.X, Size.Y - PaddingBottom - HalfOriginalSize.Y - ((OriginalSize.Y + PaddingInterNode)  * NodeIndexInCorner));
					break;
				}
			}
			return Position;
		}
	}
	return FVector2d::ZeroVector;
}

FVector2d SSchematicGraphPanel::GetSizeForNode(FGuid InNodeGuid) const
{
	if(GraphData)
	{
		return GraphData->GetSizeForNode(InNodeGuid);
	}
	static const FVector2d DefaultSize = FVector2d(50.0,50.0);
	return DefaultSize;
}

float SSchematicGraphPanel::GetScaleForNode(FGuid InNodeGuid) const
{
	if(GraphData)
	{
		return GraphData->GetScaleForNode(InNodeGuid);
	}
	return 1.f;
}

FLinearColor SSchematicGraphPanel::GetColorForNode(FGuid InNodeGuid) const
{
	if(GraphData)
	{
		return GraphData->GetColorForNode(InNodeGuid);
	}
	return FLinearColor::White;
}

const FSlateBrush* SSchematicGraphPanel::GetBrushForNode(FGuid InNodeGuid) const
{
	if(GraphData)
	{
		return GraphData->GetBrushForNode(InNodeGuid);
	}
	static const FSlateBrush* DefaultBrush = SSchematicGraphNode::FArguments()._Brush.Get();
	return DefaultBrush;
}

FText SSchematicGraphPanel::GetToolTipForNode(FGuid InNodeGuid) const
{
	if(GraphData)
	{
		return GraphData->GetToolTipForNode(InNodeGuid);
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE

#endif