// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiClientView.h"

#include "IClientSelectionModel.h"
#include "MultiStreamModel.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"
#include "Widgets/ActiveSession/Replication/Client/FrequencyContextMenuUtils.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SClientToolbar.h"

#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SMultiClientView"

namespace UE::MultiUserClient
{
	void SMultiClientView::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager, IClientSelectionModel& InDisplayClientsModel)
	{
		StreamModel = MakeShared<FMultiStreamModel>(InDisplayClientsModel, InClientManager);

		ClientManager = &InClientManager;
		ClientManager->OnRemoteClientsChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		SelectionModel = &InDisplayClientsModel;
		SelectionModel->OnSelectionChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SAssignNew(Toolbar, SClientToolbar, InClientManager.GetAuthorityCache())
				.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
				.DisplayedClients(this, &SMultiClientView::GetDisplayClientIds)
				.ForEachReplicatedObject(this, &SMultiClientView::EnumerateObjectsInStreams)
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				CreateEditorContent(InConcertClient, InClientManager)
			]
		];

		RebuildClientSubscriptions();
	}

	SMultiClientView::~SMultiClientView()
	{
		ClientManager->OnRemoteClientsChanged().RemoveAll(this);
		CleanClientSubscriptions();
	}

	TSharedRef<SWidget> SMultiClientView::CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FReplicationClientManager& InClientManager)
	{
		using namespace UE::ConcertSharedSlate;

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
		FGetAutoAssignTarget GetAutoAssignTargetDelegate = FGetAutoAssignTarget::CreateLambda([this, &InClientManager](TConstArrayView<UObject*>)
		{
			const TSharedRef<IEditableReplicationStreamModel>& LocalStream = InClientManager.GetLocalClient().GetClientEditModel();
			return StreamModel->GetEditableStreams().Contains(LocalStream) ? LocalStream.ToSharedPtr() : nullptr;
		});
		
		FCreateMultiStreamEditorParams Params
		{
			.MultiStreamModel = StreamModel.ToSharedRef(),
			.ConsolidatedObjectModel = ConcertClientSharedSlate::CreateTransactionalStreamModel(),
			.ObjectSource = MakeShared<ConcertClientSharedSlate::FActorSelectionSourceModel>(),
			.PropertySource = MakeShared<FSelectPropertyFromUClassModel>(),
			.GetAutoAssignToStreamDelegate = MoveTemp(GetAutoAssignTargetDelegate),
			.ViewerParams 
			{
				.SubobjectModel = ConcertClientSharedSlate::CreateSubobjectModelForComponentHierarchy(), // This makes actors have children in the top view
				.NameModel = ConcertClientSharedSlate::CreateEditorObjectNameModel(), // This makes actors use their labels, and components use the names given in the BP editor
				.OnExtendObjectsContextMenu = FExtendObjectMenu::CreateSP(this, &SMultiClientView::ExtendObjectContextMenu),
				.AdditionalObjectColumns =
				{
					MultiStreamColumns::ReplicationToggle(InConcertClient, ConsolidatedStreamModelAttribute, InClientManager),
					MultiStreamColumns::ReassignOwnership(InConcertClient, MultiStreamEditorAttribute, InClientManager.GetReassignmentLogic(), InClientManager)
				},
				.AdditionalPropertyColumns = { MultiStreamColumns::AssignPropertyColumn(MultiStreamEditorAttribute, InConcertClient, InClientManager) },
				.PrimaryPropertySort = { MultiStreamColumns::AssignPropertyColumnId, EColumnSortMode::Ascending}
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

	void SMultiClientView::RebuildClientSubscriptions()
	{
		CleanClientSubscriptions();

		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().AddSP(this, &SMultiClientView::OnClientChanged, Client.GetEndpointId());
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::CleanClientSubscriptions()
	{
		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::OnClientChanged(FGuid)
	{
		// When reassignment operations complete, the content of the columns changes so a resort is required.
		StreamEditor->GetEditorBase().RequestObjectColumnResort(MultiStreamColumns::ReassignOwnershipColumnId);
		StreamEditor->GetEditorBase().RequestPropertyColumnResort(MultiStreamColumns::AssignPropertyColumnId);
	}

	void SMultiClientView::ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const
	{
		FrequencyContextMenuUtils::AddFrequencyOptionsIfOneContextObject_MultiClient(MenuBuilder, ContextObjects, *ClientManager);
	}
}

#undef LOCTEXT_NAMESPACE