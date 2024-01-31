// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackEditor.h"

#include "AppModes/AvaPlaybackDefaultMode.h"
#include "AvaMediaEditorStyle.h"
#include "Broadcast/AvaBroadcastEditor.h"
#include "EdGraphUtilities.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorActions.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAvaMediaEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Playback/AvaPlaybackCommands.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Graph/AvaPlaybackGraph.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_Root.h"
#include "Playback/Graph/SchemaActions/AvaPlaybackAction_NewComment.h"
#include "Playback/Nodes/AvaPlaybackNode.h"
#include "SNodePanel.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackEditor"

TMap<TSubclassOf<UAvaPlaybackNode>, TSubclassOf<UAvaPlaybackGraphNode>> FAvaPlaybackEditor::OverrideNodeClasses;

void FAvaPlaybackEditor::InitPlaybackEditor(const EToolkitMode::Type InMode
	, const TSharedPtr<IToolkitHost>& InitToolkitHost
	, UAvalanchePlayback* InPlayback)
{
	CacheOverrideNodeClasses();
	
	AvalanchePlayback = InPlayback;
	AvalanchePlayback->SetGraphEditor(SharedThis(this));
	AvalanchePlayback->CreateGraph();
	
	CreateDefaultCommands();
	CreateGraphCommands();
	
	const FName PlaybackEditorAppName(TEXT("AvalanchePlaybackEditorApp"));
	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	
	InitAssetEditor(InMode
		, InitToolkitHost
		, PlaybackEditorAppName
		, FTabManager::FLayout::NullLayout
		, bCreateDefaultStandaloneMenu
		, bCreateDefaultToolbar
		, InPlayback);
	
	RegisterApplicationModes();
	AvalanchePlayback->DryRunGraph();
}

FName FAvaPlaybackEditor::GetToolkitFName() const
{
	return TEXT("AvaPlaybackEditor");
}

FText FAvaPlaybackEditor::GetBaseToolkitName() const
{
	return LOCTEXT("PlaybackAppLabel", "Motion Design Playback Editor");
}

FString FAvaPlaybackEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("PlaybackScriptPrefix", "Script ").ToString();
}

FLinearColor FAvaPlaybackEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.3f, 0.5f);
}

UAvalanchePlayback* FAvaPlaybackEditor::GetPlaybackObject() const
{
	return AvalanchePlayback.Get();
}

void FAvaPlaybackEditor::ExtendToolBar(TSharedPtr<FExtender> Extender)
{
	Extender->AddToolBarExtension("Asset"
		, EExtensionHook::After
		, ToolkitCommands
		, FToolBarExtensionDelegate::CreateSP(this, &FAvaPlaybackEditor::FillPlayToolBar));
}

void FAvaPlaybackEditor::FillPlayToolBar(FToolBarBuilder& ToolBarBuilder)
{
	TWeakObjectPtr<UAvalanchePlayback> Playback = GetPlaybackObject();
	
	ToolBarBuilder.BeginSection(TEXT("Player"));
	{
		ToolBarBuilder.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([Playback]
				{
					if (Playback.IsValid())
					{
						Playback->Play();
					}
				}),
				FCanExecuteAction::CreateLambda([Playback]
				{
					return Playback.IsValid() && !Playback->IsPlaying();
				}))
			, NAME_None
			, LOCTEXT("Play_Label", "Play")
			, LOCTEXT("Play_ToolTip", "Play")
			, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Play")
		);

		ToolBarBuilder.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([Playback]
				{
					if (Playback.IsValid())
					{
						Playback->Stop(EAvaPlaybackStopOptions::ForceImmediate | EAvaPlaybackStopOptions::Unload);
					}
				}),
				FCanExecuteAction::CreateLambda([Playback]
				{
					return Playback.IsValid() && Playback->IsPlaying();
				}))
			, NAME_None
			, LOCTEXT("Stop_Label", "Stop")
			, LOCTEXT("Stop_ToolTip", "Stop")
			, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Stop")
		);
	}
	ToolBarBuilder.EndSection();
	
	ToolBarBuilder.BeginSection(TEXT("Broadcast"));
	{
		ToolBarBuilder.AddToolBarButton(FExecuteAction::CreateStatic(&FAvaBroadcastEditor::OpenBroadcastEditor)
		, NAME_None
		, LOCTEXT("Broadcast_Label", "Broadcast")
		, LOCTEXT("Broadcast_ToolTip", "Opens the Motion Design Broadcast Editor Window")
		, TAttribute<FSlateIcon>::Create([]() { return IAvaMediaEditorModule::Get().GetToolbarBroadcastButtonIcon(); }));
	}
	ToolBarBuilder.EndSection();
}

