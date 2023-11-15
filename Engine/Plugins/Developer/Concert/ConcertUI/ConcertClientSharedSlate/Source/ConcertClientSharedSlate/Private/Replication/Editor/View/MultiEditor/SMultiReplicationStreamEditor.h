// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	enum class EReplicatedObjectChangeReason : uint8;
	class FConsolidatedMultiStreamModel;
	class IEditableReplicationStreamModel;
}

namespace UE::ConcertClientSharedSlate
{
	class FAggregateReadOnlyStreamModel;
	struct FCreateMultiStreamEditorParams;

	/**
	 * Displays multiple streams in a single widget.
	 * 
	 * SMultiReplicationStreamEditor's responsibility is to consolidate all replicated objects in a single UI and display all properties in the class.
	 * The creator of SMultiReplicationStreamEditor is supposed to inject custom columns into the property section for assigning specific properties
	 * to the streams in the IEditableReplicationStreamModel. 
	 */
	class SMultiReplicationStreamEditor : public IMultiReplicationStreamEditor
	{
		using Super = IMultiReplicationStreamEditor;
	public:

		SLATE_BEGIN_ARGS(SMultiReplicationStreamEditor)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FCreateMultiStreamEditorParams Params);

		virtual IReplicationStreamEditor& GetEditorBase() const override { return *EditorView; }
		virtual IEditableMultiReplicationStreamModel& GetModel() const override { return *MultiStreamModel; }

	private:

		/** This model combines all of the user provided ones so they are all displayed in one view. */
		TSharedPtr<FConsolidatedMultiStreamModel> ConsolidatedModel;
		
		/** Tells us which streams there are. */
		TSharedPtr<IEditableMultiReplicationStreamModel> MultiStreamModel;

		/** The main view, which is displaying ConsolidatedModel. */
		TSharedPtr<IReplicationStreamEditor> EditorView;
		
	};
}
