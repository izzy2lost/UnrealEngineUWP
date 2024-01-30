// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionEditorViewModel.h"
#include "Actions/AvaTransitionStateActions.h"
#include "Actions/AvaTransitionTreeActions.h"
#include "AvaTransitionCommands.h"
#include "AvaTransitionEditor.h"
#include "AvaTransitionEditorLog.h"
#include "AvaTransitionEditorUtils.h"
#include "AvaTransitionSelection.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeEditorData.h"
#include "AvaTransitionViewModelChildren.h"
#include "AvaTransitionViewModelSharedData.h"
#include "Menu/AvaTransitionToolbar.h"
#include "Menu/AvaTransitionTreeContextMenu.h"
#include "Serialization/ArchiveObjectCrc32.h"
#include "State/AvaTransitionStateViewModel.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditorSettings.h"
#include "StateTreeTaskBase.h"
#include "Views/SAvaTransitionTreeView.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "AvaTransitionEditorViewModel"

FAvaTransitionEditorViewModel::FAvaTransitionEditorViewModel(UAvaTransitionTree* InTransitionTree, const TSharedPtr<FAvaTransitionEditor>& InEditor)
	: Toolbar(MakeShared<FAvaTransitionToolbar>(*this))
	, ContextMenu(MakeShared<FAvaTransitionTreeContextMenu>(*this))
	, TransitionTreeWeak(InTransitionTree)
	, EditorWeak(InEditor)
	, Compiler(*this)
	, CommandList(MakeShared<FUICommandList>())
{
}

FAvaTransitionEditorViewModel::~FAvaTransitionEditorViewModel()
{
	UnbindDelegates();
}

void FAvaTransitionEditorViewModel::BindCommands(const TSharedRef<FUICommandList>& InCommandList)
{
	Actions =
		{
			MakeShared<FAvaTransitionTreeActions>(*this),
			MakeShared<FAvaTransitionStateActions>(*this),
		};

	InCommandList->Append(CommandList);

	for (const TSharedRef<FAvaTransitionActions>& Action : Actions)
	{
		Action->BindCommands(CommandList);
	}
}

const FAvaTransitionCompiler& FAvaTransitionEditorViewModel::GetCompiler() const
{
	return Compiler;
}

bool FAvaTransitionEditorViewModel::CanCompile() const
{
	UAvaTransitionTree* TransitionTree = GetTransitionTree();

	bool bReadOnly = GetSharedData()->IsReadOnly();

	return !bReadOnly
		&& !GEditor->IsPlayingSessionInEditor()
		&& TransitionTree;
}

void FAvaTransitionEditorViewModel::Compile()
{
	if (CanCompile())
	{
		UpdateTree();
		Compiler.Compile();
	}
}

UAvaTransitionTree* FAvaTransitionEditorViewModel::GetTransitionTree() const
{
	return TransitionTreeWeak.Get();
}

UAvaTransitionTreeEditorData* FAvaTransitionEditorViewModel::GetEditorData() const
{
	return EditorDataWeak.Get();
}

bool FAvaTransitionEditorViewModel::UpdateEditorData(bool bInCreateIfNotFound)
{
	UAvaTransitionTree* TransitionTree = GetTransitionTree();
	if (!TransitionTree)
	{
		return false;
	}

	bool bCreatedNewEditorData = false;

	UAvaTransitionTreeEditorData* EditorData = Cast<UAvaTransitionTreeEditorData>(TransitionTree->EditorData);
	if (!EditorData && bInCreateIfNotFound)
	{
		EditorData = NewObject<UAvaTransitionTreeEditorData>(TransitionTree, NAME_None, RF_Transactional);
		EditorData->AddRootState();
		TransitionTree->EditorData = EditorData;
		bCreatedNewEditorData = true;
	}

	EditorDataWeak = EditorData;
	return bCreatedNewEditorData;
}

void FAvaTransitionEditorViewModel::UpdateTree()
{
	UAvaTransitionTree* TransitionTree = GetTransitionTree();
	if (!TransitionTree)
	{
		return;
	}

	ValidateTree(TransitionTree->GetName());
	EditorDataHash = CalculateTreeHash(*TransitionTree);
}

TSharedPtr<FAvaTransitionEditor> FAvaTransitionEditorViewModel::GetEditor() const
{
	return EditorWeak.Pin();
}