void FAvaPlaybackEditor::CacheOverrideNodeClasses()
{
	if (OverrideNodeClasses.IsEmpty())
	{
		for (UClass* Class : TObjectRange<UClass>())
		{
			if (Class->IsChildOf(UAvaPlaybackGraphNode::StaticClass()))
			{
				const UAvaPlaybackGraphNode* const GraphNode = Cast<UAvaPlaybackGraphNode>(Class->GetDefaultObject());
				if (TSubclassOf<UAvaPlaybackNode> PlaybackNodeClass = GraphNode->GetPlaybackNodeClass())
				{
					OverrideNodeClasses.Add(PlaybackNodeClass, Class);
				}
			}
		}
	}	
}

TSharedRef<SGraphEditor> FAvaPlaybackEditor::CreateGraphEditor()
{
	UAvalanchePlayback* const Playback = GetPlaybackObject();
	check(Playback);
	
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Playback", "Motion Design PLAYBACK");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged  = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FAvaPlaybackEditor::OnSelectedNodesChanged);
	InEvents.OnTextCommitted     = FOnNodeTextCommitted::CreateSP(this, &FAvaPlaybackEditor::OnNodeTitleCommitted);

	GraphEditor = SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(Playback->GetGraph())
		.GraphEvents(InEvents)
		.AutoExpandActionMenu(true)
		.ShowGraphStateOverlay(false);
	
	return GraphEditor.ToSharedRef();
}

UEdGraph* FAvaPlaybackEditor::CreatePlaybackGraph(UAvalanchePlayback* InPlayback)
{
	UAvaPlaybackGraph* PlaybackGraph = CastChecked<UAvaPlaybackGraph>(FBlueprintEditorUtils::CreateNewGraph(InPlayback
		, NAME_None
		, UAvaPlaybackGraph::StaticClass()
		, UAvaPlaybackGraphSchema::StaticClass()));
	
	return PlaybackGraph;
}

void FAvaPlaybackEditor::SetupPlaybackNode(UEdGraph* InGraph
	, UAvaPlaybackNode* InPlaybackNode
	, bool bSelectNewNode)
{
	TSubclassOf<UAvaPlaybackGraphNode> NodeClass = UAvaPlaybackGraphNode::StaticClass();
	
	//Find Most Relevant Override Node Class
	{
		UClass* StartingClass = InPlaybackNode->GetClass();
		while (StartingClass && StartingClass->IsChildOf(UAvaPlaybackNode::StaticClass()))
		{
			if (TSubclassOf<UAvaPlaybackGraphNode>* const OverrideClass = OverrideNodeClasses.Find(StartingClass))
			{
				NodeClass = *OverrideClass;
				break;
			}
			StartingClass = StartingClass->GetSuperClass();
		}

		if (!NodeClass)
		{
			NodeClass = UAvaPlaybackGraphNode::StaticClass();
		}
	}
	
	UAvaPlaybackGraph* const PlaybackGraph = Cast<UAvaPlaybackGraph>(InGraph);
	check(PlaybackGraph  && NodeClass);
	
	UAvaPlaybackGraphNode* const Node = PlaybackGraph->CreatePlaybackGraphNode(NodeClass, bSelectNewNode);
	check(Node);
	
	Node->SetPlaybackNode(InPlaybackNode);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	
	if (Node->Pins.Num() == 0)
	{
		Node->AllocateDefaultPins();
	}
}

void FAvaPlaybackEditor::CompilePlaybackNodesFromGraphNodes(UAvalanchePlayback* InPlayback)
{
	UEdGraph* const PlaybackGraph = InPlayback->GetGraph();
	check(PlaybackGraph);

	TArray<UAvaPlaybackNode*> PlaybackNodes;
	PlaybackNodes.Reserve(PlaybackGraph->Nodes.Num());
	
	for (UEdGraphNode* const EdGraphNode : PlaybackGraph->Nodes)
	{
		UAvaPlaybackGraphNode* const GraphNode = Cast<UAvaPlaybackGraphNode>(EdGraphNode);
		if (GraphNode && GraphNode->GetPlaybackNode())
		{
			UAvaPlaybackNode* const PlaybackNode = GraphNode->GetPlaybackNode();
			
			// Set ChildNodes of each PlaybackNode
			TArray<UEdGraphPin*> InputPins = GraphNode->GetInputPins();
			
			TArray<UAvaPlaybackNode*> ChildNodes;
			ChildNodes.Reserve(InputPins.Num());
			
			for (UEdGraphPin* const InputPin : InputPins)
			{
				if (InputPin->LinkedTo.Num() > 0)
				{
					//Note: A Playback Node Input Pin can't be connected to multiple sources.
					UAvaPlaybackGraphNode* GraphChildNode = CastChecked<UAvaPlaybackGraphNode>(InputPin->LinkedTo[0]->GetOwningNode());
					ChildNodes.Add(GraphChildNode->GetPlaybackNode());
				}
				else
				{
					ChildNodes.AddZeroed();
				}
			}

			PlaybackNode->SetFlags(RF_Transactional);
			PlaybackNode->Modify();
			PlaybackNode->SetChildNodes(MoveTemp(ChildNodes));
			
			PlaybackNodes.Emplace(PlaybackNode);
		}
	}	
	
	InPlayback->DryRunGraph();
	
	for (UAvaPlaybackNode* const PlaybackNode : PlaybackNodes)
	{
		PlaybackNode->PostEditChange();
	}
}

void FAvaPlaybackEditor::CreateInputPin(UEdGraphNode* InGraphNode)
{
	CastChecked<UAvaPlaybackGraphNode>(InGraphNode)->CreateInputPin();
}

void FAvaPlaybackEditor::RefreshNode(UEdGraphNode& InNode)
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->RefreshNode(InNode);
	}
}

bool FAvaPlaybackEditor::GetBoundsForSelectedNodes(FSlateRect& Rect, float Padding)
{
	if (GraphEditor.IsValid())
	{
		return GraphEditor->GetBoundsForSelectedNodes(Rect, Padding);
	}
	return false;
}

TSet<UObject*> FAvaPlaybackEditor::GetSelectedNodes() const
{
	if (GraphEditor.IsValid())
	{
		return GraphEditor->GetSelectedNodes();
	}
	return TSet<UObject*>();
}

void FAvaPlaybackEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<UObject*> Selection;
	if (NewSelection.Num())
	{
		for (TSet<UObject*>::TConstIterator Iter(NewSelection); Iter; ++Iter)
		{
			if (UAvaPlaybackGraphNode* const GraphNode = Cast<UAvaPlaybackGraphNode>(*Iter))
			{
				Selection.Add(GraphNode->GetPlaybackNode());
			}
			else
			{
				Selection.Add(*Iter);
			}
		}
	}
	OnPlaybackSelectionChanged.Broadcast(Selection);
}

