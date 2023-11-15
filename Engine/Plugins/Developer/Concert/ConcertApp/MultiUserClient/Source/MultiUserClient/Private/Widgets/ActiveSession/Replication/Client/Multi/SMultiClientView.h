// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	class IClientSelectionModel;
}

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	class IMultiReplicationStreamEditor;
	class IEditableReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FMultiStreamModel;
	class FReplicationClient;
	class FReplicationClientManager;
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

	private:

		/** Used to access and update the replication status widget. */
		TSharedPtr<SClientToolbar> Toolbar;
		
		/** Combines the clients */
		TSharedPtr<FMultiStreamModel> StreamModel;
		/** Displayed in the UI. */
		TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor> StreamEditor;

		/** Creates this widget's editor content */
		TSharedRef<SWidget> CreateEditorContent(TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager);

		// SClientToolbar attributes
		TSet<FGuid> GetDisplayClientIds() const;
		void EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer);
	};
}
