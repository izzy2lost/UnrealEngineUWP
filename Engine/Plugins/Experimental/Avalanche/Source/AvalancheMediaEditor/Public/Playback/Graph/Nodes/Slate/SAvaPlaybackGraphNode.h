// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class SVerticalBox;
class UAvaPlaybackGraphNode;

class AVALANCHEMEDIAEDITOR_API SAvaPlaybackGraphNode : public SGraphNode
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPlaybackGraphNode) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, UAvaPlaybackGraphNode* InGraphNode);
	virtual void PostConstruct() {};
	
protected:
	
	//SGraphNode Interface
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	//~SGraphNode Interface
	
protected:

	TWeakObjectPtr<UAvaPlaybackGraphNode> PlaybackGraphNode;
};
