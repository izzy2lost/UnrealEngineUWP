// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "SchematicGraphPanel/SchematicGraphModel.h"
#include "SchematicGraphPanel/SSchematicGraphPanel.h"

#define LOCTEXT_NAMESPACE "SchematicGraphModel"

void FSchematicGraphModel::Reset()
{
	Nodes.Reset();
	Links.Reset();

	if (OnGraphResetDelegate.IsBound())
	{
		OnGraphReset().Broadcast();
	}
}

bool FSchematicGraphModel::RemoveNode(const FGuid& InNodeGuid)
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(TTuple<TArray<FGuid>,TArray<FGuid>>* ExistingTuple = NodeGuidToLinkGuids.Find(InNodeGuid))
		{
			TTuple<TArray<FGuid>,TArray<FGuid>> LinksToNode;
			Swap(LinksToNode, *ExistingTuple);
			NodeGuidToLinkGuids.Remove(InNodeGuid);
			for(const FGuid& LinkGuid : LinksToNode.Get<0>())
			{
				(void)RemoveLink(LinkGuid);
			}
			for(const FGuid& LinkGuid : LinksToNode.Get<1>())
			{
				(void)RemoveLink(LinkGuid);
			}
		}
		
		if (OnNodeRemovedDelegate.IsBound())
		{
			OnNodeRemovedDelegate.Broadcast(Node);
		}
		const FGuid NodeGuid = Node->GetGuid();
		NodeByGuid.Remove(Node->GetGuid());
		Nodes.RemoveAll([NodeGuid](const TSharedPtr<FSchematicGraphNode>& ExistingNode) -> bool
		{
			return ExistingNode->GetGuid() == NodeGuid;
		});
		return true;
	}
	return false;
}

bool FSchematicGraphModel::SetParentNode(const FGuid& InChildNodeGuid, const FGuid& InParentNodeGuid)
{
	if(const FSchematicGraphNode* Node = FindNode(InChildNodeGuid))
	{
		return SetParentNode(Node, FindNode(InParentNodeGuid));
	}
	return false;
}

bool FSchematicGraphModel::SetParentNode(const FSchematicGraphNode* InChildNode, const FSchematicGraphNode* InParentNode)
{
	check(InChildNode);

	if(InParentNode)
	{
		if(InParentNode == InChildNode->GetParentNode())
		{
			return false;
		}
		
		(void)RemoveFromParentNode(InChildNode);
		const_cast<FSchematicGraphNode*>(InParentNode)->ChildNodeGuids.AddUnique(InChildNode->GetGuid());
		const_cast<FSchematicGraphNode*>(InChildNode)->ParentNodeGuid = InParentNode->GetGuid();
		return true;
	}
	
	if(const FSchematicGraphNode* CurrentParentNode = InChildNode->GetParentNode())
	{
		(void)const_cast<FSchematicGraphNode*>(CurrentParentNode)->ChildNodeGuids.Remove(InChildNode->GetGuid());
		const_cast<FSchematicGraphNode*>(InChildNode)->ParentNodeGuid = FGuid();
		return true;
	}
	
	return false;
}

bool FSchematicGraphModel::RemoveFromParentNode(const FGuid& InChildNodeGuid)
{
	if(const FSchematicGraphNode* Node = FindNode(InChildNodeGuid))
	{
		return RemoveFromParentNode(Node);
	}
	return false;
}

bool FSchematicGraphModel::RemoveFromParentNode(const FSchematicGraphNode* InChildNode)
{
	check(InChildNode);
	return SetParentNode(InChildNode, nullptr);
}

FVector2d FSchematicGraphModel::GetPositionForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
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

FVector2d FSchematicGraphModel::GetPositionOffsetForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
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
	return SSchematicGraphNode::DefaultNodeSize;
}

FVector2d FSchematicGraphModel::GetSizeForNode(const FSchematicGraphNode* InNode) const
{
	return SSchematicGraphNode::DefaultNodeSize;
}

float FSchematicGraphModel::GetScaleForNode(const FGuid& InNodeGuid, bool bIncludeScaleOffset) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetScaleForNode(Node, bIncludeScaleOffset);
	}
	return 1.f;
}

