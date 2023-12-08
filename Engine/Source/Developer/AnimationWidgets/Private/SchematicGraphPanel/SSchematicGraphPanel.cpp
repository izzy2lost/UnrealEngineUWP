// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "DragAndDrop/AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "SSchematicGraphPanel"


void FSchematicGraph::Reset()
{
	Nodes.Reset();
	Links.Reset();

	if (OnGraphResetDelegate.IsBound())
	{
		OnGraphReset().Broadcast();
	}
}

FSchematicGraphNode* FSchematicGraph::AddNode(const FString& InName)
{
	if (Nodes.ContainsByPredicate([InName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InName;
	}))
	{
		return nullptr;
	}

	FSchematicGraphNode* NewElement = static_cast<FSchematicGraphNode*>(FMemory::Malloc(sizeof(FSchematicGraphNode)));
	new (NewElement) FSchematicGraphNode();
	NewElement->Name = InName;
	Nodes.Add(NewElement);

	if (OnNodeAddedDelegate.IsBound())
	{
		OnNodeAddedDelegate.Broadcast(NewElement);
	}
	return NewElement;
}

bool FSchematicGraph::RenameNode(const FString& InOldName, const FString& InNewName)
{
	FSchematicGraphNode** FoundNode = Nodes.FindByPredicate([InOldName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InOldName;
	});

	if (FoundNode)
	{
		if (OnNodeRenamedDelegate.IsBound())
		{
			OnNodeRenamedDelegate.Broadcast(*FoundNode);
		}
		(*FoundNode)->Name = InNewName;
		return true;
	}
	
	return false;
}

bool FSchematicGraph::RemoveNode(const FString& InName)
{
	FSchematicGraphNode** FoundNodePtr = Nodes.FindByPredicate([InName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InName;
	});
	if (FoundNodePtr)
	{
		FSchematicGraphNode* FoundNode = *FoundNodePtr;
		if (OnNodeRemovedDelegate.IsBound())
		{
			OnNodeRemovedDelegate.Broadcast(FoundNode);
		}
		Nodes.Remove(FoundNode);
		FMemory::Free(FoundNode);
		return true;
	}
	return false;
}

TSharedRef<FSchematicGraphNodeDragDropOp> FSchematicGraphNodeDragDropOp::New(TArray<SSchematicGraphNode*> InSchematicGraphNodes, const TArray<FString>& InElements, FSchematicGraphNodeDragDropOp::FOnEndDrag InOnEndDragDelegate)
{
	TSharedRef<FSchematicGraphNodeDragDropOp> Operation = MakeShared<FSchematicGraphNodeDragDropOp>();
	Operation->SchematicGraphNodes = InSchematicGraphNodes;
	Operation->Elements = InElements;
	Operation->OnEndDragDelegate = InOnEndDragDelegate;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FSchematicGraphNodeDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
		];
}

FString FSchematicGraphNodeDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FString& Element: Elements)
	{
		ElementNameStrings.Add(Element);
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

void SSchematicGraphNode::Construct(const FArguments& InArgs)
{
	NodeData = InArgs._NodeData;
	OnClickedDelegate = InArgs._OnClicked;
	OnBeginDragDelegate = InArgs._OnBeginDrag;
	OnEndDragDelegate = InArgs._OnEndDrag;
	OnDropDelegate = InArgs._OnDrop;
	TEasingAttributeInterpolator<FVector2d>::FSettings Vector2DInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	Position = TAnimatedAttribute<FVector2d>::Create(Vector2DInterpSettings, FVector2d::ZeroVector);
	Size = TAnimatedAttribute<FVector2d>::Create(Vector2DInterpSettings, OriginalSize);
	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	Scale = TAnimatedAttribute<float>::Create(FloatInterpSettings, 1.f);
	Brush = *FAppStyle::GetBrush("WhiteTexture");
	Brush.TintColor = EStyleColor::AccentWhite;
	SetToolTipText(FText::FromString(NodeData->Name));
}

FVector2D SSchematicGraphNode::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return OriginalSize*LayoutScaleMultiplier;
}

int32 SSchematicGraphNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NewLayerId = SNodePanel::SNode::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	NewLayerId++;

	FVector2d CurSize = Size->Get() * Scale->Get();
	FVector2d SizeOffset = (CurSize-OriginalSize)*-0.5;
	FVector2d TotalOffset = SizeOffset;

	FLinearColor TintColor = Brush.TintColor.GetSpecifiedColor();
	if (NodeData->bFade)
	{
		TintColor.A = 0.3;
	}

	FSlateDrawElement::MakeBox(
				OutDrawElements,
				NewLayerId,
				AllottedGeometry.ToPaintGeometry(CurSize, FSlateLayoutTransform(TotalOffset)),
				&Brush,
				ESlateDrawEffect::None,
				TintColor
				);
	
	return NewLayerId;
}

void SSchematicGraphNode::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SNode::OnMouseEnter(MyGeometry, MouseEvent);
	Scale->Set(ScaledUp);
}

