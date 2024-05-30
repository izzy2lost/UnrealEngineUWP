// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiClientView.h"

#include "IClientSelectionModel.h"
#include "MultiStreamModel.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"
#include "Widgets/ActiveSession/Replication/Client/Context/ContextMenuUtils.h"

#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SMultiClientView"

namespace UE::MultiUserClient
{
	void SMultiClientView::Construct(
		const FArguments&,
		TSharedRef<IConcertClient> InConcertClient,
		FMultiUserReplicationManager& InMultiUserReplicationManager,
		IClientSelectionModel& InDisplayClientsModel
		)
	{
		ClientManager = InMultiUserReplicationManager.GetClientManager();
		StreamModel = MakeShared<FMultiStreamModel>(InDisplayClientsModel, *ClientManager);

		ConcertClient = MoveTemp(InConcertClient);
		ClientManager->OnRemoteClientsChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		SelectionModel = &InDisplayClientsModel;
		SelectionModel->OnSelectionChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);

		TSharedPtr<SVerticalBox> Content;
		ChildSlot
		[
			SAssignNew(Content, SVerticalBox)

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				CreateEditorContent(ConcertClient.ToSharedRef(), InMultiUserReplicationManager)
			]
		];
		
		SReplicationStatus::AppendReplicationStatus(*Content, ClientManager->GetAuthorityCache(),
			SReplicationStatus::FArguments()
			.DisplayedClients(this, &SMultiClientView::GetDisplayClientIds)
			.ForEachReplicatedObject(this, &SMultiClientView::EnumerateObjectsInStreams)
			);

		RebuildClientSubscriptions();

		// Changing worlds affects what things are displayed in the editor.
		HideObjectsNotInEditorWorld.OnRefreshObjects().AddLambda([this]()
		{
			StreamEditor->GetEditorBase().Refresh();
		});
	}

	SMultiClientView::~SMultiClientView()
	{
		ClientManager->OnRemoteClientsChanged().RemoveAll(this);
		CleanClientSubscriptions();
	}

	TSharedRef<SWidget> SMultiClientView::CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FMultiUserReplicationManager& InMultiUserReplicationManager)
	{
		using namespace UE::ConcertSharedSlate;

		FMuteStateManager& MuteManager = *InMultiUserReplicationManager.GetMuteManager();
		
		TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditorAttribute =
		   TAttribute<TSharedPtr<IMultiReplicationStreamEditor>>::CreateLambda([this]()
		   {
			   return StreamEditor;
		   });
		const TAttribute<IObjectHierarchyModel*> ObjectHierarchyAttribute =
		   TAttribute<IObjectHierarchyModel*>::CreateLambda([this]()
		   {
			   return ObjectHierarchy.Get();
		   });
		FGetAutoAssignTarget GetAutoAssignTargetDelegate = FGetAutoAssignTarget::CreateLambda([this](TConstArrayView<UObject*>)
		{
			const TSharedRef<IEditableReplicationStreamModel>& LocalStream = ClientManager->GetLocalClient().GetClientEditModel();
			return StreamModel->GetEditableStreams().Contains(LocalStream) ? LocalStream.ToSharedPtr() : nullptr;
		});
		const TSharedRef<ConcertClientSharedSlate::FSelectPropertyFromUClassModel> PropertySourceModel = MakeShared<ConcertClientSharedSlate::FSelectPropertyFromUClassModel>();
		
		ConcertClientSharedSlate::FFilterablePropertyTreeViewParams TreeViewParams
		{
			.AdditionalPropertyColumns =
			{
				ReplicationColumns::Property::LabelColumn(),
				MultiStreamColumns::AssignPropertyColumn(MultiStreamEditorAttribute, InConcertClient, *ClientManager)
			}
		};
		TSharedRef<IPropertyTreeView> PropertyTreeView = CreateFilterablePropertyTreeView(MoveTemp(TreeViewParams));
		TSharedRef<IPropertyAssignmentView> PropertyAssignmentView = CreatePerObjectAssignmentView({ .PropertyTreeView = PropertyTreeView, .PropertySource = PropertySourceModel });
		
		FCreateMultiStreamEditorParams Params
		{
			.MultiStreamModel = StreamModel.ToSharedRef(),
			.ConsolidatedObjectModel = ConcertClientSharedSlate::CreateTransactionalStreamModel(),
			.ObjectSource = MakeShared<ConcertClientSharedSlate::FActorSelectionSourceModel>(),
			.PropertySource = PropertySourceModel,
			.GetAutoAssignToStreamDelegate = MoveTemp(GetAutoAssignTargetDelegate)
		};
		
		ObjectHierarchy = ConcertClientSharedSlate::CreateObjectHierarchyForComponentHierarchy();
		FCreateViewerParams ViewerParams
		{
			.PropertyAssignmentView = MoveTemp(PropertyAssignmentView),
			.ObjectHierarchy = ObjectHierarchy, // This makes actors have children in the top view
			.NameModel = ConcertClientSharedSlate::CreateEditorObjectNameModel(), // This makes actors use their labels, and components use the names given in the BP editor
			.OnExtendObjectsContextMenu = FExtendObjectMenu::CreateSP(this, &SMultiClientView::ExtendObjectContextMenu),
			.ObjectColumns =
			{
				MultiStreamColumns::MuteToggleColumn(MuteManager.GetChangeTracker()),
				MultiStreamColumns::AssignedClientsColumn(InConcertClient, MultiStreamEditorAttribute, ObjectHierarchyAttribute, ClientManager->GetReassignmentLogic(), *ClientManager)
			},
			.ShouldDisplayObjectDelegate = FShouldDisplayObject::CreateSP(this, &SMultiClientView::ShouldDisplayObject)
		};
		
		StreamEditor = CreateBaseMultiStreamEditor(MoveTemp(Params), MoveTemp(ViewerParams));
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

	void SMultiClientView::EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const
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
			if (SelectionModel->ContainsClient(Client.GetEndpointId()))
			{
				Client.OnModelChanged().AddSP(this, &SMultiClientView::OnClientChanged);
				Client.OnHierarchyNeedsRefresh().AddRaw(this, &SMultiClientView::OnHierarchyNeedsRefresh);
			}
			
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::CleanClientSubscriptions() const
	{
		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			Client.OnHierarchyNeedsRefresh().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::OnClientChanged() const
	{
		// When reassignment operations complete, the content of the columns changes so a resort is required.
		StreamEditor->GetEditorBase().RequestObjectColumnResort(MultiStreamColumns::AssignedClientsColumnId);
		StreamEditor->GetEditorBase().RequestPropertyColumnResort(MultiStreamColumns::AssignPropertyColumnId);
	}

	void SMultiClientView::OnHierarchyNeedsRefresh() const
	{
		// It's a bit excessive to refresh all objects when the hierarchy might have changed but it's simple (and only happens once at end of tick)
		StreamEditor->GetEditorBase().Refresh();
	}

	void SMultiClientView::ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const
	{
		ContextMenuUtils::AddFrequencyOptionsIfOneContextObject_MultiClient(MenuBuilder, ContextObjects, *ClientManager);

		if (ContextObjects.Num() == 1)
		{
			ContextMenuUtils::AddReassignmentOptions(
				MenuBuilder,
				ContextObjects[0],
				*ConcertClient,
				*ClientManager,
				*ObjectHierarchy,
				ClientManager->GetReassignmentLogic(),
				*StreamEditor
				);
		}
	}

	bool SMultiClientView::ShouldDisplayObject(const FSoftObjectPath& Object) const
	{
		return HideObjectsNotInEditorWorld.ShouldShowObject(Object);
	}
}

#undef LOCTEXT_NAMESPACE