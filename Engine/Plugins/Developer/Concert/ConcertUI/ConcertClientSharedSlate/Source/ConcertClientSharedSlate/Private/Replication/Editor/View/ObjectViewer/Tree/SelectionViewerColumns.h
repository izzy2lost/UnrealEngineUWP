// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertClientSharedSlate
{
	class IEditableReplicationStreamModel;
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
	class ISubobjectModel;
}

namespace UE::ConcertClientSharedSlate::ReplicationColumns::TopLevel
{
	extern const FName IconColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;

	enum class ETopLevelColumnOrder : int32
	{
		/** Label of the object */
		Label = 20,
		/** Class of the object */
		Type = 30,
	};

	FReplicationTopLevelObjectColumn LabelColumn(TSharedRef<IReplicationStreamModel> Model, ISubobjectModel* SubobjectModel = nullptr);
	FReplicationTopLevelObjectColumn TypeColumn(TSharedRef<IReplicationStreamModel> Model);
}

namespace UE::ConcertClientSharedSlate::ReplicationColumns::Subobject
{
	extern const FName DisplayColumnId;
	
	enum class ESubobjectColumnOrder : int32
	{
		/** Displays subobject name and class icon */
		DisplayLabel = 10,
	};

	/**
	 * Displays class icon and name of subobject
	 * @param SubobjectModel Used to get display names. Outlives the column.
	 */
	FReplicationSubobjectObjectColumn DisplayColumn(ISubobjectModel& SubobjectModel);
}

namespace UE::ConcertClientSharedSlate::ReplicationColumns::Property
{
	extern const FName ReplicatesColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** The checkbox in SDefaultReplicationStreamEditor determining whether the property is in the selection*/
		ReplicatesCheckbox = 0,
		/** Label of the property */
		Label = 10,
		/** Type of the property */
		Type = 20
	};
	
	FReplicationPropertyColumn LabelColumn();
	FReplicationPropertyColumn TypeColumn();
	/**
	 * A checkbox that is placed at the beginning of every property.
	 * Checking & unchecking adds & removes the property to the selected objects' property mapping, respectively.
	 *
	 * @param Viewer Used to determine which objects are currently selected (and being edited by the checkbox)
	 * @param Model Used to actually edit the object to properties mapping
	 * @param IsEnabledDelegate Determines whether the checkbox is enabled
	 * @param DisabledToolTipText Tooltip to display when IsEnabledDelegate returns false
	 * @param ColumnWidth Width to use for the column
	 * @param Priority Determines position of this columns relative to the others
	 */
	CONCERTCLIENTSHAREDSLATE_API FReplicationPropertyColumn ReplicatesColumns(
		TWeakPtr<IReplicationStreamViewer> Viewer,
		TWeakPtr<IEditableReplicationStreamModel> Model,
		TReplicationColumnDelegates<FReplicatedPropertyData>::FIsEnabled IsEnabledDelegate = {},
		TAttribute<FText> DisabledToolTipText = {},
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
		const IReplicationStreamModel& Model
		);
	/** Util for sorting property data by whether its ReplicatesColumns() column is checked. */
	CONCERTCLIENTSHAREDSLATE_API bool SortBySelectionThenByName_PropertyPredicate(
		const TArray<FSoftObjectPath>& SelectedObjects,
		const IReplicationStreamModel& Model,
		const FReplicatedPropertyData& Left,
		const FReplicatedPropertyData& Right
		);
}
