// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEditorModule.h"

#include "AnimNextConfig.h"
#include "EdGraphNode_Comment.h"
#include "ISettingsModule.h"
#include "IWorkspaceEditor.h"
#include "ScopedTransaction.h"
#include "SSimpleButton.h"
#include "SSimpleComboButton.h"
#include "UncookedOnlyUtils.h"
#include "Common/SRigVMAssetView.h"
#include "Framework/Application/SlateApplication.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Graph/AnimNextGraph_EdGraphNodeCustomization.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Param/ParameterCustomization.h"
#include "Param/ParameterPickerArgs.h"
#include "Param/ParametersGraphPanelPinFactory.h"
#include "Param/ParamNamePropertyCustomization.h"
#include "Param/ParamPropertyCustomization.h"
#include "Param/ParamTypePropertyCustomization.h"
#include "Param/SParameterPicker.h"
#include "Scheduler/AnimNextSchedule.h"
#include "IWorkspaceEditorModule.h"
#include "Common/SActionMenu.h"
#include "AnimNextRigVMAssetEntry.h"

#define LOCTEXT_NAMESPACE "AnimNextEditorModule"

namespace UE::AnimNext::Editor
{

class FModule : public IModule
{
	virtual void StartupModule() override
	{
		// Register settings for user editing
		ISettingsModule& SettingsModule = FModuleManager::Get().LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Editor", "General", "AnimNext",
			LOCTEXT("SettingsName", "AnimNext"),
			LOCTEXT("SettingsDescription", "Customize AnimNext Settings."),
			GetMutableDefault<UAnimNextConfig>()
		);

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParamType",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamTypePropertyTypeCustomization>(); }));

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParam",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamPropertyTypeCustomization>(); }));

