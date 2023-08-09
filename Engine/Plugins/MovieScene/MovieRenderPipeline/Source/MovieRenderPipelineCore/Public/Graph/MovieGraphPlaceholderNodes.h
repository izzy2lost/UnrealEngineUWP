// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieGraphNode.h"

#include "MovieGraphPlaceholderNodes.generated.h"

/** A node which configures anti-aliasing settings. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphAntiAliasingNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphAntiAliasingNode() = default;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif
};