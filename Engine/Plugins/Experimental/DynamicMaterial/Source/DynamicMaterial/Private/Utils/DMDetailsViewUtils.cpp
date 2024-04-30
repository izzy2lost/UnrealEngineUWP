// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "Utils/DMDetailsViewUtils.h"
#include "IDetailTreeNode.h"

TSharedPtr<IDetailTreeNode> FDMDetailsViewUtils::SearchNodesForProperty(const TArray<TSharedRef<IDetailTreeNode>>& InNodes, FName InPropertyName)
{
	for (const TSharedRef<IDetailTreeNode>& ChildNode : InNodes)
	{
		switch (ChildNode->GetNodeType())
		{
			case EDetailNodeType::Category:
			{
				TArray<TSharedRef<IDetailTreeNode>> CategoryChildNodes;
				ChildNode->GetChildren(CategoryChildNodes);

				if (TSharedPtr<IDetailTreeNode> FoundNode = SearchNodesForProperty(CategoryChildNodes, InPropertyName))
				{
					return FoundNode;
				}

				break;
			}

			case EDetailNodeType::Item:
				if (ChildNode->GetNodeName() == InPropertyName)
				{
					return ChildNode;
				}
				break;

			default:
			// Do nothing
			break;
		}
	}

	return nullptr;
}
#endif
