// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class FMenuBuilder;

namespace UE::ConcertSharedSlate
{
	class IObjectHierarchyModel;
	class IMultiReplicationStreamEditor;
	class IEditableReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FMultiStreamModel;
	class FReplicationClient;
	class FReplicationClientManager;
	class IClientSelectionModel;
	class SClientToolbar;

	/** Displays a selection of clients. */
	class SMultiClientView
		: public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SMultiClientView)
		{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager, IClientSelectionModel& InDisplayClientsModel);
		virtual ~SMultiClientView() override;

	private:

		/** Used to access and update the replication status widget. */
		TSharedPtr<SClientToolbar> Toolbar;

		FReplicationClientManager* ClientManager = nullptr;
		IClientSelectionModel* SelectionModel = nullptr;
		
		/** Combines the clients */
		TSharedPtr<FMultiStreamModel> StreamModel;
		/** Displayed in the UI. */
		TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor> StreamEditor;
		/** Used by widgets in columns. */
		TSharedPtr<ConcertSharedSlate::IObjectHierarchyModel> ObjectHierarchy;

		/** Creates this widget's editor content */
		TSharedRef<SWidget> CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FReplicationClientManager& InClientManager);

		// SClientToolbar attributes
		TSet<FGuid> GetDisplayClientIds() const;
		void EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer);
		
		void RebuildClientSubscriptions();
		void CleanClientSubscriptions();
		void OnClientChanged(FGuid Guid);
		
		/** Adds additional entries to the context menu for the object tree view. */
		void ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const;
	};
}
