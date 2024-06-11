// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextModuleEditor.h"
#include "ModuleEditorMode.h"
#include "Module/AnimNextModule.h"
#include "AnimNextEdGraphNode.h"
#include "Module/AnimNextModule_EditorData.h"
#include "EdGraphNode_Comment.h"
#include "Common/SActionMenu.h"
#include "UncookedOnlyUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "RigVMModel/RigVMController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AnimNextGraphEditor"

namespace UE::AnimNext::Editor
{

namespace GraphModes
{
	const FName GraphEditor("AnimNextGraphEditorMode");
}

namespace GraphTabs
{
	const FName Details("DetailsTab");
	const FName Document("Document");
}

const FName GraphAppIdentifier("AnimNextGraphEditor");

void FModuleEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextModule* InAnimNextModule)
{
	AnimNextModule = InAnimNextModule;

	AnimNextModule_EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData(AnimNextModule);
	AnimNextModule_EditorData->Initialize(false);
	
	DocumentManager = MakeShared<FDocumentTracker>();
	DocumentManager->Initialize(SharedThis(this));

	TSharedRef<FGraphEditorSummoner> GraphEditorSummoner = MakeShared<FGraphEditorSummoner>(SharedThis(this));
	GraphEditorSummoner->OnCreateGraphEditorWidget().BindSP(this, &FModuleEditor::CreateGraphEditorWidget);
	GraphEditorSummoner->OnGraphEditorFocused().BindSP(this, &FModuleEditor::OnGraphEditorFocused);
	GraphEditorSummoner->OnGraphEditorBackgrounded().BindSP(this, &FModuleEditor::OnGraphEditorBackgrounded);
	GraphEditorTabFactoryPtr = GraphEditorSummoner;
	DocumentManager->RegisterDocumentFactory(GraphEditorSummoner);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(InMode, InInitToolkitHost, GraphAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InAnimNextModule);

	BindCommands();

	AddApplicationMode(GraphModes::GraphEditor, MakeShared<FModuleEditorMode>(SharedThis(this)));
	SetCurrentMode(GraphModes::GraphEditor);
	
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FModuleEditor::BindCommands()
{
}

void FModuleEditor::ExtendMenu()
{
	
}

void FModuleEditor::ExtendToolbar()
{
	
}

void FModuleEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FModuleEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	
}

FName FModuleEditor::GetToolkitFName() const
{
	return FName("AnimNextGraphEditor");
}

FText FModuleEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "AnimNextGraphEditor");
}

FString FModuleEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimNextGraphEditor ").ToString();
}

FLinearColor FModuleEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FModuleEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FModuleEditor::OnGraphEditorFocused(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = InGraphEditor;
}

void FModuleEditor::OnGraphEditorBackgrounded(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = nullptr;
}

UEdGraph* FModuleEditor::GetFocusedGraph() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if (UEdGraph* Graph = FocusedGraphEdPtr.Pin()->GetCurrentGraph())
		{
			return Graph;
		}
	}
	return nullptr;
}

URigVMGraph* FModuleEditor::GetFocusedVMGraph() const
{
	UAnimNextEdGraph* EdGraph = Cast<UAnimNextEdGraph>(GetFocusedGraph());
	return AnimNextModule_EditorData->GetRigVMGraphForEditorObject(EdGraph);
}

URigVMController* FModuleEditor::GetFocusedVMController() const
{
	return AnimNextModule_EditorData->GetRigVMClient()->GetController(GetFocusedVMGraph());
}

TSharedRef<SGraphEditor> FModuleEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	if(!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShared<FUICommandList>();
	
		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(this, &FModuleEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateSP(this, &FModuleEditor::CanDeleteSelectedNodes));
	}
	
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FModuleEditor::OnCreateGraphActionMenu);
	
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FModuleEditor::IsEditable, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(Events)
		.AssetEditorToolkit(AsShared());
}

FActionMenuContent FModuleEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu, InGraph)
		.AutoExpandActionMenu(bAutoExpand)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed);

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

void FModuleEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged))
	{
		GetFocusedVMController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
	}
}

void FModuleEditor::CloseDocumentTab(const UObject* DocumentID)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FModuleEditor::InEditingMode() const
{
	// @TODO: disallow editing when debugging when implemented

	return true;
}

bool FModuleEditor::IsEditable(UEdGraph* InGraph) const
{
	return InGraph && InEditingMode() && InGraph->bEditable;
}

FGraphPanelSelectionSet FModuleEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FModuleEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	FocusedGraphEd->GetCurrentGraph()->Modify();
	
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

//	SetUISelectionState(NAME_None);

	if(FocusedGraphEd)
	{
		FocusedGraphEd->ClearSelectionSet();
	}

	TMap<UAnimNextEdGraphNode*, URigVMNode*> NodesToRemove;

	// Some nodes have sub-objects that are represented as other tabs.
	// Close them here as a pre-pass before we remove their nodes. If the documents are left open they
	// may reference dangling data and function incorrectly in cases such as FindBlueprintforNodeChecked
	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				auto CloseAllDocumentsTab = [this](const UEdGraphNode* InNode)
				{
					TArray<UObject*> NodesToClose;
					GetObjectsWithOuter(InNode, NodesToClose);
					for (UObject* Node : NodesToClose)
					{
						UEdGraph* NodeGraph = Cast<UEdGraph>(Node);
						if (NodeGraph)
						{
							CloseDocumentTab(NodeGraph);
						}
					}
				};
				
				if (Node->GetSubGraphs().Num() > 0)
				{
					CloseAllDocumentsTab(Node);
				}

				if (UAnimNextEdGraphNode* AnimNextNode = Cast<UAnimNextEdGraphNode>(Node))
				{
					if (URigVMNode* ModelNode = GetFocusedVMController()->GetGraph()->FindNodeByName(*AnimNextNode->GetModelNodePath()))
					{
						NodesToRemove.Add(AnimNextNode, ModelNode);
					}
				}
			}
		}
	}

	if (NodesToRemove.IsEmpty())
	{
		return;
	}

	GetFocusedVMController()->OpenUndoBracket(TEXT("Delete selected nodes"));

	TArray<URigVMNode*> ModelNodesToRemove;
	for (auto& It : NodesToRemove)
	{
		URigVMNode* ModelNode = It.Value;

		ModelNodesToRemove.Add(ModelNode);
	}

	GetFocusedVMController()->RemoveNodes(ModelNodesToRemove, true);
	GetFocusedVMController()->CloseUndoBracket();
}

bool FModuleEditor::CanDeleteSelectedNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bCanUserDeleteNode = false;

	if(IsEditable(GetFocusedGraph()) && SelectedNodes.Num() > 0)
	{
		for(UObject* NodeObject : SelectedNodes)
		{
			// If any nodes allow deleting, then do not disable the delete option
			UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject);
			if(Node->CanUserDeleteNode())
			{
				bCanUserDeleteNode = true;
				break;
			}
		}
	}

	return bCanUserDeleteNode;
}

}

#undef LOCTEXT_NAMESPACE