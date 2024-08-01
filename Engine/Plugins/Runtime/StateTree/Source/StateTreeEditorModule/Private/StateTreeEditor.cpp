// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "Blueprint/StateTreeConsiderationBlueprintBase.h"
#include "ContentBrowserModule.h"
#include "ClassViewerFilter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Customizations/StateTreeBindingExtension.h"
#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "Misc/UObjectToken.h"
#include "SStateTreeView.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditorCommands.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorSettings.h"
#include "StateTreeObjectHash.h"
#include "StateTreeTaskBase.h"
#include "StateTreeToolMenuContext.h"
#include "StateTreeViewModel.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenuEntry.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "FileHelpers.h"
#include "PropertyPath.h"
#include "SStateTreeOutliner.h"
#include "Debugger/SStateTreeDebuggerView.h"
#include "StateTreeSettings.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

const FName StateTreeEditorAppName(TEXT("StateTreeEditorApp"));

const FName FStateTreeEditor::SelectionDetailsTabId(TEXT("StateTreeEditor_SelectionDetails"));
const FName FStateTreeEditor::AssetDetailsTabId(TEXT("StateTreeEditor_AssetDetails"));
const FName FStateTreeEditor::StateTreeViewTabId(TEXT("StateTreeEditor_StateTreeView"));
const FName FStateTreeEditor::StateTreeOutlinerTabId(TEXT("StateTreeEditor_StateTreeOutliner"));
const FName FStateTreeEditor::StateTreeStatisticsTabId(TEXT("StateTreeEditor_StateTreeStatistics"));
const FName FStateTreeEditor::CompilerResultsTabId(TEXT("StateTreeEditor_CompilerResults"));
#if WITH_STATETREE_TRACE_DEBUGGER
const FName FStateTreeEditor::DebuggerTabId(TEXT("StateTreeEditor_Debugger"));
#endif // WITH_STATETREE_TRACE_DEBUGGER


namespace UE::StateTree::Editor
{
bool GbDisplayItemIds = false;

FAutoConsoleVariableRef CVarDisplayItemIds(
	TEXT("statetree.displayitemids"),
	GbDisplayItemIds,
	TEXT("Appends Id to task and state names in the treeview and expose Ids in the details view."));
}

void FStateTreeEditor::PostUndo(bool bSuccess)
{
}

void FStateTreeEditor::PostRedo(bool bSuccess)
{
}

void FStateTreeEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (StateTree != nullptr)
	{
		Collector.AddReferencedObject(StateTree);
	}
}

void FStateTreeEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_StateTreeEditor", "StateTree Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(SelectionDetailsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_SelectionDetails) )
		.SetDisplayName( NSLOCTEXT("StateTreeEditor", "SelectionDetailsTab", "Details" ) )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(AssetDetailsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_AssetDetails))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "AssetDetailsTab", "Asset Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(StateTreeViewTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_StateTreeView))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "StateTreeViewTab", "States"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(StateTreeOutlinerTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_StateTreeOutliner))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "StateTreeOutlinerTab", "Outliner"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(StateTreeStatisticsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_StateTreeStatistics))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "StatisticsTab", "StateTree Statistics"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.ToggleStats"));
	
	InTabManager->RegisterTabSpawner(CompilerResultsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_CompilerResults))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "CompilerResultsTab", "Compiler Results"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Log.TabIcon"));
	
#if WITH_STATETREE_TRACE_DEBUGGER
	InTabManager->RegisterTabSpawner(DebuggerTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_Debugger))
	   .SetDisplayName(NSLOCTEXT("StateTreeEditor", "DebuggerTab", "Debugger"))
	   .SetGroup(WorkspaceMenuCategoryRef)
	   .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"));
#endif // WITH_STATETREE_TRACE_DEBUGGER
}


void FStateTreeEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(SelectionDetailsTabId);
	InTabManager->UnregisterTabSpawner(AssetDetailsTabId);
	InTabManager->UnregisterTabSpawner(StateTreeViewTabId);
	InTabManager->UnregisterTabSpawner(StateTreeOutlinerTabId);
	InTabManager->UnregisterTabSpawner(StateTreeStatisticsTabId);
	InTabManager->UnregisterTabSpawner(CompilerResultsTabId);
#if WITH_STATETREE_TRACE_DEBUGGER
	InTabManager->UnregisterTabSpawner(DebuggerTabId);
#endif // WITH_STATETREE_TRACE_DEBUGGER
}

void FStateTreeEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* InStateTree)
{
	StateTree = InStateTree;
	check(StateTree != NULL);

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (EditorData == NULL)
	{
		EditorData = NewObject<UStateTreeEditorData>(StateTree, FName(), RF_Transactional);
		EditorData->AddRootState();
		StateTree->EditorData = EditorData;
		Compile();
	}

	EditorDataHash = UE::StateTree::Editor::CalcAssetHash(*StateTree);
	
	// @todo: Temporary fix
	// Make sure all states are transactional
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		TArray<UStateTreeState*> Stack;

		Stack.Add(SubTree);
		while (!Stack.IsEmpty())
		{
			if (UStateTreeState* State = Stack.Pop())
			{
				State->SetFlags(RF_Transactional);
				
				for (UStateTreeState* ChildState : State->Children)
				{
					Stack.Add(ChildState);
				}
			}
		}
	}


	StateTreeViewModel = MakeShareable(new FStateTreeViewModel());
	StateTreeViewModel->Init(EditorData);

	StateTreeViewModel->GetOnAssetChanged().AddSP(this, &FStateTreeEditor::HandleModelAssetChanged);
	StateTreeViewModel->GetOnStateAdded().AddSPLambda(this, [this](UStateTreeState* , UStateTreeState*){ UpdateAsset(); });
	StateTreeViewModel->GetOnStatesRemoved().AddSPLambda(this, [this](const TSet<UStateTreeState*>&){ UpdateAsset(); });
	StateTreeViewModel->GetOnStatesMoved().AddSPLambda(this, [this](const TSet<UStateTreeState*>&, const TSet<UStateTreeState*>&){ UpdateAsset(); });
	StateTreeViewModel->GetOnSelectionChanged().AddSP(this, &FStateTreeEditor::HandleModelSelectionChanged);
	StateTreeViewModel->GetOnBringNodeToFocus().AddSP(this, &FStateTreeEditor::HandleModelBringNodeToFocus);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false;
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	CompilerResultsListing = MessageLogModule.CreateLogListing("StateTreeCompiler", LogOptions);
	CompilerResults = MessageLogModule.CreateLogListingWidget(CompilerResultsListing.ToSharedRef());

	CompilerResultsListing->OnMessageTokenClicked().AddSP(this, &FStateTreeEditor::HandleMessageTokenClicked);

	
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_StateTree_Layout_v4")
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(AssetDetailsTabId, ETabState::OpenedTab)
				->AddTab(StateTreeStatisticsTabId, ETabState::OpenedTab)
				->AddTab(StateTreeOutlinerTabId, ETabState::OpenedTab)
				->SetForegroundTab(AssetDetailsTabId)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.5f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.75f)
					->AddTab(StateTreeViewTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(CompilerResultsTabId, ETabState::ClosedTab)
#if WITH_STATETREE_TRACE_DEBUGGER
					->AddTab(DebuggerTabId, ETabState::ClosedTab)
#endif // WITH_STATETREE_TRACE_DEBUGGER
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(SelectionDetailsTabId, ETabState::OpenedTab)
				->SetForegroundTab(SelectionDetailsTabId)
			)
		)
	);


	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, StateTreeEditorAppName, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, StateTree);

	BindCommands();
	RegisterToolbar();
	
	FStateTreeEditorModule& StateTreeEditorModule = FModuleManager::LoadModuleChecked<FStateTreeEditorModule>("StateTreeEditorModule");
	AddMenuExtender(StateTreeEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	RegenerateMenusAndToolbars();

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeEditor::OnIdentifierChanged);
	UE::StateTree::Delegates::OnSchemaChanged.AddSP(this, &FStateTreeEditor::OnSchemaChanged);
	UE::StateTree::Delegates::OnParametersChanged.AddSP(this, &FStateTreeEditor::OnRefreshDetailsView);
	UE::StateTree::Delegates::OnGlobalDataChanged.AddSP(this, &FStateTreeEditor::OnRefreshDetailsView);
	UE::StateTree::Delegates::OnStateParametersChanged.AddSP(this, &FStateTreeEditor::OnStateParametersChanged);
}

FName FStateTreeEditor::GetToolkitFName() const
{
	return FName("StateTreeEditor");
}

FText FStateTreeEditor::GetBaseToolkitName() const
{
	return NSLOCTEXT("StateTreeEditor", "AppLabel", "State Tree");
}

FString FStateTreeEditor::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("StateTreeEditor", "WorldCentricTabPrefix", "State Tree").ToString();
}

FLinearColor FStateTreeEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

void FStateTreeEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	UStateTreeToolMenuContext* Context = NewObject<UStateTreeToolMenuContext>();
	Context->StateTreeEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

