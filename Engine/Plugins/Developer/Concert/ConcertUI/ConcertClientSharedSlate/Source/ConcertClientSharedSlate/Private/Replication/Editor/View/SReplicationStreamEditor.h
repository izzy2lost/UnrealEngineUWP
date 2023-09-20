// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMultiUserReplicationClientPreset;

namespace UE::ConcertClientSharedSlate
{
	class SPropertyReplicationSelectionEditor;
	
	struct FCreateEditorParams;
	
	/** Root widget for editing UMultiUserReplicationStreamAsset. */
	class CONCERTCLIENTSHAREDSLATE_API SReplicationStreamEditor : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SReplicationStreamEditor)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const FCreateEditorParams& Params);
	};
}
