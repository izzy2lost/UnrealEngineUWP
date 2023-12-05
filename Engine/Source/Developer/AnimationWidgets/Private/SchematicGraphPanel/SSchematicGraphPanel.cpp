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

	OnGraphReset().Broadcast();
}

bool FSchematicGraph::AddNode(const FString& InName)
{
	if (Nodes.ContainsByPredicate([InName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InName;
	}))
	{
		return false;
	}

	FSchematicGraphNode* NewElement = static_cast<FSchematicGraphNode*>(FMemory::Malloc(sizeof(FSchematicGraphNode)));
	new (NewElement) FSchematicGraphNode();
	NewElement->Name = InName;
	Nodes.Add(NewElement);
	OnNodeAddedDelegate.Broadcast(NewElement);
	return true;
}

bool FSchematicGraph::RenameNode(const FString& InOldName, const FString& InNewName)
{
	FSchematicGraphNode** FoundNode = Nodes.FindByPredicate([InOldName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InOldName;
	});

	if (FoundNode)
	{
		(*FoundNode)->Name = InNewName;
		return true;
	}
	
	return false;
}

bool FSchematicGraph::RemoveNode(const FString& InName)
{
	FSchematicGraphNode** FoundNode = Nodes.FindByPredicate([InName](const FSchematicGraphNode* Node)
	{
		return Node->Name == InName;
	});
	if (FoundNode)
	{
		OnNodeRemovedDelegate.Broadcast(*FoundNode);
		Nodes.Remove(*FoundNode);
		FMemory::Free(*FoundNode);
		return true;
	}
	return false;
}

void SSchematicGraphNode::Construct(const FArguments& InArgs)
{
	NodeData = InArgs._NodeData;
	OnClickedDelegate = InArgs._OnClicked;
	OnDropDelegate = InArgs._OnDrop;
	TEasingAttributeInterpolator<FVector2d>::FSettings Vector2DInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	Position = TAnimatedAttribute<FVector2d>::Create(Vector2DInterpSettings, FVector2d::ZeroVector);
	Size = TAnimatedAttribute<FVector2d>::Create(Vector2DInterpSettings, OriginalSize);
	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	Scale = TAnimatedAttribute<float>::Create(FloatInterpSettings, 1.f);
	Brush = *FAppStyle::GetBrush("WhiteTexture");
	Brush.TintColor = EStyleColor::AccentWhite;
}

FVector2D SSchematicGraphNode::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return OriginalSize*LayoutScaleMultiplier;
}

int32 SSchematicGraphNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NewLayerId = SNodePanel::SNode::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	NewLayerId++;

	//FVector2d CenterOffset = OriginalSize*-0.5;
	FVector2d CurSize = Size->Get() * Scale->Get();
	FVector2d SizeOffset = (CurSize-OriginalSize)*-0.5;
	//FVector2d TotalOffset = CenterOffset + SizeOffset;
	FVector2d TotalOffset = SizeOffset;

	FSlateDrawElement::MakeBox(
				OutDrawElements,
				NewLayerId,
				AllottedGeometry.ToPaintGeometry(CurSize, FSlateLayoutTransform(TotalOffset)),
				&Brush,
				ESlateDrawEffect::None,
				Brush.TintColor.GetSpecifiedColor()
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
	SNode::OnDrop(MyGeometry, DragDropEvent);
	OnDropDelegate.ExecuteIfBound(this, DragDropEvent);
	return FReply::Unhandled();
}

FReply SSchematicGraphNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SNode::OnMouseButtonDown(MyGeometry, MouseEvent);
	if (MouseEvent.GetPressedButtons().Contains(EKeys::LeftMouseButton))
	{
		OnClickedDelegate.ExecuteIfBound(this);
	}
	return FReply::Handled();
}

FVector2d SSchematicGraphNode::GetPosition() const
{
	return Position->Get() - (OriginalSize*0.5);
}

void SSchematicGraphPanel::SetSchematicGraph(FSchematicGraph* InGraphData)
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
	UpdateNodeWidgetDelegate = InArgs._OnUpdateNodeWidget;
	OnNodeClickedDelegate = InArgs._OnNodeClicked;
	OnDropDelegate = InArgs._OnDrop;

	TEasingAttributeInterpolator<float>::FSettings FloatInterpSettings(EEasingInterpolatorType::BounceEaseInOut, 0.2f);
	FadeBackgroundAlpha = TAnimatedAttribute<float>::Create(FloatInterpSettings, 0.f);
	
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
														.OnDrop_Raw(this, &SSchematicGraphPanel::OnDropEvent)
														.NodeData(NodeToAdd);
	SNodePanel::AddGraphNode(NewNode);
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
	bool bFadeBackgroundBlanket = false;
	FSlateApplication& Application = FSlateApplication::Get();
	if (Application.IsDragDropping())
	{
		TSharedPtr<FDragDropOperation> DragDropOp = Application.GetDragDroppingContent();
		if (DragDropOp.IsValid())
		{
			bFadeBackgroundBlanket = true;
		}
	}
	SetFadeBackground(bFadeBackgroundBlanket);
	
	for (int32 i=0; i<Children.Num(); ++i)
	{
		TSharedRef<SSchematicGraphNode> SNode = GetChild(i);
		UpdateNodeWidgetDelegate.Execute(this, SNode.ToSharedPtr());
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

void SSchematicGraphPanel::OnDropEvent(SSchematicGraphNode* Node, const FDragDropEvent& InDragDropEvent)
{
	OnDropDelegate.ExecuteIfBound(this, Node, InDragDropEvent);
}


#undef LOCTEXT_NAMESPACE

#endif