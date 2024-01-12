// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationWidgetFactories.h"

#include "Editor/Model/GenericReplicationStreamModel.h"
#include "Editor/View/MultiEditor/SMultiReplicationStreamEditor.h"
#include "Editor/View/ObjectEditor/SBaseReplicationStreamEditor.h"

namespace UE::ConcertSharedSlate
{
	TSharedRef<IEditableReplicationStreamModel> CreateBaseStreamModel(
		TAttribute<FConcertObjectReplicationMap*> ReplicationMapAttribute,
		TSharedPtr<IStreamExtender> Extender
		)
	{
		return MakeShared<FGenericReplicationStreamModel>(MoveTemp(ReplicationMapAttribute), MoveTemp(Extender));
	}
	
	TSharedRef<IReplicationStreamEditor> CreateBaseStreamEditor(FCreateEditorParams Params)
	{
		return SNew(SBaseReplicationStreamEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			.AdditionalObjectColumns(Params.ViewerParams.AdditionalObjectColumns)
			.PrimaryObjectSort(Params.ViewerParams.PrimaryObjectSort)
			.SecondaryObjectSort(Params.ViewerParams.SecondaryObjectSort)
			.AdditionalPropertyColumns(Params.ViewerParams.AdditionalPropertyColumns)
			.PrimaryPropertySort(Params.ViewerParams.PrimaryPropertySort)
			.SecondaryPropertySort(Params.ViewerParams.SecondaryPropertySort)
			.SubobjectModel(Params.ViewerParams.SubobjectModel)
			.NameModel(Params.ViewerParams.NameModel)
			.OnExtendObjectsContextMenu(Params.ViewerParams.OnExtendObjectsContextMenu)
			.LeftOfObjectSearchBar() [ Params.ViewerParams.LeftOfObjectSearchBar.Widget ]
			.LeftOfPropertySearchBar() [ Params.ViewerParams.LeftOfPropertySearchBar.Widget ]
			.IsEditingEnabled(Params.IsEditingEnabled)
			.EditingDisabledToolTipText(Params.EditingDisabledToolTipText);
	}

	TSharedRef<IMultiReplicationStreamEditor> CreateBaseMultiStreamEditor(FCreateMultiStreamEditorParams Params)
	{
		return SNew(SMultiReplicationStreamEditor, MoveTemp(Params))
			.GetAutoAssignStream(MoveTemp(Params.GetAutoAssignToStreamDelegate));
	}
}

