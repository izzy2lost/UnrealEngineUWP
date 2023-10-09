// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	class IReplicationEditorView;
}

namespace UE::MultiUserClient
{
	class FReplicationClient;
	
	/** Displays the contents of a client. */
	class SReplicationClientView : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationClientView)
		{}
			/** The client to depict. Should always return true. If the client is destroyed, so should this widget be. */
			SLATE_ATTRIBUTE(FReplicationClient*, GetReplicationClient)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:
		
		/** The editor view of the replication content. */
		TSharedPtr<ConcertClientSharedSlate::IReplicationEditorView> EditorView;
		
		/** The client to depict. Should always return true. If the client is destroyed, so should this widget be. */
		TAttribute<FReplicationClient*> GetReplicationClientAttribute;
		
		/** Called when any of the streams change. */
		void OnModelChanged() const;
	};
}

