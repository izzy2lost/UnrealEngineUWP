// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertClientSharedSlate
{
	class IObjectToPropertiesModel;
}

namespace UE::ConcertClientSharedSlate::ReplicationObjectColumns
{
	extern const FName IconColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;

	FReplicationObjectColumn IconColumn(TSharedRef<IObjectToPropertiesModel> Model, const float ColumnWidth = 20.f);
	FReplicationObjectColumn LabelColumn();
	FReplicationObjectColumn TypeColumn(TSharedRef<IObjectToPropertiesModel> Model);
}

namespace UE::ConcertClientSharedSlate::ReplicationPropertyColumns
{
	extern const FName ReplicatesColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;
	
	FReplicationPropertyColumn LabelColumn();
	FReplicationPropertyColumn TypeColumn();
	/**
	 * A checkbox that is placed at the beginning of every property.
	 * Checking & unchecking adds & removes the property to the selected objects' property mapping, respectively.
	 */
	CONCERTCLIENTSHAREDSLATE_API FReplicationPropertyColumn ReplicatesColumns(
		TWeakPtr<IReplicationStreamViewer> Viewer,
		TWeakPtr<IEditableObjectToPropertiesModel> Model,
		const float ColumnWidth = 20.f,
		const int32 Priority = static_cast<int32>(EReplicationPropertyColumnOrder::ReplicatesCheckbox)
		);

	/**
	 * Goes through all selected objects, checks whether the property is checked on it or not, and returns a checkbox state.
	 * The state is checked or unchecked if all selected properties have the property enabled or disabled, respectively, and undetermined otherwise, i.e. if it is mixed.
	 *
	 * This is used by ReplicatesColumns.
	 */
	CONCERTCLIENTSHAREDSLATE_API ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(
		const FConcertPropertyChain& Property,
		TConstArrayView<FSoftObjectPath> Selection,
		const IObjectToPropertiesModel& Model
		);
	/** Util for sorting property data by whether its ReplicatesColumns() column is checked. */
	CONCERTCLIENTSHAREDSLATE_API bool SortBySelectionThenByName_PropertyPredicate(
		const TArray<FSoftObjectPath>& SelectedObjects,
		const IObjectToPropertiesModel& Model,
		const FReplicatedPropertyData& Left,
		const FReplicatedPropertyData& Right
		);
}