void FStateTreeEditor::HandleMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken)
{
	if (InMessageToken->GetType() == EMessageToken::Object)
	{
		const TSharedRef<FUObjectToken> ObjectToken = StaticCastSharedRef<FUObjectToken>(InMessageToken);
		UStateTreeState* State = Cast<UStateTreeState>(ObjectToken->GetObject().Get());
		if (State)
		{
			StateTreeViewModel->SetSelection(State);
		}
	}
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_StateTreeView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StateTreeViewTabId);

	return SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "StateTreeViewTab", "States"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(StateTreeView, SStateTreeView, StateTreeViewModel.ToSharedRef(), TreeViewCommandList)
		];
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_StateTreeOutliner(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StateTreeOutlinerTabId);

	return SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "StateTreeOutlinerTab", "Outliner"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(StateTreeOutliner, SStateTreeOutliner, StateTreeViewModel.ToSharedRef(), TreeViewCommandList)
		];
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_SelectionDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SelectionDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	SelectionDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SelectionDetailsView->SetObject(nullptr);
	SelectionDetailsView->OnFinishedChangingProperties().AddSP(this, &FStateTreeEditor::OnSelectionFinishedChangingProperties);

	FStateTreeEditorModule::SetDetailPropertyHandlers(*SelectionDetailsView);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "SelectionDetailsTab", "Details"))
		[
			SelectionDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AssetDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	AssetDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	AssetDetailsView->SetObject(StateTree ? StateTree->EditorData : nullptr);
	AssetDetailsView->OnFinishedChangingProperties().AddSP(this, &FStateTreeEditor::OnAssetFinishedChangingProperties);

	FStateTreeEditorModule::SetDetailPropertyHandlers(*AssetDetailsView);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "AssetDetailsTabLabel", "Asset Details"))
		[
			AssetDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_StateTreeStatistics(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StateTreeStatisticsTabId);
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatisticsTitle", "StateTree Statistics"))
		[
			SNew(SMultiLineEditableTextBox)
			.Padding(10.0f)
			.Style(FAppStyle::Get(), "Log.TextBox")
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ForegroundColor(FLinearColor::Gray)
			.IsReadOnly(true)
			.Text(this, &FStateTreeEditor::GetStatisticsText)
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_CompilerResults(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == CompilerResultsTabId);
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("CompilerResultsTitle", "Compiler Results"))
		[
			SNew(SBox)
			[
				CompilerResults.ToSharedRef()
			]
		];
	return SpawnedTab;
}

#if WITH_STATETREE_TRACE_DEBUGGER
TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_Debugger(const FSpawnTabArgs& Args)
{
	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
	if (StateTree != nullptr)
	{
		// Reuse existing view if Tab is reopened
		if (!DebuggerView.IsValid())
		{
			DebuggerView = SNew(SStateTreeDebuggerView, *StateTree, StateTreeViewModel.ToSharedRef(), TreeViewCommandList);
		}
		Widget = DebuggerView;
	}
	
	check(Args.GetTabId() == DebuggerTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("DebuggerTitle", "Debugger"))
		.TabColorScale(GetTabColorScale())
		[
			Widget.ToSharedRef()
		];
}
#endif // WITH_STATETREE_TRACE_DEBUGGER

FText FStateTreeEditor::GetStatisticsText() const
{
	if (!StateTree)
	{
		return FText::GetEmpty();
	}


	TArray<FStateTreeMemoryUsage> MemoryUsages = StateTree->CalculateEstimatedMemoryUsage();
	if (MemoryUsages.IsEmpty())
	{
		return FText::GetEmpty();
	}

	TArray<FText> Rows;

	for (const FStateTreeMemoryUsage& Usage : MemoryUsages)
	{
		const FText SizeText = FText::AsMemory(Usage.EstimatedMemoryUsage);
		const FText NumNodesText = FText::AsNumber(Usage.NodeCount);
		Rows.Add(FText::Format(LOCTEXT("UsageRow", "{0}: {1}, {2} nodes"), FText::FromString(Usage.Name), SizeText, NumNodesText));
	}

	return FText::Join(FText::FromString(TEXT("\n")), Rows);
}

void FStateTreeEditor::HandleModelAssetChanged()
{
	UpdateAsset();
}

void FStateTreeEditor::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UStateTreeState>>& SelectedStates)
{
	if (SelectionDetailsView)
	{
		TArray<UObject*> Selected;
		for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
		{
			if (UStateTreeState* State = WeakState.Get())
			{
				Selected.Add(State);
			}
		}
		SelectionDetailsView->SetObjects(Selected);
	}
}