TSharedRef<FAvaTransitionSelection> FAvaTransitionEditorViewModel::GetSelection() const
{
	return GetSharedData()->GetSelection();
}

void FAvaTransitionEditorViewModel::OnInitialize()
{
	FAvaTransitionViewModel::OnInitialize();

	BindDelegates();

	if (UpdateEditorData(/*bCreateIfNotFound*/true))
	{
		Compile();
	}

	TSharedRef<FAvaTransitionEditorViewModel> This = SharedThis(this);

	TSharedRef<FAvaTransitionViewModelSharedData> ViewModelSharedData = GetSharedData();

	ViewModelSharedData->Initialize(This);

	if (TSharedPtr<FAvaTransitionEditor> Editor = GetEditor())
	{
		ViewModelSharedData->SetReadOnly(Editor->IsReadOnly());
	}

	TreeView = SNew(SAvaTransitionTreeView, This);
}

void FAvaTransitionEditorViewModel::PostRefresh()
{
	FAvaTransitionViewModel::PostRefresh();
	RefreshTreeView();
	UpdateTree();
}

void FAvaTransitionEditorViewModel::RefreshTreeView()
{
	if (TreeView.IsValid())
	{
		TreeView->Refresh();
	}
}

void FAvaTransitionEditorViewModel::GatherChildren(FAvaTransitionViewModelChildren& OutChildren)
{
	UAvaTransitionTreeEditorData* EditorData = GetEditorData();
	if (!EditorData)
	{
		return;
	}

	OutChildren.Reserve(EditorData->SubTrees.Num());

	for (UStateTreeState* State : EditorData->SubTrees)
	{
		OutChildren.Add<FAvaTransitionStateViewModel>(State);
	}
}

void FAvaTransitionEditorViewModel::PostRedo(bool bInSuccess)
{
	UpdateEditorData();
	Refresh();
}

void FAvaTransitionEditorViewModel::PostUndo(bool bInSuccess)
{
	UpdateEditorData();
	Refresh();
}

TSharedRef<SWidget> FAvaTransitionEditorViewModel::CreateWidget()
{
	check(TreeView.IsValid());
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Toolbar->GenerateTreeToolbarWidget()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				.FillSize(1.f)
				[
					TreeView.ToSharedRef()
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				TreeView->GetVerticalScrollbar()
			]
		];
}

UObject* FAvaTransitionEditorViewModel::GetObject() const
{
	return GetEditorData();
}

void FAvaTransitionEditorViewModel::BindDelegates()
{
	UnbindDelegates();

	if (UAvaTransitionTreeEditorData* EditorData = GetEditorData())
	{
		EditorData->GetOnTreeRequestRefresh().AddSP(this, &FAvaTransitionEditorViewModel::Refresh);
	}

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FAvaTransitionEditorViewModel::OnIdentifierChanged);
	UE::StateTree::Delegates::OnSchemaChanged.AddSP(this, &FAvaTransitionEditorViewModel::OnSchemaChanged);
	UE::StateTree::Delegates::OnParametersChanged.AddSP(this, &FAvaTransitionEditorViewModel::OnParametersChanged);
	UE::StateTree::Delegates::OnStateParametersChanged.AddSP(this, &FAvaTransitionEditorViewModel::OnStateParametersChanged);
}

void FAvaTransitionEditorViewModel::UnbindDelegates()
{
	if (UAvaTransitionTreeEditorData* EditorData = GetEditorData())
	{
		EditorData->GetOnTreeRequestRefresh().RemoveAll(this);
	}

	UE::StateTree::Delegates::OnIdentifierChanged.RemoveAll(this);
	UE::StateTree::Delegates::OnSchemaChanged.RemoveAll(this);
	UE::StateTree::Delegates::OnParametersChanged.RemoveAll(this);
	UE::StateTree::Delegates::OnStateParametersChanged.RemoveAll(this);
}

uint32 FAvaTransitionEditorViewModel::CalculateTreeHash(const UAvaTransitionTree& InTree) const
{
	if (!InTree.EditorData)
	{
		return 0;
	}

	static const FName MD_ExcludeFromHash(TEXT("ExcludeFromHash"));

	class : public FArchiveObjectCrc32
	{
		virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
		{
			return !InProperty
				|| FArchiveObjectCrc32::ShouldSkipProperty(InProperty)
				|| InProperty->HasAllPropertyFlags(CPF_Transient)
				|| InProperty->HasMetaData(MD_ExcludeFromHash);
		}
	} Archive;

	return Archive.Crc32(InTree.EditorData, 0);
}