float FSchematicGraphModel::GetScaleForNode(const FSchematicGraphNode* InNode, bool bIncludeScaleOffset) const
{
	if(bIncludeScaleOffset)
	{
		return GetScaleOffsetForNode(InNode);
	}
	return 1.f;
}

float FSchematicGraphModel::GetScaleOffsetForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetScaleOffsetForNode(Node);
	}
	return 1.f;
}

float FSchematicGraphModel::GetScaleOffsetForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetScaleOffset();
}

 float FSchematicGraphModel::GetMinimumLinkDistanceForNode(const FGuid& InNodeGuid) const
 {
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetMinimumLinkDistanceForNode(Node);
	}
	return 0;
 }

 float FSchematicGraphModel::GetMinimumLinkDistanceForNode(const FSchematicGraphNode* InNode) const
 {
	check(InNode);

	// return the radius of the node - 1 pixel
	const FVector2d Size = GetSizeForNode(InNode);
	return Size.GetMax() * 0.5f - 1.f;
 }

 bool FSchematicGraphModel::IsAutoScaleEnabledForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return IsAutoScaleEnabledForNode(Node);
	}
	return false;
}

bool FSchematicGraphModel::IsAutoScaleEnabledForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->IsAutoScaleEnabled();
}

int32 FSchematicGraphModel::GetNumLayersForNode(const FGuid& InNodeGuid) const
 {
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetNumLayersForNode(Node);
	}
	return 0;
}

int32 FSchematicGraphModel::GetNumLayersForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetNumLayers();
}

FLinearColor FSchematicGraphModel::GetColorForNode(const FGuid& InNodeGuid, int32 InLayerIndex) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		const FLinearColor Color = GetColorForNode(Node, InLayerIndex);
		if(GetVisibilityForNode(Node) == ESchematicGraphVisibility::FadedOut)
		{
			return Color * 0.5f;
		}
		return Color;
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetColorForNode(const FSchematicGraphNode* InNode, int32 InLayerIndex) const
{
	check(InNode);
	return InNode->GetColor(InLayerIndex);
}

const FSlateBrush* FSchematicGraphModel::GetBrushForNode(const FGuid& InNodeGuid, int32 InLayerIndex) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSlateBrush* Brush = GetBrushForNode(Node, InLayerIndex))
		{
			return Brush;
		}
	}

	static const FSlateBrush* DefaultBrush = SSchematicGraphNode::FArguments()._BrushGetter(FGuid(), INDEX_NONE);
	return DefaultBrush;
}

const FSlateBrush* FSchematicGraphModel::GetBrushForNode(const FSchematicGraphNode* InNode, int32 InLayerIndex) const
{
	check(InNode);
	return InNode->GetBrush(InLayerIndex);
}

FText FSchematicGraphModel::GetToolTipForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetToolTipForNode(Node);
	}
	return FText();
}

FText FSchematicGraphModel::GetToolTipForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetToolTip();
}

ESchematicGraphPlacementConstraint::Type FSchematicGraphModel::GetPlacementForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetPlacementForNode(Node);
	}
	return ESchematicGraphPlacementConstraint::Free;
}

ESchematicGraphPlacementConstraint::Type FSchematicGraphModel::GetPlacementForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetPlacement();
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetVisibilityForNode(Node);
	}
	return ESchematicGraphVisibility::Visible;
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetVisibility();
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForChildNodes(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return GetVisibilityForChildNodes(Node);
	}
	return ESchematicGraphVisibility::Visible;
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForChildNodes(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->GetVisibilityForChildNodes();
}

bool FSchematicGraphModel::IsDragSupportedForNode(const FGuid& InNodeGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		return IsDragSupportedForNode(Node);
	}
	return false;
}

bool FSchematicGraphModel::IsDragSupportedForNode(const FSchematicGraphNode* InNode) const
{
	check(InNode);
	return InNode->IsDragSupported();
}

FLinearColor FSchematicGraphModel::GetBackgroundColorForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetBackgroundColorForTag(Tag);
		}
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetBackgroundColorForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetBackgroundColor();
}

FLinearColor FSchematicGraphModel::GetForegroundColorForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetForegroundColorForTag(Tag);
		}
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetForegroundColorForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetForegroundColor();
}

FLinearColor FSchematicGraphModel::GetLabelColorForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetLabelColorForTag(Tag);
		}
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetLabelColorForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetLabelColor();
}

