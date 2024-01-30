// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Graph/Nodes/Slate/SAvaPlaybackGraphNode.h"

class UAvaPlaybackGraphNode_Root;

class SAvaPlaybackGraphNode_Root : public SAvaPlaybackGraphNode
{
public:

	/** Hook that allows derived classes to supply their own SGraphPin derivatives for any pin. */
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
};
