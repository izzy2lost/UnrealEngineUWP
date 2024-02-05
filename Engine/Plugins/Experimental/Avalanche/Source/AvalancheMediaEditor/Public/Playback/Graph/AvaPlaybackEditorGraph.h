// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "AvaPlaybackEditorGraph.generated.h"

class UAvaPlaybackGraph;
class UAvaPlaybackEditorGraphNode;

UCLASS()
class UAvaPlaybackEditorGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	
	UAvaPlaybackGraph* GetPlaybackGraph() const;
	
	UAvaPlaybackEditorGraphNode* CreatePlaybackEditorGraphNode(TSubclassOf<UAvaPlaybackEditorGraphNode> NewNodeClass
		, bool bSelectNewNode = true);
};