void FStateTreeEditor::HandleModelBringNodeToFocus(const UStateTreeState* State, const FGuid NodeID)
{
	if (SelectionDetailsView && State)
	{
		FPropertyPath HighlightPath;

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* TasksProperty = CastFieldChecked<FArrayProperty>(UStateTreeState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UStateTreeState, Tasks)));
			const int32 TaskIndex = State->Tasks.IndexOfByPredicate([&NodeID](const FStateTreeEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (TaskIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty));
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty->Inner, TaskIndex));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FProperty* SingleTaskProperty = CastFieldChecked<FProperty>(UStateTreeState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UStateTreeState, SingleTask)));
			if (State->SingleTask.ID == NodeID)
			{
				HighlightPath.AddProperty(FPropertyInfo(SingleTaskProperty));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* TransitionsProperty = CastFieldChecked<FArrayProperty>(UStateTreeState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UStateTreeState, Transitions)));
			const int32 TransitionIndex = State->Transitions.IndexOfByPredicate([&NodeID](const FStateTreeTransition& Transition)
			{
				return Transition.ID == NodeID;
			});
			if (TransitionIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(TransitionsProperty));
				HighlightPath.AddProperty(FPropertyInfo(TransitionsProperty->Inner, TransitionIndex));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* EnterConditionsProperty = CastFieldChecked<FArrayProperty>(UStateTreeState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UStateTreeState, EnterConditions)));
			const int32 EnterConditionIndex = State->EnterConditions.IndexOfByPredicate([&NodeID](const FStateTreeEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (EnterConditionIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(EnterConditionsProperty));
				HighlightPath.AddProperty(FPropertyInfo(EnterConditionsProperty->Inner, EnterConditionIndex));
			}
		}

		if (HighlightPath.IsValid())
		{
			SelectionDetailsView->ScrollPropertyIntoView(HighlightPath, /*bExpandProperty*/true);
			SelectionDetailsView->HighlightProperty(HighlightPath);
			
			GEditor->GetTimerManager()->SetTimer(
				HighlighTimerHandle,
				FTimerDelegate::CreateLambda([SelectionDetailsView = SelectionDetailsView]()
				{
					SelectionDetailsView->HighlightProperty({});
				}),
				1.0f,
				/*Loop*/false);
		}
	}	
}

void FStateTreeEditor::SaveAsset_Execute()
{
	// Remember the treeview expansion state
	if (StateTreeView)
	{
		StateTreeView->SavePersistentExpandedStates();
	}

	UpdateAsset();

	// save it
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FStateTreeEditor::OnIdentifierChanged(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		UpdateAsset();
	}
}

void FStateTreeEditor::OnSchemaChanged(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		UpdateAsset();
		
		if (StateTreeViewModel)
		{
			StateTreeViewModel->NotifyAssetChangedExternally();
		}

		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnRefreshDetailsView(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FStateTreeBindingExtension can rebuild the list of bindable structs
		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnStateParametersChanged(const UStateTree& InStateTree, const FGuid ChangedStateID)
{
	if (StateTree == &InStateTree)
	{
		if (const UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree->EditorData))
		{
			TreeData->VisitHierarchy([&ChangedStateID](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				if (State.Type == EStateTreeStateType::Linked && State.LinkedSubtree.ID == ChangedStateID)
				{
					State.UpdateParametersFromLinkedSubtree();
				}
				return EStateTreeVisitor::Continue;
			});
		}

		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FStateTreeBindingExtension can rebuild the list of bindable structs
		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnAssetFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure nodes get updates when properties are changed.
	if (StateTreeViewModel)
	{
		StateTreeViewModel->NotifyAssetChangedExternally();
	}
}

void FStateTreeEditor::OnSelectionFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure nodes get updates when properties are changed.
	if (StateTreeViewModel)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = SelectionDetailsView->GetSelectedObjects();
		TSet<UStateTreeState*> ChangedStates;
		for (const TWeakObjectPtr<UObject>& WeakObject : SelectedObjects)
		{
			if (UObject* Object = WeakObject.Get())
			{
				if (UStateTreeState* State = Cast<UStateTreeState>(Object))
				{
					ChangedStates.Add(State);
				}
			}
		}
		if (ChangedStates.Num() > 0)
		{
			StateTreeViewModel->NotifyStatesChangedExternally(ChangedStates, PropertyChangedEvent);
			UpdateAsset();
		}
	}
}

namespace UE::StateTree::Editor::Internal
{
	bool FixChangedStateLinkName(FStateTreeStateLink& StateLink, const TMap<FGuid, FName>& IDToName)
	{
		if (StateLink.ID.IsValid())
		{
			const FName* Name = IDToName.Find(StateLink.ID);
			if (Name == nullptr)
			{
				// Missing link, we'll show these in the UI
				return false;
			}
			if (StateLink.Name != *Name)
			{
				// Name changed, fix!
				StateLink.Name = *Name;
				return true;
			}
		}
		return false;
	}

