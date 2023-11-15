// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationClientView.h"

#include "MultiUserReplicationSettings.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/Object/ActorSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Widgets/ActiveSession/Replication/Client/Columns/SingleClientColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SClientToolbar.h"

#include "HAL/IConsoleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SReplicationClientView"

namespace UE::MultiUserClient
{
	TAutoConsoleVariable<int32> CVarReplicationClientViewMode(
		TEXT("MultiUser.ReplicationEditorMode"),
		1,
		TEXT("Determines the look of the editor mode.\n0 - Three Sections\n1 - Two sections")
		);
	
	void SReplicationClientView::Construct(const FArguments& InArgs, const TSharedRef<IConcertClient>& InClient, FReplicationClientManager& InClientManager)
	{
		ConcertClient = InClient;
		ClientManager = &InClientManager;
		
		GetReplicationClientAttribute = InArgs._GetReplicationClient;
		FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		FGlobalAuthorityCache& AuthorityCache = ClientManager->GetAuthorityCache();
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SClientToolbar, AuthorityCache)
				.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
				.DisplayedClients(TSet{ ReplicationClient->GetEndpointId() })
				.ForEachReplicatedObject(this, &SReplicationClientView::EnumerateReplicatedObjects)
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				CreateEditorContent()
			]
		];
		
		// Refresh UI if streams change externally, e.g. a remote client changed what they sent
		ReplicationClient->OnModelChanged().AddSP(this, &SReplicationClientView::OnModelChanged);

		CVarReplicationClientViewMode->OnChangedDelegate().AddSP(this, &SReplicationClientView::OnConsoleVariableChanged);
	}

	TSharedRef<SWidget> SReplicationClientView::CreateEditorContent()
	{
		Content = SNew(SBox);
		RebuildContent();
		return Content.ToSharedRef();
	}

	void SReplicationClientView::RebuildContent()
	{
		FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);

		// Only one widget should be visible at the time so they do not interfere with each other.
		switch (CVarReplicationClientViewMode.GetValueOnGameThread())
		{
		case 0:
			Content->SetContent(CreateThreeSectionedContent(*ReplicationClient));
			break;
		case 1:
			Content->SetContent(CreateTwoSectionedContent(*ReplicationClient));
			break;

		default:
			Content->SetContent(SNullWidget::NullWidget);
		}
	}

	TSharedRef<SWidget> SReplicationClientView::CreateThreeSectionedContent(FReplicationClient& InReplicationClient)
	{
		using namespace ConcertClientSharedSlate;
		IReplicationStreamModel& PropertyModel = *InReplicationClient.GetClientEditModel();
		FAuthorityChangeTracker& AuthorityTracker = InReplicationClient.GetAuthorityDiffer();
		ISubmissionWorkflow& SubmissionWorkflow = InReplicationClient.GetSubmissionWorkflow();
		FGlobalAuthorityCache& AuthorityCache = ClientManager->GetAuthorityCache();
		
		const TAttribute<const IReplicationStreamViewer*> GetReplicationViewerAttribute =
			TAttribute<const IReplicationStreamViewer*>::CreateLambda([this](){ return EditorView_TwoSectioned.Get(); });
		TAttribute<const FConcertReplicationEditorSettings*> ReplicationSettingsAttribute =
			TAttribute<const FConcertReplicationEditorSettings*>::CreateLambda([](){ return &UMultiUserReplicationSettings::Get()->ReplicationEditorSettings; });

		// Add checkboxes in front of top level and subobject rows for changing authority
		const FCreateSubobjectViewParams SubobjectViewParams
		{
			.AdditionalColumns =
			{
				SingleClientColumns::ToggleSubobjectAuthority(AuthorityTracker, SubmissionWorkflow),
				SingleClientColumns::ConflictWarningForSubobject(ConcertClient.ToSharedRef(), AuthorityCache, InReplicationClient.GetEndpointId()),
				SingleClientColumns::OwnerOfSubobject(ConcertClient.ToSharedRef(), AuthorityCache)
			}
		};
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			.DataModel = InReplicationClient.GetClientEditModel(),
			.ObjectSource = MakeShared<FActorSelectionSourceModel>(),
			.PropertySource = MakeShared<FSelectPropertyFromUClassModel>(),
			.IsEditingEnabled = TAttribute<bool>::CreateLambda([&SubmissionWorkflow](){ return SubmissionWorkflow.GetUploadability() != EChangeUploadability::NotImplemented; }),
			.EditingDisabledToolTipText = LOCTEXT("Editing.NotImplemented", "Editing remote clients is not implemented. You can only edit the local client."),
			.ReplicationSettingsAttribute = MoveTemp(ReplicationSettingsAttribute),
			.ViewerParams =
			{
				.SubobjectView = CreateUnrealEditorSubobjectView(SubobjectViewParams),
				.AdditionalObjectColumns =
				{
					SingleClientColumns::ToggleTopLevelAuthority(PropertyModel, AuthorityTracker, SubmissionWorkflow),
					SingleClientColumns::OwnerOfTopLevelObject(ConcertClient.ToSharedRef(), AuthorityCache, PropertyModel)
				},
				.AdditionalPropertyColumns =
				{
					SingleClientColumns::OwnerOfProperty(ConcertClient.ToSharedRef(), AuthorityCache, GetReplicationViewerAttribute),
					SingleClientColumns::ConflictWarningForProperty(ConcertClient.ToSharedRef(), GetReplicationViewerAttribute, AuthorityCache, InReplicationClient.GetEndpointId())
				}
			}
		};

		EditorView_ThreeSectioned = CreateDefaultStreamEditor(ReplicationEditorCreationParams);
		return EditorView_ThreeSectioned.ToSharedRef();
	}

	TSharedRef<SWidget> SReplicationClientView::CreateTwoSectionedContent(FReplicationClient& InReplicationClient)
	{
		using namespace ConcertClientSharedSlate;
		FAuthorityChangeTracker& AuthorityTracker = InReplicationClient.GetAuthorityDiffer();
		ISubmissionWorkflow& SubmissionWorkflow = InReplicationClient.GetSubmissionWorkflow();
		FGlobalAuthorityCache& AuthorityCache = ClientManager->GetAuthorityCache();
		
		const TAttribute<const IReplicationStreamViewer*> GetReplicationViewerAttribute =
			TAttribute<const IReplicationStreamViewer*>::CreateLambda([this](){ return EditorView_TwoSectioned.Get(); });
		TAttribute<const FConcertReplicationEditorSettings*> ReplicationSettingsAttribute =
			TAttribute<const FConcertReplicationEditorSettings*>::CreateLambda([](){ return &UMultiUserReplicationSettings::Get()->ReplicationEditorSettings; });

		// Add checkboxes in front of top level and subobject rows for changing authority
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			.DataModel = InReplicationClient.GetClientEditModel(),
			.ObjectSource = MakeShared<FActorSelectionSourceModel>(),
			.PropertySource = MakeShared<FSelectPropertyFromUClassModel>(),
			.IsEditingEnabled = TAttribute<bool>::CreateLambda([&SubmissionWorkflow](){ return SubmissionWorkflow.GetUploadability() != EChangeUploadability::NotImplemented; }),
			.EditingDisabledToolTipText = LOCTEXT("Editing.NotImplemented", "Editing remote clients is not implemented. You can only edit the local client."),
			.ReplicationSettingsAttribute = MoveTemp(ReplicationSettingsAttribute),
			.ViewerParams =
			{
				.SubobjectModel = CreateDefaultComponentHierarchySubobjectModel(), // This makes actors have children in the top view
				.AdditionalObjectColumns =
				{
					// TODO DP: When we decide which UI version to choose, remove subobject from these function names.
					SingleClientColumns::ToggleSubobjectAuthority(AuthorityTracker, SubmissionWorkflow),
					SingleClientColumns::ConflictWarningForSubobject(ConcertClient.ToSharedRef(), AuthorityCache, InReplicationClient.GetEndpointId()),
					SingleClientColumns::OwnerOfSubobject(ConcertClient.ToSharedRef(), AuthorityCache)
				},
				.AdditionalPropertyColumns =
				{
					SingleClientColumns::OwnerOfProperty(ConcertClient.ToSharedRef(), AuthorityCache, GetReplicationViewerAttribute),
					SingleClientColumns::ConflictWarningForProperty(ConcertClient.ToSharedRef(), GetReplicationViewerAttribute, AuthorityCache, InReplicationClient.GetEndpointId())
				},
			}
		};

		EditorView_TwoSectioned = CreateDefaultStreamEditor(ReplicationEditorCreationParams);
		return EditorView_TwoSectioned.ToSharedRef();
	}

	void SReplicationClientView::OnModelChanged() const
	{
		// Only one is valid at the same time
		if (EditorView_TwoSectioned)
		{
			EditorView_TwoSectioned->Refresh();
		}
		if (EditorView_ThreeSectioned)
		{
			EditorView_ThreeSectioned->Refresh();
		}
	}

	void SReplicationClientView::OnConsoleVariableChanged(IConsoleVariable* ConsoleVariable)
	{
		RebuildContent();
	}

	void SReplicationClientView::EnumerateReplicatedObjects(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const
	{
		const FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		ReplicationClient->GetClientEditModel()->ForEachReplicatedObject([&Consumer](const FSoftObjectPath& Object)
		{
			Consumer(Object);
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE