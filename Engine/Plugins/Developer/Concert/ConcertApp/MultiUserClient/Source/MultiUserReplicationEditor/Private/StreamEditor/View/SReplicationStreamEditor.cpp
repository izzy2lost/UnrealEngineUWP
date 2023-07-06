// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationStreamEditor.h"

#include "MultiUserReplicationStreamAsset.h"
#include "StreamEditor/Model/Object/EditorObjectSelectionSourceModel.h"
#include "StreamEditor/Model/Property/SelectPropertyFromUClassModel.h"
#include "StreamEditor/Model/PropertySelectionAssetModel.h"
#include "StreamEditor/View/ObjectEditor/SPropertyReplicationSelectionEditor.h"
#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserReplicationEditor
{
	void SReplicationStreamEditor::Construct(const FArguments& InArgs, UMultiUserReplicationStreamAsset& InEditedAsset)
	{
		AssetReadWriteModel = MakeShared<FPropertySelectionAssetModel>(*InEditedAsset.ReplicationList);
		ObjectSourceModel = MakeShared<FEditorObjectSelectionSourceModel>();
		PropertySourceModel = MakeShared<FSelectPropertyFromUClassModel>();
		ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			[
				SNew(SPropertyReplicationSelectionEditor, AssetReadWriteModel.ToSharedRef(), ObjectSourceModel.ToSharedRef(), PropertySourceModel.ToSharedRef())
			]
		];
	}
}