void FAvaPlaybackEditor::OnNodeTitleCommitted(const FText& NewText
	, ETextCommit::Type CommitInfo
	, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameNode", "Rename Node"));
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

void FAvaPlaybackEditor::RegisterApplicationModes()
{
	TArray<TSharedRef<FApplicationMode>> ApplicationModes;
	TSharedPtr<FAvaPlaybackEditor> This = SharedThis(this);
	
	ApplicationModes.Add(MakeShared<FAvaPlaybackDefaultMode>(This));
	//Can add more App Modes here
	
	for (const TSharedRef<FApplicationMode>& AppMode : ApplicationModes)
	{
		AddApplicationMode(AppMode->GetModeName(), AppMode);
	}

	SetCurrentMode(FAvaPlaybackAppMode::DefaultMode);
}

void FAvaPlaybackEditor::CreateDefaultCommands()
{
}

void FAvaPlaybackEditor::CreateGraphCommands()
{
	GraphEditorCommands = MakeShared<FUICommandList>();
	{
		const FGraphEditorCommandsImpl& GraphCommands = FGraphEditorCommands::Get();
		const FGenericCommands& GenericCommands = FGenericCommands::Get();
		const FAvaPlaybackCommands& PlaybackCommands = FAvaPlaybackCommands::Get();
			
		// Playback Commands
		GraphEditorCommands->MapAction(PlaybackCommands.AddInputPin,
			FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::AddInputPin),
			FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanAddInputPin));

		GraphEditorCommands->MapAction(PlaybackCommands.RemoveInputPin,
			FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::RemoveInputPin),
			FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanRemoveInputPin));
		
		// Graph Editor Commands
		GraphEditorCommands->MapAction(GraphCommands.CreateComment
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CreateComment));

		// Editing commands
		GraphEditorCommands->MapAction(GenericCommands.SelectAll
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::SelectAllNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanSelectAllNodes)
		);

		GraphEditorCommands->MapAction(GenericCommands.Delete
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::DeleteSelectedNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanDeleteSelectedNodes)
		);
		
		GraphEditorCommands->MapAction(GenericCommands.Copy
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CopySelectedNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanCopySelectedNodes)
		);

		GraphEditorCommands->MapAction(GenericCommands.Cut
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CutSelectedNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanCutSelectedNodes)
		);

		GraphEditorCommands->MapAction(GenericCommands.Paste
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::PasteNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanPasteNodes)
		);

		GraphEditorCommands->MapAction(GenericCommands.Duplicate
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::DuplicateSelectedNodes)
			, FCanExecuteAction::CreateSP(this, &FAvaPlaybackEditor::CanDuplicateSelectedNodes)
		);

		// Alignment Commands
		GraphEditorCommands->MapAction(GraphCommands.AlignNodesTop
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignTop)
		);

		GraphEditorCommands->MapAction(GraphCommands.AlignNodesMiddle
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignMiddle)
		);

		GraphEditorCommands->MapAction(GraphCommands.AlignNodesBottom
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignBottom)
		);

		GraphEditorCommands->MapAction(GraphCommands.AlignNodesLeft
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignLeft)
		);

		GraphEditorCommands->MapAction(GraphCommands.AlignNodesCenter
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignCenter)
		);

		GraphEditorCommands->MapAction(GraphCommands.AlignNodesRight
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnAlignRight)
		);

		GraphEditorCommands->MapAction(GraphCommands.StraightenConnections
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnStraightenConnections)
		);

		// Distribution Commands
		GraphEditorCommands->MapAction(GraphCommands.DistributeNodesHorizontally
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnDistributeNodesH)
		);

		GraphEditorCommands->MapAction(GraphCommands.DistributeNodesVertically
			, FExecuteAction::CreateSP(this, &FAvaPlaybackEditor::OnDistributeNodesV)
		);
	}
}

bool FAvaPlaybackEditor::CanAddInputPin() const
{
	return GetSelectedNodes().Num() == 1;
}

void FAvaPlaybackEditor::AddInputPin()
{
	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	// Iterator used but should only contain one node
	for (TSet<UObject*>::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UAvaPlaybackGraphNode* SelectedNode = Cast<UAvaPlaybackGraphNode>(*It))
		{
			SelectedNode->AddInputPin();
			break;
		}
	}
}

bool FAvaPlaybackEditor::CanRemoveInputPin() const
{
	return true;
}

void FAvaPlaybackEditor::RemoveInputPin()
{
	if (GraphEditor.IsValid())
	{
		if (UEdGraphPin* const SelectedPin = GraphEditor->GetGraphPinForMenu())
		{
			UAvaPlaybackGraphNode* const SelectedNode = Cast<UAvaPlaybackGraphNode>(SelectedPin->GetOwningNode());
			if (SelectedNode && SelectedNode == SelectedPin->GetOwningNode())
			{
				SelectedNode->RemoveInputPin(SelectedPin);
			}
		}
	}
}

