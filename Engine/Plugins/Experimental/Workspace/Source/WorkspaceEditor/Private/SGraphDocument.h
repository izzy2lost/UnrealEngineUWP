// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GraphEditor.h"
#include "IWorkspaceEditorModule.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::Workspace
{
	class FWorkspaceEditor;
	class FWorkspaceEditorModule;
}

namespace UE::Workspace
{

// Wrapper widget for a graph editor
class SGraphDocument : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SGraphDocument) {}

	SLATE_EVENT(FOnCreateActionMenu, OnCreateActionMenu)

	SLATE_EVENT(FOnNodeTextCommitted, OnNodeTextCommitted)

	SLATE_EVENT(FOnDeleteSelectedNodes, OnDeleteSelectedNodes)

	SLATE_EVENT(FOnGraphSelectionChanged, OnGraphSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FWorkspaceEditor> InHostingApp, UEdGraph* InGraph);
	
	void BindCommands();
	
	void DeleteSelectedNodes();

	bool CanDeleteSelectedNodes() const;

	bool IsEditable(UEdGraph* InGraph) const;

	// The graph we are editing
	UEdGraph* EdGraph = nullptr;

	// The graph editor we wrap
	TSharedPtr<SGraphEditor> GraphEditor;

	// Command list for graphs
	TSharedPtr<FUICommandList> CommandList;

	// The hosting app
	TWeakPtr<FWorkspaceEditor> HostingAppPtr;

	// Delegate called when we delete nodes
	FOnDeleteSelectedNodes OnDeleteSelectedNodes;

	friend class FWorkspaceEditorModule;
};

}