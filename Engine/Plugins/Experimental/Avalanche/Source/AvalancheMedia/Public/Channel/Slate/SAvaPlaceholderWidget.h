// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

class AVALANCHEMEDIA_API SAvaPlaceholderWidget : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPlaceholderWidget) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	void SetChannelName(const FText& InChannelName);
	
protected:

	TSharedPtr<STextBlock> ChannelText;
};
