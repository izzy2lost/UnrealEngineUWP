// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SOverlay;

namespace UE::MultiUserClient
{
	/** Wraps SReplicationControlsTab with an overlay warning the user that this feature is still experimental */
	class SReplicationTabWithWarningOverlay : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationTabWithWarningOverlay)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		
	private:

		TSharedPtr<SOverlay> Overlay;
	};
}

