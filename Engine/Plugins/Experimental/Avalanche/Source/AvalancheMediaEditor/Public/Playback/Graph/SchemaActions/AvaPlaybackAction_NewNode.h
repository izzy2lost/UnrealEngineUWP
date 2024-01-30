// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "Templates/SubclassOf.h"
#include "AvaPlaybackAction_NewNode.generated.h"

class UAvaPlaybackNode;

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaybackAction_NewNode : public FEdGraphSchemaAction
{
	
	GENERATED_BODY()

public:
	
	FAvaPlaybackAction_NewNode() = default;
	
	FAvaPlaybackAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{
	}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph
		, UEdGraphPin* FromPin
		, const FVector2D Location
		, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

	void SetPlaybackNodeClass(TSubclassOf<UAvaPlaybackNode> InPlaybackNodeClass);

protected:
	
	/** Connects new node to output of selected nodes */
	void ConnectToSelectedNodes(UAvaPlaybackNode* NewNode, UEdGraph* ParentGraph) const;

protected:
	
	/** Class of node we want to create */
	UPROPERTY()
	TSubclassOf<UAvaPlaybackNode> PlaybackNodeClass;
	
};
