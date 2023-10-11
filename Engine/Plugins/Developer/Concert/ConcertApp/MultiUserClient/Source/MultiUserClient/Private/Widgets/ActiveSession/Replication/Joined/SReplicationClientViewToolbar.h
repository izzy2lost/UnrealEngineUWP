// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	class FLocalStreamChangeTracker;

	/**
	 * Contains a bunch of actions that can be performed on client view.
	 *
	 * Layout:
	 * |                  AdditionalToolbarWidgets | Submit | Revert |
	 */
	class SReplicationClientViewToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SReplicationClientViewToolbar)
		{}
			/** Used to submit and revert locally made changes. */
			SLATE_ATTRIBUTE(FLocalStreamChangeTracker*, GetChangeTrackerAttribute)

			/** Placed towards the left of the buttons, which are on the right side of the toolbar. */
			SLATE_NAMED_SLOT(FArguments, AdditionalToolbarWidgets)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:
		
		/** Used to submit and revert locally made changes. */
		TAttribute<FLocalStreamChangeTracker*> GetChangeTrackerAttribute;

		/** Check()s that the GetChangeTrackerAttribute is valid. */
		FLocalStreamChangeTracker& GetChangeTracker() const;
		
		TSharedRef<SWidget> BuildUploadButton();
		FText GetUploadButtonToolTipText() const;
		FReply OnUploadButtonClicked() const;
		
		TSharedRef<SWidget> BuildRevertButton();
	};
}

