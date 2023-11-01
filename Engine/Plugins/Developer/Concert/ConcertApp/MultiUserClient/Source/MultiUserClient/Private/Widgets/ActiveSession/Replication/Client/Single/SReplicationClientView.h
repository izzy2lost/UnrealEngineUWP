// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamEditor;
}

namespace UE::MultiUserClient
{
	class FReplicationClient;
	class FReplicationClientManager;
	
	/** Displays the contents of a client. */
	class SReplicationClientView : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationClientView)
		{}
			/** The client to depict. Should always return something valid. If the client is destroyed, so should this widget be. */
			SLATE_ATTRIBUTE(FReplicationClient*, GetReplicationClient)
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<IConcertClient>& InClient, FReplicationClientManager& InClientManager);

	private:
		
		/** The editor view of the replication content. */
		TSharedPtr<ConcertClientSharedSlate::IReplicationStreamEditor> EditorView;
		
		/** The client to depict. Should always return true. If the client is destroyed, so should this widget be. */
		TAttribute<FReplicationClient*> GetReplicationClientAttribute;
		
		/** Called when any of the streams change. */
		void OnModelChanged() const;
	};
}