	void ValidateLinkedStates(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->Modify();

		// Make sure all state links are valid and update the names if needed.

		// Create ID to state name map.
		TMap<FGuid, FName> IDToName;

		TreeData->VisitHierarchy([&IDToName](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			IDToName.Add(State.ID, State.Name);
			return EStateTreeVisitor::Continue;
		});
		
		// Fix changed names.
		TreeData->VisitHierarchy([&IDToName](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			State.Modify();
			if (State.Type == EStateTreeStateType::Linked)
			{
				FixChangedStateLinkName(State.LinkedSubtree, IDToName);
			}
					
			for (FStateTreeTransition& Transition : State.Transitions)
			{
				FixChangedStateLinkName(Transition.State, IDToName);
			}

			return EStateTreeVisitor::Continue;
		});
	}

	void UpdateParents(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->Modify();
		TreeData->ReparentStates();
	}

	void ApplySchema(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		
		}
		const UStateTreeSchema* Schema = TreeData->Schema;
		if (!Schema)
		{
			return;
		}

		TreeData->Modify();
		
		// Clear evaluators if not allowed.
		if (Schema->AllowEvaluators() == false && TreeData->Evaluators.Num() > 0)
		{
			UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Evaluators due to current schema restrictions."), *GetNameSafe(&StateTree));
			TreeData->Evaluators.Reset();
		}


		TreeData->VisitHierarchy([&StateTree, Schema](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			State.Modify();

			// Clear enter conditions if not allowed.
			if (Schema->AllowEnterConditions() == false && State.EnterConditions.Num() > 0)
			{
				UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Enter Conditions in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				State.EnterConditions.Reset();
			}

			// Clear Utility if not allowed
			if (Schema->AllowUtilityConsiderations() == false && State.Considerations.Num() > 0)
			{
				UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Utility Considerations in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				State.Considerations.Reset();
			}

			// Keep single and many tasks based on what is allowed.
			if (Schema->AllowMultipleTasks() == false)
			{
				if (State.Tasks.Num() > 0)
				{
					State.Tasks.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Tasks in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
				
				// Task name is the same as state name.
				if (FStateTreeTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FStateTreeTaskBase>())
				{
					Task->Name = State.Name;
				}
			}
			else
			{
				if (State.SingleTask.Node.IsValid())
				{
					State.SingleTask.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Single Task in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
			}
			
			return EStateTreeVisitor::Continue;
		});
	}

	void RemoveUnusedBindings(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TMap<FGuid, const FStateTreeDataView> AllStructValues;
		TreeData->GetAllStructValues(AllStructValues);
		TreeData->Modify();
		TreeData->GetPropertyEditorBindings()->RemoveUnusedBindings(AllStructValues);
	}

	void UpdateLinkedStateParameters(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->Modify();

		TreeData->VisitHierarchy([](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			if (State.Type == EStateTreeStateType::Linked
				|| State.Type == EStateTreeStateType::LinkedAsset)
			{
				State.Modify();
				State.UpdateParametersFromLinkedSubtree();
			}
			return EStateTreeVisitor::Continue;
		});
	}

	static void MakeSaveOnCompileSubMenu(UToolMenu* InMenu)
	{
		FToolMenuSection& Section = InMenu->AddSection("Section");
		const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();
		Section.AddMenuEntry(Commands.SaveOnCompile_Never);
		Section.AddMenuEntry(Commands.SaveOnCompile_SuccessOnly);
		Section.AddMenuEntry(Commands.SaveOnCompile_Always);
	}

	static void GenerateCompileOptionsMenu(UToolMenu* InMenu)
	{
		FToolMenuSection& Section = InMenu->AddSection("Section");
		const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

		// @TODO: disable the menu and change up the tooltip when all sub items are disabled
		Section.AddSubMenu(
			"SaveOnCompile",
			LOCTEXT("SaveOnCompileSubMenu", "Save on Compile"),
			LOCTEXT("SaveOnCompileSubMenu_ToolTip", "Determines how the StateTree is saved whenever you compile it."),
			FNewToolMenuDelegate::CreateStatic(&MakeSaveOnCompileSubMenu));
	}

	static void SetSaveOnCompileSetting(const EStateTreeSaveOnCompile NewSetting)
	{
		UStateTreeEditorSettings* Settings = GetMutableDefault<UStateTreeEditorSettings>();
		Settings->SaveOnCompile = NewSetting;
		Settings->SaveConfig();
	}

	static bool IsSaveOnCompileOptionSet(TWeakPtr<FStateTreeEditor> Editor, const EStateTreeSaveOnCompile Option)
	{
		const UStateTreeEditorSettings* Settings = GetDefault<UStateTreeEditorSettings>();

		EStateTreeSaveOnCompile CurrentSetting = Settings->SaveOnCompile;
		if (!Editor.IsValid() || !Editor.Pin()->IsSaveOnCompileEnabled())
		{
			// If save-on-compile is disabled for the StateTree, then we want to 
			// show "Never" as being selected
			// 
			// @TODO: a tooltip explaining why would be nice too
			CurrentSetting = EStateTreeSaveOnCompile::Never;
		}

		return (CurrentSetting == Option);
	}
} // UE::StateTree::Editor::Internal

namespace UE::StateTree::Editor
{
	void ValidateAsset(UStateTree& StateTree)
	{
		UE::StateTree::Editor::Internal::UpdateParents(StateTree);
		UE::StateTree::Editor::Internal::ApplySchema(StateTree);
		UE::StateTree::Editor::Internal::RemoveUnusedBindings(StateTree);
		UE::StateTree::Editor::Internal::ValidateLinkedStates(StateTree);
		UE::StateTree::Editor::Internal::UpdateLinkedStateParameters(StateTree);
	}

