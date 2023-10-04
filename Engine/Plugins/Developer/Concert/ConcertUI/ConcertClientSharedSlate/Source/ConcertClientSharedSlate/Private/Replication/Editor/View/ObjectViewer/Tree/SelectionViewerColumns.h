// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "ReplicationColumn.h"
#include "Templates/SharedPointer.h"

struct FConcertPropertyChain;

namespace UE::ConcertClientSharedSlate
{
	class FReplicatedObjectData;
	class FReplicatedPropertyData;
	class IEditableObjectToPropertiesModel;
	class IObjectToPropertiesModel;
	class SObjectToPropertyEditor;
}

namespace UE::ConcertClientSharedSlate::ReplicationObjectColumns
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

	FReplicationObjectColumn IconColumn(TSharedRef<IObjectToPropertiesModel> Model, const float ColumnWidth = 20.f);
	FReplicationObjectColumn LabelColumn();
	FReplicationObjectColumn TypeColumn(TSharedRef<IObjectToPropertiesModel> Model);
}

namespace UE::ConcertClientSharedSlate::ReplicationPropertyColumns
{
	using FReplicationPropertyColumn = TReplicationColumn<TSharedPtr<FReplicatedPropertyData>>;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** The checkbox in SObjectToPropertyEditor determining whether the property is in the selection*/
		ReplicatesCheckbox = 0,
		/** Label of the property */
		Label = 10,
		/** Type of the property */
		Type = 20
	};
	
	/** The checkbox in SObjectToPropertyEditor determining whether the property is in the selection*/
	extern const FName ReplicatesColumnId;
	extern const FName LabelColumnId;
	extern const FName TypeColumnId;

	DECLARE_DELEGATE_RetVal_OneParam(ECheckBoxState, FGetPropertyCheckboxState, const FConcertPropertyChain& /*Property*/);
	DECLARE_DELEGATE_TwoParams(FOnPropertyCheckboxChanged, bool /*bIsChecked*/, const FConcertPropertyChain& /*Property*/);

	FReplicationPropertyColumn ReplicatesColumns(
		FGetPropertyCheckboxState GetPropertyCheckboxStateDelegate,
		FOnPropertyCheckboxChanged OnPropertyBoxToggledDelegate,
		const float ColumnWidth = 20.f
		);
	FReplicationPropertyColumn LabelColumn();
	FReplicationPropertyColumn TypeColumn();

	/**
	 * Goes through all selected objects, checks whether the property is checked on it or not, and returns a checkbox state.
	 * The state is checked or unchecked if all selected properties have the property enabled or disabled, respectively, and undetermined otherwise, i.e. if it is mixed.
	 *
	 * This is used by ReplicatesColumns.
	 */
	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(const FConcertPropertyChain& Property, TConstArrayView<FSoftObjectPath> Selection, const IObjectToPropertiesModel& Model);
}
