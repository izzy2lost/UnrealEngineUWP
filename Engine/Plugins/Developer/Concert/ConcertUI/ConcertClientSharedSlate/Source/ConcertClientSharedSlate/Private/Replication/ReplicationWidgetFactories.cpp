// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationWidgetFactories.h"

#include "Editor/Model/TransactionalReplicationStreamModel.h"
#include "Editor/Model/Subobject/ComponentHierarchySubobjectModel.h"
#include "Editor/View/MultiEditor/SMultiReplicationStreamEditor.h"
#include "Editor/View/ObjectEditor/SDefaultReplicationStreamEditor.h"
#include "Editor/View/ObjectEditor/SBaseReplicationStreamEditor.h"
#include "Editor/View/SubobjectView/SSubobjectView.h"

namespace UE::ConcertClientSharedSlate
{
	TSharedRef<IReplicationSubobjectView> CreateUnrealEditorSubobjectView(FCreateSubobjectViewParams Params)
	{
		return SNew(SSubobjectView, MoveTemp(Params.SubobjectModel))
			.AdditionalColumns(Params.AdditionalColumns);
	}
	
	TSharedRef<ISubobjectModel> CreateDefaultComponentHierarchySubobjectModel()
	{
		return MakeShared<FComponentHierarchySubobjectModel>();
	}
	
	TSharedRef<IReplicationStreamEditor> CreateBaseStreamEditor(FCreateEditorParams Params)
	{
		return SNew(SBaseReplicationStreamEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			.AdditionalObjectColumns(Params.ViewerParams.AdditionalObjectColumns)
			.AdditionalPropertyColumns(Params.ViewerParams.AdditionalPropertyColumns)
			.SubobjectView(Params.ViewerParams.SubobjectView)
			.SubobjectModel(Params.ViewerParams.SubobjectModel)
			.OnExtendObjectsContextMenu(Params.ViewerParams.OnExtendObjectsContextMenu)
			.SortPropertyRowPredicate(Params.ViewerParams.SortPropertyRowPredicate)
			.LeftOfObjectSearchBar() [ Params.ViewerParams.LeftOfObjectSearchBar.Widget ]
			.LeftOfPropertySearchBar() [ Params.ViewerParams.LeftOfPropertySearchBar.Widget ]
			.IsEditingEnabled(Params.IsEditingEnabled)
			.EditingDisabledToolTipText(Params.EditingDisabledToolTipText)
			.ReplicationSettings(Params.ReplicationSettingsAttribute);
	}

	TSharedRef<IReplicationStreamEditor> CreateDefaultStreamEditor(FCreateEditorParams Params)
	{
		return SNew(SDefaultReplicationStreamEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			.AdditionalObjectColumns(Params.ViewerParams.AdditionalObjectColumns)
			.AdditionalPropertyColumns(Params.ViewerParams.AdditionalPropertyColumns)
			.SubobjectView(Params.ViewerParams.SubobjectView)
			.SubobjectModel(Params.ViewerParams.SubobjectModel)
			.OnExtendObjectsContextMenu(Params.ViewerParams.OnExtendObjectsContextMenu)
			.SortPropertyRowPredicate(Params.ViewerParams.SortPropertyRowPredicate)
			.LeftOfObjectSearchBar() [ Params.ViewerParams.LeftOfObjectSearchBar.Widget ]
			.LeftOfPropertySearchBar() [ Params.ViewerParams.LeftOfPropertySearchBar.Widget ]
			.IsEditingEnabled(Params.IsEditingEnabled)
			.EditingDisabledToolTipText(Params.EditingDisabledToolTipText)
			.ReplicationSettings(Params.ReplicationSettingsAttribute);
	}

	TSharedRef<IEditableReplicationStreamModel> CreatePropertySelectionModel(
		UObject& OwnerObject,
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute
		)
	{
		return MakeShared<FTransactionalReplicationStreamModel>(
			OwnerObject,
			MoveTemp(ReplicationMapAttribute)
			);
	}

	TSharedRef<IMultiReplicationStreamEditor> CreateBaseMultiStreamEditor(FCreateMultiStreamEditorParams Params)
	{
		return SNew(SMultiReplicationStreamEditor, MoveTemp(Params));
	}
}

