// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationClientView.h"

#include "Replication/Client/ReplicationClient.h"
#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IReplicationEditorView.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Widgets/ActiveSession/Replication/Joined/SReplicationClientViewToolbar.h"

#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserClient
{
	void SReplicationClientView::Construct(const FArguments& InArgs)
	{
		GetReplicationClientAttribute = InArgs._GetReplicationClient;
		FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		using namespace ConcertClientSharedSlate;
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			ReplicationClient->GetClientEditModel(),
			MakeShared<FEditorObjectSelectionSourceModel>(),
			MakeShared<FSelectPropertyFromUClassModel>()
		};

		EditorView = CreateEditorForUnrealEditor(ReplicationEditorCreationParams);
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SReplicationClientViewToolbar)
				.GetChangeTrackerAttribute_Lambda([this](){ return &GetReplicationClientAttribute.Get()->GetDiffer(); })
				.AdditionalToolbarWidgets() [ InArgs._AdditionalToolbarWidgets.Widget ]
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