const FSlateBrush* FSchematicGraphModel::GetBackgroundBrushForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetBackgroundBrushForTag(Tag);
		}
	}
	return nullptr;
}

const FSlateBrush* FSchematicGraphModel::GetBackgroundBrushForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetBackgroundBrush();
}

const FSlateBrush* FSchematicGraphModel::GetForegroundBrushForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetForegroundBrushForTag(Tag);
		}
	}
	return nullptr;
}

const FSlateBrush* FSchematicGraphModel::GetForegroundBrushForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetForegroundBrush();
}

const FText FSchematicGraphModel::GetLabelForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetLabelForTag(Tag);
		}
	}
	return FText();
}

const FText FSchematicGraphModel::GetLabelForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetLabel();
}

const FText FSchematicGraphModel::GetToolTipForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetToolTipForTag(Tag);
		}
	}
	return FText();
}

const FText FSchematicGraphModel::GetToolTipForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetToolTip();
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForTag(const FGuid& InNodeGuid, const FGuid& InTagGuid) const
{
	if(const FSchematicGraphNode* Node = FindNode(InNodeGuid))
	{
		if(const FSchematicGraphTag* Tag = Node->FindTag(InTagGuid))
		{
			return GetVisibilityForTag(Tag);
		}
	}
	return ESchematicGraphVisibility::Visible;
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForTag(const FSchematicGraphTag* InTag) const
{
	check(InTag);
	return InTag->GetVisibility();
}

bool FSchematicGraphModel::IsLinkedTo(const FGuid& InSourceNodeGuid, const FGuid& InTargetNodeGuid) const
{
	return FindLink<>(InSourceNodeGuid, InTargetNodeGuid) != nullptr;
}

TArray<const FSchematicGraphLink*> FSchematicGraphModel::FindLinksOnNode(const FGuid& InNodeGuid) const
{
	TArray<const FSchematicGraphLink*> Result;
	for(const TSharedPtr<FSchematicGraphLink>& Link : Links)
	{
		if(Link->GetSourceNodeGuid() == InNodeGuid ||
			Link->GetTargetNodeGuid() == InNodeGuid)
		{
			Result.Add(Link.Get());
		}
	}
	return Result;
}

TArray<const FSchematicGraphLink*> FSchematicGraphModel::FindLinksOnSource(const FGuid& InSourceNodeGuid) const
{
	TArray<const FSchematicGraphLink*> Result;
	for(const TSharedPtr<FSchematicGraphLink>& Link : Links)
	{
		if(Link->GetSourceNodeGuid() == InSourceNodeGuid)
		{
			Result.Add(Link.Get());
		}
	}
	return Result;
}

TArray<const FSchematicGraphLink*> FSchematicGraphModel::FindLinksOnTarget(const FGuid& InTargetNodeGuid) const
{
	TArray<const FSchematicGraphLink*> Result;
	for(const TSharedPtr<FSchematicGraphLink>& Link : Links)
	{
		if(Link->GetTargetNodeGuid() == InTargetNodeGuid)
		{
			Result.Add(Link.Get());
		}
	}
	return Result;
}

bool FSchematicGraphModel::RemoveLink(const FGuid& InLinkGuid)
{
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		if (OnLinkRemovedDelegate.IsBound())
		{
			OnLinkRemovedDelegate.Broadcast(Link);
		}
		const FGuid LinkGuid = Link->GetGuid();
		const uint32 LinkHash = Link->GetLinkHash();
		const FGuid SourceNodeGuid = Link->GetSourceNodeGuid();
		const FGuid TargetNodeGuid = Link->GetTargetNodeGuid();
		LinkByGuid.Remove(Link->GetGuid());
		LinkByHash.Remove(LinkHash);
		if(TTuple<TArray<FGuid>,TArray<FGuid>>* Tuple = NodeGuidToLinkGuids.Find(SourceNodeGuid))
		{
			Tuple->Get<0>().Remove(LinkGuid);
			if(Tuple->Get<0>().IsEmpty() && Tuple->Get<1>().IsEmpty())
			{
				NodeGuidToLinkGuids.Remove(SourceNodeGuid);
			}
		}
		if(TTuple<TArray<FGuid>,TArray<FGuid>>* Tuple = NodeGuidToLinkGuids.Find(TargetNodeGuid))
		{
			Tuple->Get<1>().Remove(LinkGuid);
			if(Tuple->Get<0>().IsEmpty() && Tuple->Get<1>().IsEmpty())
			{
				NodeGuidToLinkGuids.Remove(TargetNodeGuid);
			}
		}

		Links.RemoveAll([LinkGuid](const TSharedPtr<FSchematicGraphLink>& ExistingLink) -> bool
		{
			return ExistingLink->GetGuid() == LinkGuid;
		});
		return true;
	}
	return false;
}

 float FSchematicGraphModel::GetMinimumForLink(const FGuid& InLinkGuid) const
 {
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetMinimumForLink(Link);
	}
	return 0.f;
 }

 float FSchematicGraphModel::GetMinimumForLink(const FSchematicGraphLink* InLink) const
 {
	check(InLink);
	return InLink->GetMinimum();
 }

 float FSchematicGraphModel::GetMaximumForLink(const FGuid& InLinkGuid) const
 {
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetMaximumForLink(Link);
	}
	return 1.f;
 }

 float FSchematicGraphModel::GetMaximumForLink(const FSchematicGraphLink* InLink) const
 {
	check(InLink);
	return InLink->GetMaximum();
 }