void FAvaPlaybackEditor::CreateComment()
{
	if (GraphEditor.IsValid())
	{
		FAvaPlaybackAction_NewComment CommentAction;
		CommentAction.PerformAction(GraphEditor->GetCurrentGraph(), nullptr, GraphEditor->GetPasteLocation());
	}
}

void FAvaPlaybackEditor::SelectAllNodes()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->SelectAllNodes();
	}
}

bool FAvaPlaybackEditor::CanDeleteSelectedNodes() const
{
	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (TSet<UObject*>::TConstIterator It(SelectedNodes); It; ++It)
		{
			if (Cast<UAvaPlaybackGraphNode_Root>(*It))
			{
				// Return false if only root node is selected, as it can't be deleted
				return false;
			}
		}
	}
	return SelectedNodes.Num() > 0;
}

void FAvaPlaybackEditor::DeleteSelectedNodes()
{
	if (!GraphEditor.IsValid() || !GraphEditor->GetCurrentGraph())
	{
		return;
	}
	
	const FScopedTransaction Transaction(LOCTEXT("DeleteSelectedNodes", "Delete Selected Nodes"));
	GraphEditor->GetCurrentGraph()->Modify();

	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	GraphEditor->ClearSelectionSet();

	check(AvalanchePlayback.IsValid());
	for (TSet<UObject*>::TConstIterator It(SelectedNodes); It; ++It)
	{
		UEdGraphNode* const Node = CastChecked<UEdGraphNode>(*It);
		if (Node->CanUserDeleteNode())
		{
			if (UAvaPlaybackGraphNode* const PlaybackGraphNode = Cast<UAvaPlaybackGraphNode>(Node))
			{
				UAvaPlaybackNode* const NodeToDelete = PlaybackGraphNode->GetPlaybackNode();
				FBlueprintEditorUtils::RemoveNode(nullptr, PlaybackGraphNode, true);

				// Make sure Playback is updated to match graph
				AvalanchePlayback->CompilePlaybackNodesFromGraphNodes();

				// Remove this node from the AvalanchePlayback's list of all Playback Nodes
				AvalanchePlayback->RemovePlaybackNode(NodeToDelete);
				AvalanchePlayback->MarkPackageDirty();
			}
			else
			{
				FBlueprintEditorUtils::RemoveNode(nullptr, Node, true);
			}
		}
	}
}

