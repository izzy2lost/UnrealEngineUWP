// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiClientView.h"

#include "Selection/ISelectionModel.h"
#include "MultiStreamModel.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/Object/IObjectNameModel.h"
#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IMultiObjectPropertyAssignmentView.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"
#include "Replication/Stream/Discovery/MultiUserStreamExtender.h"
#include "Widgets/ActiveSession/Replication/Client/Context/ContextMenuUtils.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/ActiveSession/Replication/Client/PropertySelection/SPropertySelectionComboButton.h"
#include "Widgets/ActiveSession/Replication/Client/SPresetComboButton.h"
#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMultiClientView"

namespace UE::MultiUserClient::Replication
{
	void SMultiClientView::Construct(
		const FArguments&,
		TSharedRef<IConcertClient> InConcertClient,
		FMultiUserReplicationManager& InMultiUserReplicationManager,
		IOnlineClientSelectionModel& InOnlineClientSelectionModel
		)
	{
		ClientManager = InMultiUserReplicationManager.GetClientManager();
		UserSelectedProperties = InMultiUserReplicationManager.GetUserPropertySelector();
		StreamModel = MakeShared<FMultiStreamModel>(InOnlineClientSelectionModel, *ClientManager);
		OnlineClientSelectionModel = &InOnlineClientSelectionModel;
		ConcertClient = MoveTemp(InConcertClient);
		
		ClientManager->OnRemoteClientsChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		OnlineClientSelectionModel->OnSelectionChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		UserSelectedProperties->OnPropertySelectionChanged().AddRaw(this, &SMultiClientView::RefreshUI);

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
			.ReplicatableClients(this, &SMultiClientView::GetReplicatableClientIds)
			.ForEachObjectInStream(this, &SMultiClientView::EnumerateObjectsInStreams)
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
		UserSelectedProperties->OnPropertySelectionChanged().RemoveAll(this);
	}