FVector2d FSchematicGraphModel::GetSourceNodeOffsetForLink(const FGuid& InLinkGuid) const
 {
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetSourceNodeOffsetForLink(Link);
	}
	return FVector2d::ZeroVector;
 }

 FVector2d FSchematicGraphModel::GetSourceNodeOffsetForLink(const FSchematicGraphLink* InLink) const
 {
	check(InLink);
	return InLink->GetSourceNodeOffset();
 }

 FVector2d FSchematicGraphModel::GetTargetNodeOffsetForLink(const FGuid& InLinkGuid) const
 {
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetTargetNodeOffsetForLink(Link);
	}
	return FVector2d::ZeroVector;
 }

 FVector2d FSchematicGraphModel::GetTargetNodeOffsetForLink(const FSchematicGraphLink* InLink) const
 {
	check(InLink);
	return InLink->GetTargetNodeOffset();
 }

 FLinearColor FSchematicGraphModel::GetColorForLink(const FGuid& InLinkGuid) const
{
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		const FLinearColor Color = GetColorForLink(Link);
		if(GetVisibilityForLink(Link) == ESchematicGraphVisibility::FadedOut)
		{
			return Color * 0.5f;
		}
		return Color;
	}
	return FLinearColor::White;
}

FLinearColor FSchematicGraphModel::GetColorForLink(const FSchematicGraphLink* InLink) const
{
	check(InLink);
	return InLink->GetColor();
}

 float FSchematicGraphModel::GetThicknessForLink(const FGuid& InLinkGuid) const
 {
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetThicknessForLink(Link);
	}
	return 1.f;
 }

 float FSchematicGraphModel::GetThicknessForLink(const FSchematicGraphLink* InLink) const
 {
	check(InLink);
	return InLink->GetThickness();
 }

 const FSlateBrush* FSchematicGraphModel::GetBrushForLink(const FGuid& InLinkGuid) const
{
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetBrushForLink(Link);
	}
	return nullptr;
}

const FSlateBrush* FSchematicGraphModel::GetBrushForLink(const FSchematicGraphLink* InLink) const
{
	check(InLink);
	return InLink->GetBrush();
}

const FText FSchematicGraphModel::GetToolTipForLink(const FGuid& InLinkGuid) const
{
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetToolTipForLink(Link);
	}
	return FText();
}

const FText FSchematicGraphModel::GetToolTipForLink(const FSchematicGraphLink* InLink) const
{
	check(InLink);
	return InLink->GetToolTip();
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForLink(const FGuid& InLinkGuid) const
{
	if(const FSchematicGraphLink* Link = FindLink(InLinkGuid))
	{
		return GetVisibilityForLink(Link);
	}
	return ESchematicGraphVisibility::Visible;
}

ESchematicGraphVisibility::Type FSchematicGraphModel::GetVisibilityForLink(const FSchematicGraphLink* InLink) const
{
	check(InLink);
	return InLink->GetVisibility();
}

#undef LOCTEXT_NAMESPACE

#endif