// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	class FStreamChangeTracker;
	class ISubmissionWorkflow;

	/**
	 * Contains a bunch of actions that can be performed on client view.
	 *
	 * Layout:
	 * | AdditionalToolbarWidgets                   | Submit | Revert |
	 */
	class SSingleClientToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SSingleClientToolbar)
		{}
			/** Manages submission and reverting of changes. Used for upload and revert buttons. */
			SLATE_ATTRIBUTE(ISubmissionWorkflow*, SubmissionWorkflow)

			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
	};
}

