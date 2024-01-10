// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "SchematicGraphPanel/SchematicGraphNode.h"
#include "SchematicGraphPanel/SchematicGraphModel.h"

#define LOCTEXT_NAMESPACE "SchematicGraphNode"

const FSchematicGraphNode* FSchematicGraphNode::GetParentNode() const
{
	if(Model && ParentNodeGuid.IsValid())
	{
		return Model->FindNode(ParentNodeGuid);
	}
	return nullptr;
}

const FSchematicGraphNode* FSchematicGraphNode::GetChildNode(int32 InChildNodeIndex) const
{
	check(ChildNodeGuids.IsValidIndex(InChildNodeIndex));

	const FGuid& ChildNodeGuid = ChildNodeGuids[InChildNodeIndex];
	if(Model && ChildNodeGuid.IsValid())
	{
		return Model->FindNode(ChildNodeGuid);
	}
	return nullptr;
}

ESchematicGraphVisibility::Type FSchematicGraphNode::GetVisibility() const
{
	if(Visibility == ESchematicGraphVisibility::Hidden)
	{
		return Visibility;
	}
	
	if(const FSchematicGraphNode* ParentNode = GetParentNode())
	{
		const ESchematicGraphVisibility::Type ParentVisibility = Model->GetVisibilityForChildNodes(ParentNode);
		if(ParentVisibility != ESchematicGraphVisibility::Visible)
		{
			return ParentVisibility;
		}
	}
	return Visibility;
}

FString FSchematicGraphNode::GetDragDropDecoratorLabel() const
{
	return GetGuid().ToString();
}

bool FSchematicGraphNode::RemoveTag(const FGuid& InTagGuid)
{
	if(const FSchematicGraphTag* Tag = FindTag(InTagGuid))
	{
		if (Model && Model->OnTagRemovedDelegate.IsBound())
		{
			Model->OnTagRemovedDelegate.Broadcast(this, Tag);
		}
		const FGuid TagGuid = Tag->GetGuid();
		TagByGuid.Remove(Tag->GetGuid());
		Tags.RemoveAll([TagGuid](const TSharedPtr<FSchematicGraphTag>& ExistingTag) -> bool
		{
			return ExistingTag->GetGuid() == TagGuid;
		});
		return true;
	}
	return false;
}

void FSchematicGraphNode::NotifyTagAdded(const TSharedPtr<FSchematicGraphTag>& Tag)
{
	if (Model != nullptr && Model->OnTagAddedDelegate.IsBound())
	{
		Model->OnTagAddedDelegate.Broadcast(this, Tag.Get());
	}
}

ESchematicGraphVisibility::Type FSchematicGraphGroupNode::GetVisibilityForChildNodes() const
{
	if(!IsExpanded())
	{
		return ESchematicGraphVisibility::Hidden;
	}
	return FSchematicGraphNode::GetVisibilityForChildNodes();
}

#undef LOCTEXT_NAMESPACE

#endif