void FAvaTransitionEditorViewModel::ValidateTree(const FString& InTreeDebugName)
{
	UAvaTransitionTreeEditorData* EditorData = GetEditorData();
	if (!EditorData || !EditorData->Schema)
	{
		return;
	}

	EditorData->ReparentStates();

	// Clear evaluators if not allowed.
	if (!EditorData->Evaluators.IsEmpty() && !EditorData->Schema->AllowEvaluators())
	{
		UE_LOG(LogAvaEditorTransition, Warning
			, TEXT("%s: Resetting Evaluators due to current schema restrictions.")
			, *InTreeDebugName);

		EditorData->Evaluators.Reset();
	}

	// Apply Schema Rules to each State 
	EditorData->VisitHierarchy([&InTreeDebugName, EditorData](UStateTreeState& State, UStateTreeState*)
	{
		// Clear enter conditions if not allowed.
		if (!State.EnterConditions.IsEmpty() && !EditorData->Schema->AllowEnterConditions())
		{
			UE_LOG(LogAvaEditorTransition, Warning
				, TEXT("%s: Resetting Enter Conditions in state %s due to current schema restrictions.")
				, *InTreeDebugName
				, *State.GetName());

			State.EnterConditions.Reset();
		}

		// Keep single and many tasks based on what is allowed.
		if (!EditorData->Schema->AllowMultipleTasks())
		{
			if (!State.Tasks.IsEmpty())
			{
				State.Tasks.Reset();
				UE_LOG(LogAvaEditorTransition, Warning
					, TEXT("%s: Resetting Tasks in state %s due to current schema restrictions.")
					, *InTreeDebugName
					, *State.GetName());
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
				UE_LOG(LogAvaEditorTransition, Warning
					, TEXT("%s: Resetting Single Task in state %s due to current schema restrictions.")
					, *InTreeDebugName
					, *State.GetName());
			}
		}

		return EStateTreeVisitor::Continue;
	});

	// Remove unused Bindings
	{
		TMap<FGuid, const FStateTreeDataView> AllStructValues;
		EditorData->GetAllStructValues(AllStructValues);
		EditorData->GetPropertyEditorBindings()->RemoveUnusedBindings(AllStructValues);
	}

	// Validate Linked States
	{
		// Make sure all state links are valid and update the names if needed.
		// Create ID to state name map.
		TMap<FGuid, FName> IdToName;

		EditorData->VisitHierarchy([&IdToName](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			IdToName.Add(State.ID, State.Name);
			return EStateTreeVisitor::Continue;
		});

		static auto FixChangedStateLinkName = [](FStateTreeStateLink& StateLink, const TMap<FGuid, FName>& IDToName)
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
		};

		// Fix changed names.
		EditorData->VisitHierarchy([&IdToName](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			if (State.Type == EStateTreeStateType::Linked)
			{
				FixChangedStateLinkName(State.LinkedSubtree, IdToName);
			}
					
			for (FStateTreeTransition& Transition : State.Transitions)
			{
				FixChangedStateLinkName(Transition.State, IdToName);
			}

			return EStateTreeVisitor::Continue;
		});
	}

	// Update Linked State Parameters
	EditorData->VisitHierarchy([](UStateTreeState& State, UStateTreeState* /*ParentState*/)
	{
		if (State.Type == EStateTreeStateType::Linked)
		{
			State.UpdateParametersFromLinkedSubtree();
		}
		return EStateTreeVisitor::Continue;
	});
}

void FAvaTransitionEditorViewModel::OnIdentifierChanged(const UStateTree& InStateTree)
{
	if (&InStateTree == GetTransitionTree())
	{
		UpdateTree();
	}
}

void FAvaTransitionEditorViewModel::OnSchemaChanged(const UStateTree& InStateTree)
{
	if (&InStateTree == GetTransitionTree())
	{
		UpdateTree();
		// todo: notify asset change externally
	}
}

void FAvaTransitionEditorViewModel::OnParametersChanged(const UStateTree& InStateTree)
{
	// todo
}

void FAvaTransitionEditorViewModel::OnStateParametersChanged(const UStateTree& InStateTree, const FGuid InGuid)
{
	// todo
}

#undef LOCTEXT_NAMESPACE
