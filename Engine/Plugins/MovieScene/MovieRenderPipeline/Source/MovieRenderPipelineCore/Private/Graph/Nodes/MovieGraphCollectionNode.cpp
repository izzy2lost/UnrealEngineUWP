// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphCollectionNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Graph/MoviePipelineRenderLayerSubsystem.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MovieGraph"

UMovieGraphCollectionNode::UMovieGraphCollectionNode()
{
	Collection = CreateDefaultSubobject<UMovieGraphCollection>(TEXT("Collection"));

#if WITH_EDITOR
	Collection->OnCollectionNameChangedDelegate.AddWeakLambda(this, [this](UMovieGraphCollection* ChangedCollection)
	{
		OnNodeChangedDelegate.Broadcast(this);
	});
#endif
}

#if WITH_EDITOR
FText UMovieGraphCollectionNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText CollectionNodeName = LOCTEXT("NodeName_Collection", "Collection");
	static const FText CollectionNodeDescription = LOCTEXT("NodeDescription_Collection", "Collection\n{0}");

	if (bGetDescriptive && Collection && !Collection->GetCollectionName().IsEmpty())
	{
		return FText::Format(CollectionNodeDescription, FText::FromString(Collection->GetCollectionName()));
	}

	return CollectionNodeName;
}

FText UMovieGraphCollectionNode::GetMenuCategory() const
{
	return LOCTEXT("CollectionNode_Category", "Utility");
}

FLinearColor UMovieGraphCollectionNode::GetNodeTitleColor() const
{
	static const FLinearColor CollectionNodeColor = FLinearColor(0.047f, 0.501f, 0.654f);
	return CollectionNodeColor;
}

FSlateIcon UMovieGraphCollectionNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon CollectionIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SceneOutliner.NewFolderIcon");

	OutColor = FLinearColor::White;
	return CollectionIcon;
}
#endif // WITH_EDITOR

FString UMovieGraphCollectionNode::GetNodeInstanceName() const
{
	return Collection ? Collection->GetCollectionName() : FString();
}

#undef LOCTEXT_NAMESPACE // "MovieGraph"