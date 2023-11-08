// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationWidgetFactories.h"

#include "Editor/Model/TransactionalPropertySelectionModel.h"
#include "Editor/Model/Subobject/ComponentHierarchySubobjectModel.h"
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
	
	TSharedRef<IReplicationStreamEditor> CreateEditor(FCreateEditorParams Params)
	{
		return SNew(SBaseReplicationStreamEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			.AdditionalObjectColumns(Params.AdditionalObjectColumns)
			.AdditionalPropertyColumns(Params.AdditionalPropertyColumns)
			.SubobjectView(Params.SubobjectView)
			.SubobjectModel(Params.SubobjectModel)
			.OnExtendObjectsContextMenu(Params.OnExtendObjectsContextMenu)
			.SortPropertyRowPredicate(Params.SortPropertyRowPredicate)
			.LeftOfObjectSearchBar() [ Params.LeftOfObjectSearchBar.Widget ]
			.LeftOfPropertySearchBar() [ Params.LeftOfPropertySearchBar.Widget ]
			.IsEditingEnabled(Params.IsEditingEnabled)
			.EditingDisabledToolTipText(Params.EditingDisabledToolTipText)
			.ReplicationSettings(Params.ReplicationSettingsAttribute);
	}

	TSharedRef<IReplicationStreamEditor> CreateDefaultStreamEditor(FCreateEditorParams Params)
	{
		return SNew(SDefaultReplicationStreamEditor, Params.DataModel, Params.ObjectSource, Params.PropertySource)
			.AdditionalObjectColumns(Params.AdditionalObjectColumns)
			.AdditionalPropertyColumns(Params.AdditionalPropertyColumns)
			.SubobjectView(Params.SubobjectView)
			.SubobjectModel(Params.SubobjectModel)
			.OnExtendObjectsContextMenu(Params.OnExtendObjectsContextMenu)
			.SortPropertyRowPredicate(Params.SortPropertyRowPredicate)
			.LeftOfObjectSearchBar() [ Params.LeftOfObjectSearchBar.Widget ]
			.LeftOfPropertySearchBar() [ Params.LeftOfPropertySearchBar.Widget ]
			.IsEditingEnabled(Params.IsEditingEnabled)
			.EditingDisabledToolTipText(Params.EditingDisabledToolTipText)
			.ReplicationSettings(Params.ReplicationSettingsAttribute);
	}

	TSharedRef<IEditableObjectToPropertiesModel> CreatePropertySelectionModel(
		UObject& OwnerObject,
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute
		)
	{
		return MakeShared<FTransactionalPropertySelectionModel>(
			OwnerObject,
			MoveTemp(ReplicationMapAttribute)
			);
	}
}

