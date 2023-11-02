// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	class IObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FStreamChangeTracker;
	class ISubmissionWorkflow;

	/**
	 * Contains a bunch of actions that can be performed on client view.
	 */
	class SSingleClientToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SSingleClientToolbar)
		{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		
			/** The clients to show statistics for */
			SLATE_ATTRIBUTE(TSet<FGuid>, DisplayedClients)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const ConcertClientSharedSlate::IObjectToPropertiesModel& InObjectModel, FGlobalAuthorityCache& InAuthorityCache);
	};
}

