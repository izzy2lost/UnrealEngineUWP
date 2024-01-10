// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiReplicationStreamEditor.h"

#include "ConsolidatedMultiStreamModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"

namespace UE::ConcertSharedSlate
{
	void SMultiReplicationStreamEditor::Construct(const FArguments& InArgs, FCreateMultiStreamEditorParams Params)
	{
		MultiStreamModel = MoveTemp(Params.MultiStreamModel);
		ConsolidatedModel = MakeShared<FConsolidatedMultiStreamModel>(MoveTemp(Params.ConsolidatedObjectModel), MoveTemp(Params.MultiStreamModel), InArgs._GetAutoAssignStream);
		
		const FCreateEditorParams BaseEditorParams
		{
			.DataModel = ConsolidatedModel.ToSharedRef(),
			.ObjectSource = MoveTemp(Params.ObjectSource),
			.PropertySource = MoveTemp(Params.PropertySource),
			.ViewerParams = MoveTemp(Params.ViewerParams)
		};
		EditorView = CreateBaseStreamEditor(BaseEditorParams);

		ChildSlot
		[
			EditorView.ToSharedRef()
		];
	}
}
