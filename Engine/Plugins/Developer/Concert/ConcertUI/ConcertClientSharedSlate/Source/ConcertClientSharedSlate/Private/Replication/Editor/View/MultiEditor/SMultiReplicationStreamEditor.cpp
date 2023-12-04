// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiReplicationStreamEditor.h"

#include "ConsolidatedMultiStreamModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"

namespace UE::ConcertClientSharedSlate
{
	void SMultiReplicationStreamEditor::Construct(const FArguments& InArgs, FCreateMultiStreamEditorParams Params)
	{
		MultiStreamModel = MoveTemp(Params.MultiStreamModel);

		const bool bConsolidatedModelShouldTransact = EnumHasAnyFlags(Params.Flags, EMultiStreamEditorFlags::Transactional);
		ConsolidatedModel = MakeShared<FConsolidatedMultiStreamModel>(MoveTemp(Params.MultiStreamModel), bConsolidatedModelShouldTransact, InArgs._GetAutoAssignStream);
		
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
