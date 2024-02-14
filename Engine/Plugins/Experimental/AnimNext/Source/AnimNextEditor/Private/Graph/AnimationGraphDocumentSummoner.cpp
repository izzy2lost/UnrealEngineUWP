// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationGraphDocumentSummoner.h"
#include "Common/SActionMenu.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

namespace UE::AnimNext::Editor
{

FAnimationGraphDocumentSummoner::FAnimationGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FGraphDocumentSummoner(InIdentifier, InHostingApp)
{
}

FActionMenuContent FAnimationGraphDocumentSummoner::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu, InGraph)
		.AutoExpandActionMenu(bAutoExpand)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed);

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

bool FAnimationGraphDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	return Object && Object->IsA<UAnimNextGraph_EdGraph>();
}

}
