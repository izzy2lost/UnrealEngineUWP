// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "ReplicationColumn.h"
#include "Templates/SharedPointer.h"

namespace UE::MultiUserReplicationEditor
{
	class FReplicatedObjectData;
	class FReplicatedPropertyData;
	class IEditableObjectToPropertiesModel;
	class IObjectToPropertiesModel;
	class SPropertyReplicationSelectionEditor;
}

namespace UE::MultiUserReplicationEditor::ReplicationObjectColumns
{
	using FReplicationObjectColumn = TReplicationColumn<TSharedPtr<FReplicatedObjectData>>;
	
	enum class EReplicationColumnOrder : int32
	{
		/** Displays the class icon */
		Icon = 10,
		/** Label of the object */
		Label = 20,
		/** Class of the object */
		Type = 30,
	};
	
	extern const FName IconColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;

	FReplicationObjectColumn IconColumn(TSharedRef<IObjectToPropertiesModel> Model);
	FReplicationObjectColumn LabelColumn();
	FReplicationObjectColumn TypeColumn(TSharedRef<IObjectToPropertiesModel> Model);
}

namespace UE::MultiUserReplicationEditor::ReplicationPropertyColumns
{
	using FReplicationPropertyColumn = TReplicationColumn<TSharedPtr<FReplicatedPropertyData>>;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** The checkbox in SPropertyReplicationSelectionEditor determining whether the property is in the selection*/
		ReplicatesCheckbox = 0,
		/** Label of the property */
		Label = 10
	};
	
	/** The checkbox in SPropertyReplicationSelectionEditor determining whether the property is in the selection*/
	extern const FName ReplicatesColumnId;
	extern const FName LabelColumnId;

	FReplicationPropertyColumn ReplicatesColumns(TSharedRef<SPropertyReplicationSelectionEditor> EditorWidget, TSharedRef<IEditableObjectToPropertiesModel> Model);
	FReplicationPropertyColumn LabelColumn();

	/**
	 * Goes through all selected objects, checks whether the property is checked on it or not, and returns a checkbox state.
	 * The state is checked or unchecked if all selected properties have the property enabled or disabled, respectively, and undetermined otherwise, i.e. if it is mixed.
	 *
	 * This is used by ReplicatesColumns.
	 */
	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(const FReplicatedPropertyData& RowData, TConstArrayView<TSharedPtr<FReplicatedObjectData>> Selection, const IObjectToPropertiesModel& Model);
}
