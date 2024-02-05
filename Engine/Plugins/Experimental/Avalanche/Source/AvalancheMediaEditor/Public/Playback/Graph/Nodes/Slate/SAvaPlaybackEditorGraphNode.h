// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class SVerticalBox;
class UAvaPlaybackEditorGraphNode;

class AVALANCHEMEDIAEDITOR_API SAvaPlaybackEditorGraphNode : public SGraphNode
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPlaybackEditorGraphNode) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, UAvaPlaybackEditorGraphNode* InGraphNode);
	virtual void PostConstruct() {};
	
protected:
	
	//SGraphNode Interface
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	//~SGraphNode Interface
	
protected:

	TWeakObjectPtr<UAvaPlaybackEditorGraphNode> PlaybackGraphNode;
};