	uint32 CalcAssetHash(const UStateTree& StateTree)
	{
		uint32 EditorDataHash = 0;
		if (StateTree.EditorData != nullptr)
		{
			FStateTreeObjectCRC32 Archive;
			EditorDataHash = Archive.Crc32(StateTree.EditorData, 0);
		}

		return EditorDataHash;
	}

	template <typename ClassType, typename = typename TEnableIf<TIsDerivedFrom<ClassType, UStateTreeNodeBlueprintBase>::Value>::Type>
	class FEditorNodeClassFilter : public IClassViewerFilter
	{
	public:
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			check(InClass);
			return InClass->IsChildOf(ClassType::StaticClass());
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InUnloadedClassData->IsChildOf(ClassType::StaticClass());
		}
	};

	using FStateTreeTaskBPClassFilter = FEditorNodeClassFilter<UStateTreeTaskBlueprintBase>;
	using FStateTreeConditionBPClassFilter = FEditorNodeClassFilter<UStateTreeConditionBlueprintBase>;
	using FStateTreeConsiderationBPClassFilter = FEditorNodeClassFilter<UStateTreeConsiderationBlueprintBase>;
}; // UE::StateTree::Editor

void FStateTreeEditor::BindCommands()
{
	const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.Compile,
		FExecuteAction::CreateSP(this, &FStateTreeEditor::Compile),
		FCanExecuteAction::CreateSP(this, &FStateTreeEditor::CanCompile));

	TWeakPtr<FStateTreeEditor> WeakThisPtr = SharedThis(this);
	ToolkitCommands->MapAction(
		FStateTreeEditorCommands::Get().SaveOnCompile_Never,
		FExecuteAction::CreateStatic(&UE::StateTree::Editor::Internal::SetSaveOnCompileSetting, EStateTreeSaveOnCompile::Never),
		FCanExecuteAction::CreateSP(this, &FStateTreeEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&UE::StateTree::Editor::Internal::IsSaveOnCompileOptionSet, WeakThisPtr, EStateTreeSaveOnCompile::Never)
	);
	ToolkitCommands->MapAction(
		FStateTreeEditorCommands::Get().SaveOnCompile_SuccessOnly,
		FExecuteAction::CreateStatic(&UE::StateTree::Editor::Internal::SetSaveOnCompileSetting, EStateTreeSaveOnCompile::SuccessOnly),
		FCanExecuteAction::CreateSP(this, &FStateTreeEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&UE::StateTree::Editor::Internal::IsSaveOnCompileOptionSet, WeakThisPtr, EStateTreeSaveOnCompile::SuccessOnly)
	);
	ToolkitCommands->MapAction(
		FStateTreeEditorCommands::Get().SaveOnCompile_Always,
		FExecuteAction::CreateStatic(&UE::StateTree::Editor::Internal::SetSaveOnCompileSetting, EStateTreeSaveOnCompile::Always),
		FCanExecuteAction::CreateSP(this, &FStateTreeEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&UE::StateTree::Editor::Internal::IsSaveOnCompileOptionSet, WeakThisPtr, EStateTreeSaveOnCompile::Always)
	);
}

bool FStateTreeEditor::IsSaveOnCompileEnabled() const
{
	return true;
}

