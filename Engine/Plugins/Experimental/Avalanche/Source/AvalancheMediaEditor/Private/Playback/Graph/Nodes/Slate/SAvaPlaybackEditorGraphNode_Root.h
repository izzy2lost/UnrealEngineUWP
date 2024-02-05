// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Graph/Nodes/Slate/SAvaPlaybackEditorGraphNode.h"

class UAvaPlaybackEditorGraphNode_Root;

class SAvaPlaybackEditorGraphNode_Root : public SAvaPlaybackEditorGraphNode
{
public:

	/** Hook that allows derived classes to supply their own SGraphPin derivatives for any pin. */
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
};