void SSchematicGraphNode::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SNode::OnMouseLeave(MouseEvent);
	Scale->Set(1.f);
}

FReply SSchematicGraphNode::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SNode::OnDragOver(MyGeometry, DragDropEvent);
	Scale->Set(ScaledDown);
	return FReply::Handled();
}

void SSchematicGraphNode::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SNode::OnDragLeave(DragDropEvent);
	Scale->Set(1.f);
}

FReply SSchematicGraphNode::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// Avoid dropping onto itself
	TSharedPtr<FSchematicGraphNodeDragDropOp> SchematicDragDropOp = DragDropEvent.GetOperationAs<FSchematicGraphNodeDragDropOp>();
	if (SchematicDragDropOp)
	{
		if (!SchematicDragDropOp->GetElements().IsEmpty())
		{
			if (SchematicDragDropOp->GetElements().ContainsByPredicate([this](const FString& Name)
			{
				return Name == NodeData->Name;
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
	TArray<FString> DraggedElements = {NodeData->Name};
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && DraggedElements.Num() > 0)
	{
		bIsBeingDragged = true;
		TSharedRef<FSchematicGraphNodeDragDropOp> DragDropOp = FSchematicGraphNodeDragDropOp::New({this}, MoveTemp(DraggedElements), OnEndDragDelegate);
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

void SSchematicGraphNode::SetPosition(const FVector2d& InPosition, bool bImmediately)
{
	bImmediately ?
		Position->SetValueAndStop(FVector2d(InPosition))
		: Position->Set(InPosition);
}

void SSchematicGraphPanel::SetSchematicGraph(FSchematicGraph* InGraphData)
{
	if (GraphData)
	{
		GraphData->OnNodeAdded().RemoveAll(this);
		GraphData->OnNodeRemoved().RemoveAll(this);
		GraphData->OnNodeRenamed().RemoveAll(this);
		GraphData->OnGraphReset().RemoveAll(this);
	}
	
	GraphData = InGraphData;
	if (GraphData)
	{
		GraphData->OnNodeAdded().AddSP(this, &SSchematicGraphPanel::AddNode);
		GraphData->OnNodeRemoved().AddSP(this, &SSchematicGraphPanel::RemoveNode);
		GraphData->OnNodeRenamed().AddSP(this, &SSchematicGraphPanel::RenameNode);
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
	UpdateNodeWidgetDelegate = InArgs._OnUpdateNodeWidget;
	OnNodeClickedDelegate = InArgs._OnNodeClicked;
	OnBeginDragDelegate = InArgs._OnBeginDrag;
	OnEndDragDelegate = InArgs._OnEndDrag;
	OnDropDelegate = InArgs._OnDrop;

	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	FadeBackgroundAlpha = TAnimatedAttribute<float>::Create(FloatInterpSettings, 0.f);
	
	SNodePanel::Construct();

	SetVisibility(bIsOverlay ? EVisibility::SelfHitTestInvisible : EVisibility::Visible);
	if (GraphData)
	{
		GraphData->OnNodeAdded().AddSP(this, &SSchematicGraphPanel::AddNode);
		GraphData->OnNodeRemoved().AddSP(this, &SSchematicGraphPanel::RemoveNode);
		GraphData->OnNodeRenamed().AddSP(this, &SSchematicGraphPanel::RenameNode);
		GraphData->OnGraphReset().AddSP(this, &SSchematicGraphPanel::RebuildPanel);
	}
}

void SSchematicGraphPanel::RebuildPanel()
{
	RemoveAllNodes();

	if (GraphData)
	{
		for (FSchematicGraphNode* Node : GraphData->Nodes)
		{
			AddNode(Node);
		}
	}
}

void SSchematicGraphPanel::AddNode(FSchematicGraphNode* NodeToAdd)
{
	TSharedRef<SSchematicGraphNode> NewNode = SNew(SSchematicGraphNode)
														.OnClicked_Raw(this, &SSchematicGraphPanel::OnNodeClicked)
														.OnBeginDrag_Raw(this, &SSchematicGraphPanel::OnBeginDragEvent)
														.OnEndDrag_Raw(this, &SSchematicGraphPanel::OnEndDragEvent)
														.OnDrop_Raw(this, &SSchematicGraphPanel::OnDropEvent)
														.NodeData(NodeToAdd);
	SNodePanel::AddGraphNode(NewNode);

	// Update the position/size/brush immediately without animation
	{
		TGuardValue<bool> AnimateNodeGuard(bAnimatePosition, false);
		Tick(0.f);
	}
}

void SSchematicGraphPanel::RemoveNode(FSchematicGraphNode* InNodeToRemove)
{
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = GetChild(Iter);
		if (Child->NodeData == InNodeToRemove)
		{
			Children.RemoveAt(Iter);
			break;
		}
	}
	for (int32 Iter = 0; Iter != VisibleChildren.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = StaticCastSharedRef<SSchematicGraphNode>(VisibleChildren[Iter]);
		if (Child->NodeData == InNodeToRemove)
		{
			VisibleChildren.RemoveAt(Iter);
			break;
		}
	}
}

void SSchematicGraphPanel::RenameNode(FSchematicGraphNode* InNodeToRename)
{
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = GetChild(Iter);
		if (Child->NodeData == InNodeToRename)
		{
			Child->SetToolTipText(FText::FromString(InNodeToRename->Name));
			break;
		}
	}
	for (int32 Iter = 0; Iter != VisibleChildren.Num(); ++Iter)
	{
		TSharedRef<SSchematicGraphNode> Child = StaticCastSharedRef<SSchematicGraphNode>(VisibleChildren[Iter]);
		if (Child->NodeData == InNodeToRename)
		{
			Child->SetToolTipText(FText::FromString(InNodeToRename->Name));
			break;
		}
	}
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

	if (FadeBackgroundAlpha->Get() > 0.f)
	{
		const FSlateBrush* Brush = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
		FLinearColor TransparentGrey = FLinearColor(0.5, 0.5, 0.5, FadeBackgroundAlpha->Get());
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
			int32 ChildLayerId = NodeLayerId;

			const bool bNodeIsVisible = FSlateRect::DoRectanglesIntersect( CurWidget.Geometry.GetLayoutBoundingRect(), MyCullingRect );

			if (bNodeIsVisible)
			{
				int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, MyCullingRect, OutDrawElements, ChildLayerId, InWidgetStyle, true );
				MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId + 1 );
			}
		}
	}

	// Draw the software cursor
	++MaxLayerId;
	PaintSoftwareCursor(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayerId);

	return MaxLayerId;
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
	bool bIsDragging = false;
	FSlateApplication& Application = FSlateApplication::Get();
	if (Application.IsDragDropping())
	{
		TSharedPtr<FDragDropOperation> DragDropOp = Application.GetDragDroppingContent();
		if (DragDropOp.IsValid())
		{
			bIsDragging = true;
		}
	}
	SetFadeBackground(bIsDragging);

	uint16 TopRightNodes = 0;
	uint16 TopLeftNodes = 0;
	uint16 BottomRightNodes = 0;
	uint16 BottomLeftNodes = 0;
	FVector2D Size = GetCachedGeometry().Size;
	for (int32 i=0; i<Children.Num(); ++i)
	{
		TSharedRef<SSchematicGraphNode> SNode = GetChild(i);
		UpdateNodeWidgetDelegate.ExecuteIfBound(this, SNode.ToSharedPtr());

		FVector2d NewPosition = SNode->GetPosition() + (SNode->OriginalSize*0.5);
		bool bImmediatePosition = !bAnimatePosition;
		switch(SNode->NodeData->Placement)
		{
			case ESchematicGraphNodePlacement::Panel:
			{
				break;
			}
			case ESchematicGraphNodePlacement::TopLeft:
			{
				NewPosition = FVector2d(PaddingLeft + SNode->OriginalSize.X*0.5, PaddingTop + SNode->OriginalSize.Y*0.5 + ((SNode->OriginalSize.Y + PaddingInterNode) * TopLeftNodes));
				TopLeftNodes++;
				break;
			}
			case ESchematicGraphNodePlacement::TopRight:
			{
				NewPosition = FVector2d(Size.X - PaddingRight - (SNode->OriginalSize.X * 0.5), PaddingTop + SNode->OriginalSize.Y*0.5 + ((SNode->OriginalSize.Y + PaddingInterNode) * TopRightNodes));
				TopRightNodes++;
				break;
			}
			case ESchematicGraphNodePlacement::BottomLeft:
			{
				NewPosition = FVector2d(PaddingLeft + SNode->OriginalSize.X*0.5, Size.Y - PaddingBottom - (SNode->OriginalSize.Y * 0.5) - ((SNode->OriginalSize.Y + PaddingInterNode)  * BottomLeftNodes));
				BottomLeftNodes++;
				break;
			}
			case ESchematicGraphNodePlacement::BottomRight:
			{
				NewPosition = FVector2d(Size.X - PaddingRight - (SNode->OriginalSize.X * 0.5), Size.Y - PaddingBottom - (SNode->OriginalSize.Y * 0.5) - ((SNode->OriginalSize.Y + PaddingInterNode)  * BottomRightNodes));
				BottomRightNodes++;
				break;
			}
		}

		if (SNode->bIsBeingDragged)
		{
			if (bIsDragging)
			{
				const FVector2f AbsoluteMousePosition = FSlateApplication::Get().GetCursorPos();
				const FGeometry& Geometry = GetTickSpaceGeometry();

				const FVector2d LocalMousePosition =  (AbsoluteMousePosition - Geometry.GetAbsolutePosition()) / Geometry.GetAccumulatedLayoutTransform().GetScale();
				NewPosition = LocalMousePosition;
				bImmediatePosition = true;
				
			}
			else if (DeltaTime > 0.f)
			{
				SNode->bIsBeingDragged = false;
			}
		}

		SNode->SetPosition(NewPosition, bImmediatePosition);
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


#undef LOCTEXT_NAMESPACE

#endif