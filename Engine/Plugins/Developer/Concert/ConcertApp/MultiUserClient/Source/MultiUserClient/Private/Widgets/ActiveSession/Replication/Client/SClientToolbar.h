// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FStreamChangeTracker;
	class ISubmissionWorkflow;

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
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const ConcertClientSharedSlate::IReplicationStreamModel& InObjectModel, FGlobalAuthorityCache& InAuthorityCache);
	};
}

