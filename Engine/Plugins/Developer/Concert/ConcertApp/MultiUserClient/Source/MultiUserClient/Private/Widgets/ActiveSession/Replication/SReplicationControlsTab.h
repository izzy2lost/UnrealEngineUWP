// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"

class SExpandableArea;

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;

	/** Root widget for replication in Multi-User session. */
	class SReplicationControlsTab : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationControlsTab)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager);
	};
}

