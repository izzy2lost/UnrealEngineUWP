// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SReplicationStatus.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FStreamChangeTracker;
	class ISubmissionWorkflow;
	class SReplicationStatus;

	/**
	 * Contains a bunch of actions that can be performed on client view.
	 */
	class SClientToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SClientToolbar)
		{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		
			/** The clients to show statistics for */
			SLATE_ATTRIBUTE(TSet<FGuid>, DisplayedClients)
		
			/** Delegate which enumerates every replicated object. */
			SLATE_EVENT(FForEachReplicatedObject, ForEachReplicatedObject)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FGlobalAuthorityCache& InAuthorityCache);

		/** Updates the status text of how many objects are being replicated. */
		void RefreshStatusText();
		
	private:

		/** Displays how many objects are being replicated */
		TSharedPtr<SReplicationStatus> ReplicationStatus;
	};
}

