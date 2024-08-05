// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/ObjectSource/IObjectSourceModel.h"
#include "Replication/Editor/UnrealEditor/HideObjectsNotInWorldLogic.h"
#include "Selection/SelectionModelFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient::Replication
{
	class FUserPropertySelector;
}

class IConcertClient;
class FMenuBuilder;

namespace UE::ConcertSharedSlate
{
	class IMultiObjectPropertyAssignmentView;
	class IObjectHierarchyModel;
	class IMultiReplicationStreamEditor;
	class IEditableReplicationStreamModel;
}

namespace UE::MultiUserClient::Replication
{
	class FGlobalAuthorityCache;
	class FMultiStreamModel;
	class FMultiUserReplicationManager;
	class FOnlineClient;
	class FOnlineClientManager;
	class SPropertySelectionComboButton;

	/** Displays a selection of clients. */
	class SMultiClientView
		: public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SMultiClientView){}
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			TSharedRef<IConcertClient> InConcertClient,
			FMultiUserReplicationManager& InMultiUserReplicationManager UE_LIFETIMEBOUND,
			IOnlineClientSelectionModel& InOnlineClientSelectionModel UE_LIFETIMEBOUND
			);
		virtual ~SMultiClientView() override;

	private:

		TSharedPtr<IConcertClient> ConcertClient;
		FOnlineClientManager* ClientManager = nullptr;
		FUserPropertySelector* UserSelectedProperties = nullptr;
		IOnlineClientSelectionModel* OnlineClientSelectionModel = nullptr;
		
		/** Combines the clients */
		TSharedPtr<FMultiStreamModel> StreamModel;
		/** Displayed in the UI. */
		TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor> StreamEditor;
		/** Used by widgets in columns. */
		TSharedPtr<ConcertSharedSlate::IObjectHierarchyModel> ObjectHierarchy;

		/**
		 * This combo button is shown to the left of the search bar in the bottom half of the replication UI.
		 * It allows users to specify the properties they want to work on (i.e. these properties should be shown in the property view).
		 */
		TSharedPtr<SPropertySelectionComboButton> PropertySelectionButton;
		/** Displays the properties for the objects displayed in the top view. */
		TSharedPtr<ConcertSharedSlate::IMultiObjectPropertyAssignmentView> PropertyAssignmentView;

		/** This logic helps us decide whether an object should be displayed and lets us know that the object list needs to be refreshed (e.g. due to world change). */
		ConcertClientSharedSlate::FHideObjectsNotInWorldLogic HideObjectsNotInEditorWorld;

		/** Creates this widget's editor content */
		TSharedRef<SWidget> CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FMultiUserReplicationManager& InMultiUserReplicationManager);
		TSharedRef<SWidget> CreateNoPropertiesWarning() const;

		// SClientToolbar attributes
		/** @return Gets the clients that may be replicating */
		TSet<FGuid> GetReplicatableClientIds() const;
		/** Calls Consumer for each object path that is in a stream - independent of whether it is being replicated or not. */
		void EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const;
		
		void RebuildClientSubscriptions();
		void CleanClientSubscriptions() const;
		void RefreshUI() const;
		
		/** Adds additional entries to the context menu for the object tree view. */
		void ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<TSoftObjectPtr<>> ContextObjects) const;
		/** Decides whether the object should be displayed: do not show it if it's not in the editor world. */
		bool ShouldDisplayObject(const FSoftObjectPath& Object) const;
		
		void OnPreAddObjectsFromComboButton(TArrayView<const ConcertSharedSlate::FSelectableObjectInfo>);
		void OnPostAddObjectsFromComboButton(TArrayView<const ConcertSharedSlate::FSelectableObjectInfo>);
	};
}
