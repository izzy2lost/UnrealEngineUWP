// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertSyncClient;
class SOverlay;

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;

	/** Wraps SReplicationControlsTab with an overlay warning the user that this feature is still experimental */
	class SReplicationTabWithWarningOverlay : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationTabWithWarningOverlay)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager, TSharedRef<IConcertSyncClient> InClient);
		
	private:

		TSharedPtr<SOverlay> Overlay;
	};
}

