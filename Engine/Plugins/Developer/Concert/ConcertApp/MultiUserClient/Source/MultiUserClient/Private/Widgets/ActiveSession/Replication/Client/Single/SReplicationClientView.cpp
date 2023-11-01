// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationClientView.h"

#include "SingleClientColumns.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Widgets/ActiveSession/Replication/Client/Single/SSingleClientToolbar.h"

#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SReplicationClientView"

namespace UE::MultiUserClient
{
	void SReplicationClientView::Construct(const FArguments& InArgs, const TSharedRef<IConcertClient>& InClient, FReplicationClientManager& InClientManager)
	{
		GetReplicationClientAttribute = InArgs._GetReplicationClient;
		FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);

		ConcertClientSharedSlate::IObjectToPropertiesModel& PropertyModel = *ReplicationClient->GetClientEditModel();
		FAuthorityChangeTracker& AuthorityTracker = ReplicationClient->GetAuthorityDiffer();
		ISubmissionWorkflow& SubmissionWorkflow = ReplicationClient->GetSubmissionWorkflow();
		FGlobalAuthorityCache& AuthorityCache = InClientManager.GetAuthorityCache();
		TAttribute<const ConcertClientSharedSlate::IReplicationStreamViewer*> GetReplicationViewerAttribute =
			TAttribute<const ConcertClientSharedSlate::IReplicationStreamViewer*>::CreateLambda([this](){ return EditorView.Get(); });

		// Add checkboxes in front of top level and subobject rows for changing authority
		using namespace ConcertClientSharedSlate;
		const FCreateSubobjectViewParams SubobjectViewParams
		{
			.AdditionalColumns =
			{
				SingleClientColumns::ToggleSubobjectAuthority(AuthorityTracker, SubmissionWorkflow),
				SingleClientColumns::OwnerOfSubobject(InClient, AuthorityCache)
			}
		};
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			.DataModel = ReplicationClient->GetClientEditModel(),
			.ObjectSource = MakeShared<FEditorObjectSelectionSourceModel>(),
			.PropertySource = MakeShared<FSelectPropertyFromUClassModel>(),
			.SubobjectView = CreateUnrealEditorSubobjectView(SubobjectViewParams),
			.AdditionalObjectColumns =
			{
				SingleClientColumns::ToggleTopLevelAuthority(PropertyModel, AuthorityTracker, SubmissionWorkflow),
				SingleClientColumns::OwnerOfTopLevelObject(InClient, AuthorityCache, PropertyModel)
			},
			.AdditionalPropertyColumns =
			{
				SingleClientColumns::OwnerOfProperty(InClient, AuthorityCache, GetReplicationViewerAttribute)
			},
			.IsEditingEnabled = TAttribute<bool>::CreateLambda([&SubmissionWorkflow](){ return SubmissionWorkflow.GetUploadability() != EChangeUploadability::NotImplemented; }),
			.EditingDisabledToolTipText = LOCTEXT("Editing.NotImplemented", "Editing remote clients is not implemented. You can only edit the local client.")
		};

		EditorView = CreateDefaultStreamEditor(ReplicationEditorCreationParams);
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SSingleClientToolbar)
				.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				EditorView.ToSharedRef()
			]
		];
		
		// Refresh UI if streams change externally, e.g. a remote client changed what they sent
		ReplicationClient->OnModelExternallyChanged().AddSP(this, &SReplicationClientView::OnModelChanged);
	}
	
	void SReplicationClientView::OnModelChanged() const
	{
		EditorView->Refresh();
	}
}

#undef LOCTEXT_NAMESPACE