void FStateTreeEditor::RegisterToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* ToolBar;
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolBar = ToolMenus->ExtendMenu(MenuName);
	}
	else
	{
		ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}

	static const FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);

	FToolMenuSection& CompileSection = ToolBar->AddSection("Compile", TAttribute<FText>(), InsertAfterAssetSection);
	
	CompileSection.AddDynamicEntry("CompileCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UStateTreeToolMenuContext* Context = InSection.FindContext<UStateTreeToolMenuContext>();
		if (Context && Context->StateTreeEditor.IsValid())
		{
			const TSharedPtr<FStateTreeEditor> StateTreeEditor = Context->StateTreeEditor.Pin();
			if (StateTreeEditor.IsValid())
			{
				const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

				FToolMenuEntry& CompileButton = InSection.AddEntry(FToolMenuEntry::InitToolBarButton(
					Commands.Compile,
					TAttribute<FText>(),
					TAttribute<FText>(),
					TAttribute<FSlateIcon>(StateTreeEditor.ToSharedRef(), &FStateTreeEditor::GetCompileStatusImage)));
				CompileButton.StyleNameOverride = "CalloutToolbar";

				FToolMenuEntry& CompileOptions = InSection.AddEntry(FToolMenuEntry::InitComboButton(
					"CompileComboButton",
					FUIAction(),
					FNewToolMenuDelegate::CreateStatic(&UE::StateTree::Editor::Internal::GenerateCompileOptionsMenu),
					LOCTEXT("CompileOptions_ToolbarTooltip", "Options to customize how State Trees compile")
				));
				CompileOptions.StyleNameOverride = "CalloutToolbar";
				CompileOptions.ToolBarData.bSimpleComboBox = true;
			}
		}
	}));

	static const FToolMenuInsert InsertAfterCompileSection("Compile", EToolMenuInsertType::After);

	FToolMenuSection& CreateNewNodeSection = ToolBar->AddSection("CreateNewNodes", TAttribute<FText>(), InsertAfterCompileSection);

	CreateNewNodeSection.AddDynamicEntry("CreateNewNodes", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UStateTreeToolMenuContext* Context = InSection.FindContext<UStateTreeToolMenuContext>();
		if (Context && Context->StateTreeEditor.IsValid())
		{
			const TSharedPtr<FStateTreeEditor> StateTreeEditor = Context->StateTreeEditor.Pin();
			if (StateTreeEditor.IsValid())
			{
				const TSharedRef<FStateTreeEditor> StateTreeEditorRef = StateTreeEditor.ToSharedRef();
				FToolMenuEntry& CreateNewTaskDropdown = InSection.AddEntry(FToolMenuEntry::InitComboButton(
					"CreateNewTaskComboButton",
					FUIAction(),
					FOnGetContent::CreateSP(StateTreeEditorRef, &FStateTreeEditor::GenerateTaskBPBaseClassesMenu),
					LOCTEXT("CreateNewTask_Title", "New Task"),
					LOCTEXT("CreateNewTask_ToolbarTooltip", "Create a new Blueprint State Tree Task"),
					TAttribute<FSlateIcon>(StateTreeEditorRef, &FStateTreeEditor::GetNewTaskButtonImage)
				));

				FToolMenuEntry& CreateNewConditionDropdown = InSection.AddEntry(FToolMenuEntry::InitComboButton(
					"CreateNewConditionComboButton",
					FUIAction(),
					FOnGetContent::CreateSP(StateTreeEditorRef, &FStateTreeEditor::GenerateConditionBPBaseClassesMenu),
					LOCTEXT("CreateNewCondition_Title", "New Condition"),
					LOCTEXT("CreateNewCondition_ToolbarTooltip", "Create a new Blueprint State Tree Condition"),
					TAttribute<FSlateIcon>(StateTreeEditorRef, &FStateTreeEditor::GetNewConditionButtonImage)
				));

				FToolMenuEntry& CreateNewConsiderationDropdown = InSection.AddEntry(FToolMenuEntry::InitComboButton(
					"CreateNewConsiderationComboButton",
					FUIAction(),
					FOnGetContent::CreateSP(StateTreeEditorRef, &FStateTreeEditor::GenerateConsiderationBPBaseClassesMenu),
					LOCTEXT("CreateNewConsideration_Title", "New Consideration"),
					LOCTEXT("CreateNewConsideration_ToolbarTooltip", "Create a new Blueprint State Tree Utility Consideration"),
					TAttribute<FSlateIcon>(StateTreeEditorRef, &FStateTreeEditor::GetNewConsiderationButtonImage)
				));
			}
		}
	}));
}

