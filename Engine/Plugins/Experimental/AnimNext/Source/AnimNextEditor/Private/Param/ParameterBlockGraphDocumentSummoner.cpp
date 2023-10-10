// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterBlockGraphDocumentSummoner.h"
#include "SParametersActionMenu.h"
#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

namespace UE::AnimNext::Editor
{

FParameterBlockGraphDocumentSummoner::FParameterBlockGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FGraphDocumentSummoner(InIdentifier, InHostingApp)
{
}

FActionMenuContent FParameterBlockGraphDocumentSummoner::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const
{
	TSharedRef<SParametersActionMenu> ActionMenu = SNew(SParametersActionMenu)
		.AutoExpandActionMenu(bAutoExpand)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed);

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

bool FParameterBlockGraphDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	return Object && Object->IsA<UAnimNextParameterBlock_EdGraph>();
}

}
