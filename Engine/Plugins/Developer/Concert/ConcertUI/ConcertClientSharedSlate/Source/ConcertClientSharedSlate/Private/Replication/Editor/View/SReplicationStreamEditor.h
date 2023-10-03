// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/IReplicationEditorView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMultiUserReplicationClientPreset;

namespace UE::ConcertClientSharedSlate
{
	class SPropertyReplicationSelectionEditor;
	
	struct FCreateEditorParams;
	
	/** Root widget for editing a set of replication streams. */
	class CONCERTCLIENTSHAREDSLATE_API SReplicationStreamEditor : public IReplicationEditorView
	{
	public:
		
		SLATE_BEGIN_ARGS(SReplicationStreamEditor)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const FCreateEditorParams& Params);

		//~ Begin IReplicationEditorView Interface
		virtual void Refresh() override;
		//~ End IReplicationEditorView Interface

	private:
		
		TSharedPtr<SPropertyReplicationSelectionEditor> Editor;
	};
}