		Identifier = MakeShared<FParamNamePropertyTypeIdentifier>();
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FNameProperty::StaticClass()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamNamePropertyTypeCustomization>(); }),
			Identifier);

		PropertyModule.RegisterCustomClassLayout("AnimNextGraph_Parameter", 
			FOnGetDetailCustomizationInstance::CreateLambda([] { return MakeShared<FParameterCustomization>(); }));

		PropertyModule.RegisterCustomClassLayout("AnimNextGraph_EdGraphNode",
			FOnGetDetailCustomizationInstance::CreateLambda([] { return MakeShared<FAnimNextGraph_EdGraphNodeCustomization>(); }));

		AnimNextGraphPanelNodeFactory = MakeShared<FAnimNextGraphPanelNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		ParametersGraphPanelPinFactory = MakeShared<FParametersGraphPanelPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(ParametersGraphPanelPinFactory);

		RegisterWorkspaceDocumentTypes();

		SRigVMAssetView::RegisterCategoryFactory("Parameters", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextGraph_EditorData* EditorData = CastChecked<UAnimNextGraph_EditorData>(InEditorData);
			return SNew(SSimpleComboButton)
				.Text(LOCTEXT("AddParameterButton", "Add Parameter"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.HasDownArrow(true)
				.OnGetMenuContent_Lambda([EditorData]()
				{
					FAssetData AssetData(UncookedOnly::FUtils::GetAsset(EditorData));
					
					FParameterPickerArgs Args;
					Args.bMultiSelect = false;
					Args.bShowSourceGraph = false;
					Args.bShowBoundParameters = false;
					Args.bShowBuiltInParameters = false; // Built-In parameters disabled for MVP
					Args.OnFilterParameter = FOnFilterParameter::CreateLambda([EditorData, AssetData](const FParameterBindingReference& InParameterBinding)
					{
						// Skip params that are already bound in this graph
						if(InParameterBinding.Graph == AssetData)
						{
							return EFilterParameterResult::Exclude;
						}
						
						return EFilterParameterResult::Include;
					});

					Args.OnAddParameter = FOnAddParameter::CreateLambda([EditorData](const FParameterToAdd& ParameterToAdd)
					{
						FSlateApplication::Get().DismissAllMenus();

						check(EditorData->FindEntry(ParameterToAdd.Name) == nullptr);
						FScopedTransaction Transaction(LOCTEXT("AddParameter", "Add parameter"));
						EditorData->AddParameter(ParameterToAdd.Name, ParameterToAdd.Type);
					});
					Args.OnParameterPicked = FOnParameterPicked::CreateLambda([EditorData](const FParameterBindingReference& InParameterBinding)
					{
						FSlateApplication::Get().DismissAllMenus();

						if (EditorData->FindEntry(InParameterBinding.Parameter) == nullptr)
						{
							const FAnimNextParamType Type = UncookedOnly::FUtils::GetParameterTypeFromName(InParameterBinding.Parameter);
							if (Type.IsValid())
							{
								EditorData->AddParameter(InParameterBinding.Parameter, Type);
							};
						}
					});
					
					return SNew(SParameterPicker)
						.Args(Args);
				});
		});

		SRigVMAssetView::RegisterCategoryFactory("Event Graphs", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextGraph_EditorData* EditorData = CastChecked<UAnimNextGraph_EditorData>(InEditorData);
			return SNew(SSimpleButton)
				.Text(LOCTEXT("AddEventGraphButton", "Add Event Graph"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([EditorData]()
				{
					FScopedTransaction Transaction(LOCTEXT("AddEventGraph", "Add Event Graph"));

					// Create a new entry for the graph
					EditorData->AddEventGraph(TEXT("NewGraph"));

					return FReply::Handled();
				});
		});

		SRigVMAssetView::RegisterCategoryFactory("Animation Graphs", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextGraph_EditorData* EditorData = CastChecked<UAnimNextGraph_EditorData>(InEditorData);
			return SNew(SSimpleButton)
				.Text(LOCTEXT("AddGraphButton", "Add Animation Graph"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([EditorData]()
				{
					FScopedTransaction Transaction(LOCTEXT("AddAnimationGraph", "Add Animation Graph"));

					// Create a new entry for the graph
					EditorData->AddAnimationGraph(TEXT("NewGraph"));

					return FReply::Handled();
				});
		});
	}

	virtual void ShutdownModule() override
	{
		if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout("AnimNextParamType");
			PropertyModule.UnregisterCustomPropertyTypeLayout("AnimNextParam");
			PropertyModule.UnregisterCustomPropertyTypeLayout("NameProperty");
			PropertyModule.UnregisterCustomClassLayout("AnimNextGraph_Parameter");
			PropertyModule.UnregisterCustomClassLayout("AnimNextGraph_EdGraphNode");
		}

		FEdGraphUtilities::UnregisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		FEdGraphUtilities::UnregisterVisualPinFactory(ParametersGraphPanelPinFactory);

		UnregisterWorkspaceDocumentTypes();

		SRigVMAssetView::UnregisterCategoryFactory("Parameters");
		SRigVMAssetView::UnregisterCategoryFactory("Parameter Graphs");
	}

	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) override
	{
		return SNew(SParameterPicker)
			.Args(InArgs);
	}

	void RegisterWorkspaceDocumentTypes()
	{
		Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<Workspace::IWorkspaceEditorModule>("WorkspaceEditor");
		WorkspaceEditorModule.RegisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNext.AnimNextSchedule")),
			Workspace::FObjectDocumentArgs(
				Workspace::FOnMakeDocumentWidget::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext)
				{
					UAnimNextSchedule* Schedule = CastChecked<UAnimNextSchedule>(InContext.Object);
					FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
					FDetailsViewArgs DetailsViewArgs;
					DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
					TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
					DetailsView->SetObject(Schedule);
					return DetailsView;
				}),
				Workspace::WorkspaceTabs::MiddleDocumentArea));

		WorkspaceEditorModule.RegisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNext.AnimNextGraph")),
			Workspace::FObjectDocumentArgs(
				Workspace::FOnMakeDocumentWidget::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext)
				{
					UAnimNextGraph* Graph = CastChecked<UAnimNextGraph>(InContext.Object);
					UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Graph);

					TWeakPtr<Workspace::IWorkspaceEditor> WeakWorkspaceEditor = InContext.WorkspaceEditor;

					EditorData->RigVMGraphModifiedEvent.RemoveAll(&InContext.WorkspaceEditor.Get());
					EditorData->RigVMGraphModifiedEvent.AddSPLambda(&InContext.WorkspaceEditor.Get(), [WeakWorkspaceEditor](ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject)
					{
						if(TSharedPtr<Workspace::IWorkspaceEditor> WorkspaceEditor = WeakWorkspaceEditor.Pin())
						{
							if (InType == ERigVMGraphNotifType::InteractionBracketClosed)
							{
								WorkspaceEditor->RefreshDetails();
							}
						}
					});

					return SNew(SRigVMAssetView, EditorData)
						.OnSelectionChanged_Lambda([WeakWorkspaceEditor](const TArray<UObject*>& InEntries)
						{
							if(TSharedPtr<Workspace::IWorkspaceEditor> WorkspaceEditor = WeakWorkspaceEditor.Pin())
							{
								WorkspaceEditor->SetDetailsObjects(InEntries);
							}
						})
						.OnOpenGraph_Lambda([WeakWorkspaceEditor](URigVMGraph* InGraph)
						{
							if(TSharedPtr<Workspace::IWorkspaceEditor> WorkspaceEditor = WeakWorkspaceEditor.Pin())
							{
								if(IRigVMClientHost* RigVMClientHost = InGraph->GetImplementingOuter<IRigVMClientHost>())
								{
									if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(InGraph))
									{
										WorkspaceEditor->OpenObjects({EditorObject});
									}
								}
							}
						})
						.OnDeleteEntries_Lambda([WeakWorkspaceEditor](const TArray<UAnimNextRigVMAssetEntry*>& InEntries)
						{
							if(TSharedPtr<Workspace::IWorkspaceEditor> WorkspaceEditor = WeakWorkspaceEditor.Pin())
							{
								if(InEntries.Num() > 0)
								{
									TArray<UObject*> EdGraphsToClose;
									EdGraphsToClose.Reserve(InEntries.Num());
									for(UAnimNextRigVMAssetEntry* Entry : InEntries)
									{
										if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
										{
											if(URigVMEdGraph* EdGraph = GraphInterface->GetEdGraph())
											{
												EdGraphsToClose.Add(EdGraph);
											}
										}
									}

									WorkspaceEditor->CloseObjects(EdGraphsToClose);
								}
							}
						});
				}),
				Workspace::WorkspaceTabs::LeftDocumentArea));

		Workspace::FGraphDocumentWidgetArgs GraphArgs;
		GraphArgs.SpawnLocation = Workspace::WorkspaceTabs::MiddleDocumentArea;
		GraphArgs.OnCreateActionMenu = Workspace::FOnCreateActionMenu::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext, UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
		{
			TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu, InGraph)
				.AutoExpandActionMenu(bAutoExpand)
				.NewNodePosition(InNodePosition)
				.DraggedFromPins(InDraggedPins)
				.OnClosedCallback(InOnMenuClosed);

			TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
			return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
		});
		GraphArgs.OnNodeTextCommitted = Workspace::FOnNodeTextCommitted::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext, const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
		{
			URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(NodeBeingChanged->GetGraph());
			if (RigVMEdGraph == nullptr)
			{
				return;
			}

			UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged);
			if (CommentBeingChanged == nullptr)
			{
				return;
			}

			RigVMEdGraph->GetController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
		});
		GraphArgs.OnDeleteSelectedNodes = Workspace::FOnDeleteSelectedNodes::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext, const FGraphPanelSelectionSet& InSelectedNodes)
		{
			if(InSelectedNodes.IsEmpty())
			{
				return;
			}

			URigVMController* Controller = nullptr;
			
			bool bRelinkPins = false;
			TArray<URigVMNode*> NodesToRemove;

			for (FGraphPanelSelectionSet::TConstIterator NodeIt(InSelectedNodes); NodeIt; ++NodeIt)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
				{
					URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(Node->GetGraph());
					if (RigVMEdGraph == nullptr)
					{
						continue;
					}
					
					if (Node->CanUserDeleteNode())
					{
						if (const URigVMEdGraphNode* RigVMEdGraphNode = Cast<URigVMEdGraphNode>(Node))
						{
							if(Controller == nullptr)
							{
								Controller = RigVMEdGraphNode->GetController();
							}

							bRelinkPins = bRelinkPins || FSlateApplication::Get().GetModifierKeys().IsShiftDown();

							if(URigVMGraph* Model = RigVMEdGraph->GetModel())
							{
								if(URigVMNode* ModelNode = Model->FindNodeByName(*RigVMEdGraphNode->GetModelNodePath()))
								{
									NodesToRemove.Add(ModelNode);
								}
							}
						}
						else if (const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
						{
							if(URigVMGraph* Model = RigVMEdGraph->GetModel())
							{
								if(URigVMNode* ModelNode = Model->FindNodeByName(CommentNode->GetFName()))
								{
									NodesToRemove.Add(ModelNode);
								}
							}
						}
						else
						{
							Node->GetGraph()->RemoveNode(Node);
						}
					}
				}
			}

			if(NodesToRemove.IsEmpty() || Controller == nullptr)
			{
				return;
			}

			Controller->OpenUndoBracket(TEXT("Delete selected nodes"));
			if(bRelinkPins && NodesToRemove.Num() == 1)
			{
				Controller->RelinkSourceAndTargetPins(NodesToRemove[0], true);;
			}
			Controller->RemoveNodes(NodesToRemove, true);
			Controller->CloseUndoBracket();
		});
		GraphArgs.OnGraphSelectionChanged = Workspace::FOnGraphSelectionChanged::CreateLambda([](const Workspace::FWorkspaceEditorContext& InContext, const FGraphPanelSelectionSet& NewSelection)
		{
			URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(InContext.Object);
			if (RigVMEdGraph == nullptr)
			{
				return;
			}

			if (RigVMEdGraph->bIsSelecting || GIsTransacting)
			{
				return;
			}

			TGuardValue<bool> SelectGuard(RigVMEdGraph->bIsSelecting, true);

			TArray<FName> NodeNamesToSelect;
			for (UObject* Object : NewSelection)
			{
				if (URigVMEdGraphNode* RigVMEdGraphNode = Cast<URigVMEdGraphNode>(Object))
				{
					NodeNamesToSelect.Add(RigVMEdGraphNode->GetModelNodeName());
				}
				else if(UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
				{
					NodeNamesToSelect.Add(Node->GetFName());
				}
			}
			RigVMEdGraph->GetController()->SetNodeSelection(NodeNamesToSelect, true, true);

			InContext.WorkspaceEditor->SetDetailsObjects(NewSelection.Array());
		});

		WorkspaceEditorModule.RegisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNextUncookedOnly.AnimNextGraph_EdGraph")), WorkspaceEditorModule.CreateGraphDocumentArgs(GraphArgs));
	}

	void UnregisterWorkspaceDocumentTypes()
	{
		if(FModuleManager::Get().IsModuleLoaded("WorkspaceEditor"))
		{
			Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<Workspace::IWorkspaceEditorModule>("PropertyEditor");
			WorkspaceEditorModule.UnregisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNext.AnimNextSchedule")));
			WorkspaceEditorModule.UnregisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNext.AnimNextGraph")));
			WorkspaceEditorModule.UnregisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNextUncookedOnly.AnimNextGraph_EdGraph")));
		}
	}
	
	/** Node factory for the AnimNext graph */
	TSharedPtr<FAnimNextGraphPanelNodeFactory> AnimNextGraphPanelNodeFactory;
	
	/** Pin factory for parameters */
	TSharedPtr<FParametersGraphPanelPinFactory> ParametersGraphPanelPinFactory;

	/** Type identifier for parameter names */
	TSharedPtr<FParamNamePropertyTypeIdentifier> Identifier;
};

}

IMPLEMENT_MODULE(UE::AnimNext::Editor::FModule, AnimNextEditor);

#undef LOCTEXT_NAMESPACE