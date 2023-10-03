// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationStreamEditor.h"

#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/View/ObjectEditor/SPropertyReplicationSelectionEditor.h"

#include "Widgets/SBoxPanel.h"

namespace UE::ConcertClientSharedSlate
{
	void SReplicationStreamEditor::Construct(const FArguments& InArgs, const FCreateEditorParams& Params)
	{
		ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			[
				SAssignNew(Editor, SPropertyReplicationSelectionEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			]
		];
	}

	void SReplicationStreamEditor::Refresh()
	{
		Editor->RefreshObjectData();
		Editor->RefreshPropertyData();
	}
}