	TSharedRef<SWidget> SMultiClientView::CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FMultiUserReplicationManager& InMultiUserReplicationManager)
	{
		using namespace UE::ConcertSharedSlate;

		FMuteStateManager& MuteManager = *InMultiUserReplicationManager.GetMuteManager();
		FUserPropertySelector& PropertySelector = *InMultiUserReplicationManager.GetUserPropertySelector();
		
		ObjectHierarchy = ConcertClientSharedSlate::CreateObjectHierarchyForComponentHierarchy();
		const TSharedRef<IObjectNameModel> NameModel = ConcertClientSharedSlate::CreateEditorObjectNameModel();
		
		TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditorAttribute =
		   TAttribute<TSharedPtr<IMultiReplicationStreamEditor>>::CreateLambda([this]()
		   {
			   return StreamEditor;
		   });
		FGetAutoAssignTarget GetAutoAssignTargetDelegate = FGetAutoAssignTarget::CreateLambda([this](TConstArrayView<UObject*>)
		{
			const TSharedRef<IEditableReplicationStreamModel>& LocalStream = ClientManager->GetLocalClient().GetClientEditModel();
			return StreamModel->GetEditableStreams().Contains(LocalStream) ? LocalStream.ToSharedPtr() : nullptr;
		});
		
		FCreatePropertyTreeViewParams TreeViewParams
		{
			.PropertyColumns =
			{
				ReplicationColumns::Property::LabelColumn(),
				MultiStreamColumns::AssignPropertyColumn(MultiStreamEditorAttribute, InConcertClient, *ClientManager)
			},
			.CreateCategoryRow = CreateDefaultCategoryGenerator(NameModel),
		};
		TreeViewParams.LeftOfPropertySearchBar.Widget = SAssignNew(PropertySelectionButton, SPropertySelectionComboButton, PropertySelector)
			.GetObjectDisplayString_Lambda([NameModel](const TSoftObjectPtr<>& Object){ return NameModel->GetObjectDisplayName(Object); });
		TreeViewParams.NoItemsContent.Widget = CreateNoPropertiesWarning();
		const TSharedRef<IPropertyTreeView> PropertyTreeView = CreateSearchablePropertyTreeView(MoveTemp(TreeViewParams));
		
		const TSharedRef<IPropertySourceProcessor> PropertySourceModel = PropertySelector.GetPropertySourceProcessor();
		PropertyAssignmentView = CreateMultiObjectAssignmentView(
			{ .PropertyTreeView = PropertyTreeView, .ObjectHierarchy = ObjectHierarchy, .PropertySource = PropertySourceModel}
			);
		PropertyAssignmentView->OnObjectGroupsChanged().AddLambda([this]()
		{
			PropertySelectionButton->RefreshSelectableProperties(PropertyAssignmentView->GetDisplayedGroups());
		});
		
		
		FCreateMultiStreamEditorParams Params
		{
			.MultiStreamModel = StreamModel.ToSharedRef(),
			.ConsolidatedObjectModel = ConcertClientSharedSlate::CreateTransactionalStreamModel(),
			.ObjectSource = MakeShared<ConcertClientSharedSlate::FActorSelectionSourceModel>(),
			.PropertySource = PropertySourceModel,
			.GetAutoAssignToStreamDelegate = MoveTemp(GetAutoAssignTargetDelegate),
			.OnPreAddSelectedObjectsDelegate = FSelectObjectsFromComboButton::CreateSP(this, &SMultiClientView::OnPreAddObjectsFromComboButton),
			.OnPostAddSelectedObjectsDelegate = FSelectObjectsFromComboButton::CreateSP(this, &SMultiClientView::OnPostAddObjectsFromComboButton),
		};
		FCreateViewerParams ViewerParams
		{
			.PropertyAssignmentView = PropertyAssignmentView.ToSharedRef(),
			// .ObjectHierarchy Do not assign so we only show the actors
			.NameModel = NameModel, // This makes actors use their labels, and components use the names given in the BP editor
			.OnExtendObjectsContextMenu = FExtendObjectMenu::CreateSP(this, &SMultiClientView::ExtendObjectContextMenu),
			.ObjectColumns =
			{
				MultiStreamColumns::MuteToggleColumn(MuteManager.GetChangeTracker()),
				MultiStreamColumns::AssignedClientsColumn(InConcertClient, MultiStreamEditorAttribute, *ObjectHierarchy, ClientManager->GetReassignmentLogic(), *ClientManager)
			},
			.ShouldDisplayObjectDelegate = FShouldDisplayObject::CreateSP(this, &SMultiClientView::ShouldDisplayObject),
		};
		ViewerParams.RightOfObjectSearchBar.Widget = SNew(SPresetComboButton, *InConcertClient, *InMultiUserReplicationManager.GetPresetManager());
		StreamEditor = CreateBaseMultiStreamEditor(MoveTemp(Params), MoveTemp(ViewerParams));
		check(StreamEditor);
		
		return StreamEditor.ToSharedRef();
	}

	TSharedRef<SWidget> SMultiClientView::CreateNoPropertiesWarning() const
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoProperties", "Use Edit button to add replicated properties"))
			];
	}

	TSet<FGuid> SMultiClientView::GetReplicatableClientIds() const
	{
		TSet<FGuid> ClientIds;
		StreamModel->ForEachClient([&ClientIds](const FOnlineClient* Client)
		{
			ClientIds.Add(Client->GetEndpointId());
			return EBreakBehavior::Continue;
		});
		return ClientIds;
	}

	void SMultiClientView::EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const
	{
		StreamModel->ForEachClient([&Consumer](const FOnlineClient* Client)
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

		OnlineClientSelectionModel->ForEachItem([this](FOnlineClient& Client)
		{
			Client.OnModelChanged().AddSP(this, &SMultiClientView::RefreshUI);
			Client.OnHierarchyNeedsRefresh().AddRaw(this, &SMultiClientView::RefreshUI);
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::CleanClientSubscriptions() const
	{
		ClientManager->ForEachClient([this](FOnlineClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			Client.OnHierarchyNeedsRefresh().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::RefreshUI() const
	{
		StreamEditor->GetEditorBase().Refresh();
	}

	void SMultiClientView::ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<TSoftObjectPtr<>> ContextObjects) const
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

	void SMultiClientView::OnPreAddObjectsFromComboButton(TArrayView<const ConcertSharedSlate::FSelectableObjectInfo>)
	{
		// When the user adds using the combo button, automatically add discover relevant objects and properties
		ClientManager->GetLocalClient().GetStreamExtender()
			.SetShouldExtend(true);
	}

	void SMultiClientView::OnPostAddObjectsFromComboButton(TArrayView<const ConcertSharedSlate::FSelectableObjectInfo>)
	{
		ClientManager->GetLocalClient().GetStreamExtender()
			.SetShouldExtend(false);
	}
}

#undef LOCTEXT_NAMESPACE