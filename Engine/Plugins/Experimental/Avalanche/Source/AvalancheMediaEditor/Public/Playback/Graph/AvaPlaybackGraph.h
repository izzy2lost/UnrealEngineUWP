// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "AvaPlaybackGraph.generated.h"

class UAvalanchePlayback;
class UAvaPlaybackGraphNode;

UCLASS()
class UAvaPlaybackGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	
	UAvalanchePlayback* GetPlayback() const;
	
	UAvaPlaybackGraphNode* CreatePlaybackGraphNode(TSubclassOf<UAvaPlaybackGraphNode> NewNodeClass
		, bool bSelectNewNode = true);
};