void FAvaPlaybackEditor::DeleteSelectedDuplicatableNodes()
{
	if (!GraphEditor.IsValid())
	{
		return;
	}
	
	// Cache off the old selection
	const TSet<UObject*> OldSelectedNodes = GetSelectedNodes();

	// Clear the selection and only select the nodes that can be duplicated
	TSet<UObject*> RemainingNodes;
	GraphEditor->ClearSelectionSet();

	for (TSet<UObject*>::TConstIterator Iter(OldSelectedNodes); Iter; ++Iter)
	{
		UEdGraphNode* const Node = Cast<UEdGraphNode>(*Iter);
		if (Node && Node->CanDuplicateNode())
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
		else
		{
			RemainingNodes.Add(Node);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	// Reselect whatever's left from the original selection after the deletion
	GraphEditor->ClearSelectionSet();

	for (TSet<UObject*>::TConstIterator Iter(RemainingNodes); Iter; ++Iter)
	{
		if (UEdGraphNode* const Node = Cast<UEdGraphNode>(*Iter))
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
}

bool FAvaPlaybackEditor::CanCopySelectedNodes() const
{
	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	
	for (TSet<UObject*>::TConstIterator Iter(SelectedNodes); Iter; ++Iter)
	{
		UEdGraphNode* const Node = Cast<UEdGraphNode>(*Iter);
		
		//If at least one node can be duplicated, we should allow copy
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	
	return false;
}

void FAvaPlaybackEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	
	for (TSet<UObject*>::TConstIterator Iter(SelectedNodes); Iter; ++Iter)
	{
		if (UAvaPlaybackGraphNode* const Node = Cast<UAvaPlaybackGraphNode>(*Iter))
		{
			Node->PrepareForCopying();
		}
	}
	
	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	
	for (TSet<UObject*>::TConstIterator Iter(SelectedNodes); Iter; ++Iter)
	{
		if (UAvaPlaybackGraphNode* const Node = Cast<UAvaPlaybackGraphNode>(*Iter))
		{
			Node->PostCopyNode();
		}
	}
}

bool FAvaPlaybackEditor::CanCutSelectedNodes() const
{
	return CanCopySelectedNodes() && CanDeleteSelectedNodes();
}

void FAvaPlaybackEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	// Cut should only delete nodes that can be duplicated
	DeleteSelectedDuplicatableNodes();
}

bool FAvaPlaybackEditor::CanPasteNodes() const
{
	if (AvalanchePlayback.IsValid() && AvalanchePlayback->GetGraph())
	{
		FString ClipboardContent;
		FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
		return FEdGraphUtilities::CanImportNodesFromText(AvalanchePlayback->GetGraph(), ClipboardContent);
	}
	return false;
}

void FAvaPlaybackEditor::PasteNodes()
{
	if (GraphEditor.IsValid())
	{
		PasteNodesHere(GraphEditor->GetPasteLocation());
	}
}

void FAvaPlaybackEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!GraphEditor.IsValid() || !AvalanchePlayback.IsValid() || !AvalanchePlayback->GetGraph())
	{
		return;
	}
	
	// Undo/Redo support
	const FScopedTransaction Transaction(LOCTEXT("PasteNodes", "Paste Playback Nodes"));
	AvalanchePlayback->GetGraph()->Modify();
	AvalanchePlayback->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(AvalanchePlayback->GetGraph(), TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AverageNodePosition(0.0f,0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		AverageNodePosition.X += Node->NodePosX;
		AverageNodePosition.Y += Node->NodePosY;
	}

	if (PastedNodes.Num() > 0)
	{
		FVector2D::FReal InverseNumNodes = 1.0 / static_cast<FVector2D::FReal>(PastedNodes.Num());
		AverageNodePosition.X *= InverseNumNodes;
		AverageNodePosition.Y *= InverseNumNodes;
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* const Node = *It;
		if (UAvaPlaybackGraphNode* PlaybackGraphNode = Cast<UAvaPlaybackGraphNode>(Node))
		{
			AvalanchePlayback->AddPlaybackNode(PlaybackGraphNode->GetPlaybackNode());
		}

		// Select the newly pasted stuff
		GraphEditor->SetNodeSelection(Node, true);

		Node->NodePosX = (Node->NodePosX - AverageNodePosition.X) + Location.X ;
		Node->NodePosY = (Node->NodePosY - AverageNodePosition.Y) + Location.Y ;

		Node->SnapToGrid(SNodePanel::GetSnapGridSize());

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	// Force new pasted nodes to have same connections as graph nodes
	AvalanchePlayback->CompilePlaybackNodesFromGraphNodes();

	// Update UI
	GraphEditor->NotifyGraphChanged();

	AvalanchePlayback->PostEditChange();
	AvalanchePlayback->MarkPackageDirty();
}

bool FAvaPlaybackEditor::CanDuplicateSelectedNodes() const
{
	return CanCopySelectedNodes();
}

void FAvaPlaybackEditor::DuplicateSelectedNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

void FAvaPlaybackEditor::OnAlignTop()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignTop();
	}
}

void FAvaPlaybackEditor::OnAlignMiddle()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignMiddle();
	}
}

void FAvaPlaybackEditor::OnAlignBottom()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignBottom();
	}
}

void FAvaPlaybackEditor::OnAlignLeft()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignLeft();
	}
}

void FAvaPlaybackEditor::OnAlignCenter()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignCenter();
	}
}

void FAvaPlaybackEditor::OnAlignRight()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnAlignRight();
	}
}

void FAvaPlaybackEditor::OnStraightenConnections()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnStraightenConnections();
	}
}

void FAvaPlaybackEditor::OnDistributeNodesH()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnDistributeNodesH();
	}
}

void FAvaPlaybackEditor::OnDistributeNodesV()
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->OnDistributeNodesV();
	}
}

#undef LOCTEXT_NAMESPACE