void FStateTreeEditor::Compile()
{
	if (!StateTree)
	{
		return;
	}

	// Note: If the compilation process changes, also update UStateTreeCompileAllCommandlet and UStateTreeFactory::FactoryCreateNew.
	
	UpdateAsset();
	
	if (CompilerResultsListing.IsValid())
	{
		CompilerResultsListing->ClearMessages();
	}

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);

	bLastCompileSucceeded = Compiler.Compile(*StateTree);

	if (CompilerResultsListing.IsValid())
	{
		Log.AppendToLog(CompilerResultsListing.Get());
	}

	if (bLastCompileSucceeded)
	{
		// Success
		StateTree->LastCompiledEditorDataHash = EditorDataHash;

		UE::StateTree::Delegates::OnPostCompile.Broadcast(*StateTree);
	}
	else
	{
		// Make sure not to leave stale data on failed compile.
		StateTree->ResetCompiled();
		StateTree->LastCompiledEditorDataHash = 0;

		// Show log
		TabManager->TryInvokeTab(CompilerResultsTabId);
	}

	const UStateTreeEditorSettings* Settings = GetMutableDefault<UStateTreeEditorSettings>();
	const bool bShouldSaveOnCompile = ((Settings->SaveOnCompile == EStateTreeSaveOnCompile::Always)
									|| ((Settings->SaveOnCompile == EStateTreeSaveOnCompile::SuccessOnly) && bLastCompileSucceeded));

	if (bShouldSaveOnCompile)
	{
		const TArray<UPackage*> PackagesToSave { StateTree->GetOutermost() };
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
	}
}

bool FStateTreeEditor::CanCompile() const
{
	if (StateTree == nullptr)
	{
		return false;
	}
	
	// We can't recompile while in PIE
	if (GEditor->IsPlaySessionInProgress())
	{
		return false;
	}

	return true;
}

FSlateIcon FStateTreeEditor::GetCompileStatusImage() const
{
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");
	static const FName CompileStatusWarning("Blueprint.CompileStatus.Overlay.Warning");

	if (StateTree == nullptr)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	}
	
	const bool bCompiledDataResetDuringLoad = StateTree->LastCompiledEditorDataHash == EditorDataHash && !StateTree->IsReadyToRun();

	if (!bLastCompileSucceeded || bCompiledDataResetDuringLoad)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusError);
	}
	
	if (StateTree->LastCompiledEditorDataHash != EditorDataHash)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	}
	
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusGood);
}

FSlateIcon FStateTreeEditor::GetNewConditionButtonImage() const
{
	//placeholder
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), NAME_None, NAME_None, NAME_None);
}

TSharedRef<SWidget> FStateTreeEditor::GenerateConditionBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilters.Add(MakeShareable(new UE::StateTree::Editor::FStateTreeConditionBPClassFilter));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FStateTreeEditor::OnNodeBPBaseClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

FSlateIcon FStateTreeEditor::GetNewConsiderationButtonImage() const
{
	//placeholder
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), NAME_None, NAME_None, NAME_None);
}

TSharedRef<SWidget> FStateTreeEditor::GenerateConsiderationBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilters.Add(MakeShareable(new UE::StateTree::Editor::FStateTreeConsiderationBPClassFilter));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FStateTreeEditor::OnNodeBPBaseClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

FSlateIcon FStateTreeEditor::GetNewTaskButtonImage() const
{
	//placeholder
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), NAME_None, NAME_None, NAME_None);
}

TSharedRef<SWidget> FStateTreeEditor::GenerateTaskBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilters.Add(MakeShareable(new UE::StateTree::Editor::FStateTreeTaskBPClassFilter));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FStateTreeEditor::OnNodeBPBaseClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

void FStateTreeEditor::OnNodeBPBaseClassPicked(UClass* NodeClass) const
{
	check(NodeClass);
	
	if (!StateTree)
	{
		return;
	}

	const FString ClassName = FBlueprintEditorUtils::GetClassNameWithoutSuffix(NodeClass);
	const FString PathName = FPaths::GetPath(StateTree->GetOutermost()->GetPathName());

	// Now that we've generated some reasonable default locations/names for the package, allow the user to have the final say
	// before we create the package and initialize the blueprint inside of it.
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathName;
	SaveAssetDialogConfig.DefaultAssetName = ClassName + TEXT("_New");
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (!SaveObjectPath.IsEmpty())
	{
		const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		const FString SavePackagePath = FPaths::GetPath(SavePackageName);
		const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);

		if (UPackage* Package = CreatePackage(*SavePackageName))
		{
			// Create and init a new Blueprint
			if (UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(NodeClass, Package, FName(*SaveAssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()))
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewBP);

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewBP);

				Package->MarkPackageDirty();
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

void FStateTreeEditor::UpdateAsset()
{
	if (!StateTree)
	{
		return;
	}

	UE::StateTree::Editor::ValidateAsset(*StateTree);
	EditorDataHash = UE::StateTree::Editor::CalcAssetHash(*StateTree);
}

#undef LOCTEXT_NAMESPACE
