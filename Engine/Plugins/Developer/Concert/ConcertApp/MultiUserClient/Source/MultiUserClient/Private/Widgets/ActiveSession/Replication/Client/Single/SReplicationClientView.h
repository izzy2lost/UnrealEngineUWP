// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class SBox;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamEditor;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
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

		/** Holds the dynamic content, which changes based on CVarReplicationClientViewMode  */
		TSharedPtr<SBox> Content;
		
		/** Used to rebuild Content */
		TSharedPtr<IConcertClient> ConcertClient;
		FReplicationClientManager* ClientManager;
		
		/** The editor view of the replication content. */
		TSharedPtr<ConcertClientSharedSlate::IReplicationStreamEditor> EditorView_TwoSectioned;
		/** The editor view of the replication content. */
		TSharedPtr<ConcertClientSharedSlate::IReplicationStreamEditor> EditorView_ThreeSectioned;
		
		/** The client to depict. Should always return true. If the client is destroyed, so should this widget be. */
		TAttribute<FReplicationClient*> GetReplicationClientAttribute;


		TSharedRef<SWidget> CreateEditorContent();
		void RebuildContent();
		TSharedRef<SWidget> CreateThreeSectionedContent(FReplicationClient& InReplicationClient);
		TSharedRef<SWidget> CreateTwoSectionedContent(FReplicationClient& InReplicationClient);
		
		/** Called when any of the streams change. */
		void OnModelChanged() const;
		
		void OnConsoleVariableChanged(IConsoleVariable* ConsoleVariable);
	};
}

