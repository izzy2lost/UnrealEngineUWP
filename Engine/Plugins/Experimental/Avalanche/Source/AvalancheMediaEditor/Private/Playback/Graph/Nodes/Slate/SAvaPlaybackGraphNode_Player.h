// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Graph/Nodes/Slate/SAvaPlaybackGraphNode.h"

class UAvaPlaybackNodePlayer;

class SAvaPlaybackGraphNode_Player : public SAvaPlaybackGraphNode
{
public:
	SAvaPlaybackGraphNode_Player();
	~SAvaPlaybackGraphNode_Player();

	virtual void PostConstruct() override;

	/** Creates a preview viewport if necessary */
	TSharedRef<SWidget> CreatePreviewWidget();

	const FSlateBrush* GetPlayerPreviewBrush() const;

	//SGraphNode Interface
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	//~SGraphNode Interface

protected:

	TWeakObjectPtr<UAvaPlaybackNodePlayer> PlayerNode;

	struct FPreviewBrush;
	TUniquePtr<FPreviewBrush> PlayerPreviewBrush;
};
