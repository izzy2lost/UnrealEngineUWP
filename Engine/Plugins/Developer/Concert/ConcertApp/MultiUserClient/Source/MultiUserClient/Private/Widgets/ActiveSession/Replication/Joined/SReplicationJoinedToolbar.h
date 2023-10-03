// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;

	/** Contains a bunch of actions that can be performed while connected to replication. */
	class SReplicationJoinedToolbar : public SCompoundWidget
	{
	public:
		
		SLATE_BEGIN_ARGS(SReplicationJoinedToolbar)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager);

	private:

		/** Acts as the model of this view */
		TSharedPtr<FMultiUserReplicationManager> ReplicationManager;
		
		TSharedRef<SWidget> BuildUploadButton();
		FText GetUploadButtonToolTipText() const;
		FReply OnUploadButtonClicked() const;
		
		TSharedRef<SWidget> BuildRevertButton();
	};
}

