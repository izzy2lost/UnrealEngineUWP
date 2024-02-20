// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphDocument.h"

#include "Framework/Commands/GenericCommands.h"
#include "WorkspaceEditor.h"
#include "EdGraph/EdGraph.h"

namespace UE::Workspace
{

void SGraphDocument::Construct(const FArguments& InArgs, TSharedRef<FWorkspaceEditor> InHostingApp, UEdGraph* InGraph)
{
	HostingAppPtr = InHostingApp;
	EdGraph = InGraph;

	BindCommands();

	OnDeleteSelectedNodes = InArgs._OnDeleteSelectedNodes;

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateLambda([this, OnCreateActionMenu = InArgs._OnCreateActionMenu](UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bInAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
	{
		if(OnCreateActionMenu.IsBound())
		{
			return OnCreateActionMenu.Execute(FWorkspaceEditorContext(HostingAppPtr.Pin().ToSharedRef(), EdGraph), InGraph, InNodePosition, InDraggedPins, bInAutoExpand, InOnMenuClosed);
		}
		return FActionMenuContent();
	});
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateLambda([this, OnGraphSelectionChanged = InArgs._OnGraphSelectionChanged](const TSet<UObject*>& NewSelection)
	{
		OnGraphSelectionChanged.ExecuteIfBound(FWorkspaceEditorContext(HostingAppPtr.Pin().ToSharedRef(), EdGraph), NewSelection);
	});
	Events.OnTextCommitted = ::FOnNodeTextCommitted::CreateLambda([this, OnNodeTextCommitted = InArgs._OnNodeTextCommitted](const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
	{
		OnNodeTextCommitted.ExecuteIfBound(FWorkspaceEditorContext(HostingAppPtr.Pin().ToSharedRef(), EdGraph), NewText, CommitInfo, NodeBeingChanged);
	});

	ChildSlot
	[
		SAssignNew(GraphEditor, SGraphEditor)
		.AdditionalCommands(CommandList)
		.IsEditable(this, &SGraphDocument::IsEditable, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(Events)
		.AssetEditorToolkit(HostingAppPtr)
	];
}

void SGraphDocument::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SGraphDocument::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SGraphDocument::CanDeleteSelectedNodes));
}

void SGraphDocument::DeleteSelectedNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	OnDeleteSelectedNodes.ExecuteIfBound(FWorkspaceEditorContext(HostingAppPtr.Pin().ToSharedRef(), EdGraph), SelectedNodes);
}

bool SGraphDocument::CanDeleteSelectedNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	bool bCanUserDeleteNode = false;

	if(IsEditable(EdGraph) && SelectedNodes.Num() > 0)
	{
		for(UObject* NodeObject : SelectedNodes)
		{
			// If any nodes allow deleting, then do not disable the delete option
			UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject);
			if(Node && Node->CanUserDeleteNode())
			{
				bCanUserDeleteNode = true;
				break;
			}
		}
	}

	return bCanUserDeleteNode;
}

bool SGraphDocument::IsEditable(UEdGraph* InGraph) const
{
	return InGraph && HostingAppPtr.Pin()->InEditingMode() && InGraph->bEditable;
}

};