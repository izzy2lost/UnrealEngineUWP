// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiClientView.h"

#include "MultiStreamModel.h"
#include "MultiUserReplicationSettings.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/Object/ActorSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SClientToolbar.h"

#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SMultiClientView"

namespace UE::MultiUserClient
{
	void SMultiClientView::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& ClientManager, IClientSelectionModel& InDisplayClientsModel)
	{
		TAttribute<const FConcertReplicationEditorSettings*> ReplicationSettingsAttribute =
			TAttribute<const FConcertReplicationEditorSettings*>::CreateLambda([]()
			{
				return &UMultiUserReplicationSettings::Get()->ReplicationEditorSettings;
			});
		// TODO DP:
		StreamModel = MakeShared<FMultiStreamModel>(InDisplayClientsModel, ClientManager);

		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SAssignNew(Toolbar, SClientToolbar, ClientManager.GetAuthorityCache())
				.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
				.DisplayedClients(this, &SMultiClientView::GetDisplayClientIds)
				.ForEachReplicatedObject(this, &SMultiClientView::EnumerateObjectsInStreams)
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				CreateEditorContent(InConcertClient, ClientManager)
			]
		];
	}

	TSharedRef<SWidget> SMultiClientView::CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FReplicationClientManager& InClientManager)
	{
		using namespace UE::ConcertClientSharedSlate;

		TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditorAttribute =
		   TAttribute<TSharedPtr<IMultiReplicationStreamEditor>>::CreateLambda([this]()
		   {
			   return StreamEditor;
		   });
		const TAttribute<IReplicationStreamModel*> ConsolidatedStreamModelAttribute =
		   TAttribute<IReplicationStreamModel*>::CreateLambda([this]()
		   {
			   return &StreamEditor->GetConsolidatedModel();
		   });
		
		FCreateMultiStreamEditorParams Params
		{
			.MultiStreamModel = StreamModel.ToSharedRef(),
			.ObjectSource = MakeShared<FActorSelectionSourceModel>(),
			.PropertySource = MakeShared<FSelectPropertyFromUClassModel>(),
			.ViewerParams =
			{
				.SubobjectModel = CreateDefaultComponentHierarchySubobjectModel(), // This makes actors have children in the top view
				.AdditionalObjectColumns =
				{
					MultiStreamColumns::ReplicationToggle(InConcertClient, ConsolidatedStreamModelAttribute, InClientManager),
					MultiStreamColumns::ReassignOwnership(InConcertClient, ConsolidatedStreamModelAttribute, InClientManager.GetReassignmentLogic(), InClientManager)
				},
				.AdditionalPropertyColumns = { MultiStreamColumns::AssignPropertyColumn(MoveTemp(MultiStreamEditorAttribute), InConcertClient, InClientManager) }
			}
		};
		StreamEditor = CreateBaseMultiStreamEditor(MoveTemp(Params));
		check(StreamEditor);
		return StreamEditor.ToSharedRef();
	}

	TSet<FGuid> SMultiClientView::GetDisplayClientIds() const
	{
		TSet<FGuid> ClientIds;
		StreamModel->ForEachClient([&ClientIds](const FReplicationClient* Client)
		{
			ClientIds.Add(Client->GetEndpointId());
			return EBreakBehavior::Continue;
		});
		return ClientIds;
	}

	void SMultiClientView::EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer)
	{
		StreamModel->ForEachClient([&Consumer](const FReplicationClient* Client)
		{
			Client->GetClientEditModel()->ForEachReplicatedObject([&Consumer](const FSoftObjectPath& Object)
			{
				Consumer(Object);
				return EBreakBehavior::Continue;
			});
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE