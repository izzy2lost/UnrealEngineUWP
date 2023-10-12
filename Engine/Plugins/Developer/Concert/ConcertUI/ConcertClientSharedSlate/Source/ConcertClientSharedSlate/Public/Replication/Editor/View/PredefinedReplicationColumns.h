// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Replication/Editor/View/ReplicationColumn.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Templates/SharedPointer.h"

struct FConcertPropertyChain;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
	class IReplicationStreamViewer;
	class IObjectToPropertiesModel;
}

namespace UE::ConcertClientSharedSlate::ReplicationObjectColumns
{
	using FReplicationObjectColumn = TReplicationColumn<FReplicatedObjectData>;
	enum class EReplicationColumnOrder : int32
	{
		/** Displays the class icon */
		Icon = 10,
		/** Label of the object */
		Label = 20,
		/** Class of the object */
		Type = 30,
	};
}

namespace UE::ConcertClientSharedSlate::ReplicationPropertyColumns
{
	using FReplicationPropertyColumn = TReplicationColumn<FReplicatedPropertyData>;
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** The checkbox in SDefaultReplicationStreamEditor determining whether the property is in the selection*/
		ReplicatesCheckbox = 0,
		/** Label of the property */
		Label = 10,
		/** Type of the property */
		Type = 20
	};
	
	DECLARE_DELEGATE_RetVal_OneParam(ECheckBoxState, FGetColumnCheckboxState, const FConcertPropertyChain& /*Property*/);
	DECLARE_DELEGATE_TwoParams(FOnColumnCheckboxChanged, bool /*bIsChecked*/, const FConcertPropertyChain& /*Property*/);

	/** Util for creating a checkbox in a property row. The delegates will be given the path to the property. */
	CONCERTCLIENTSHAREDSLATE_API FReplicationPropertyColumn MakePropertyCheckboxColumn(
		FGetColumnCheckboxState GetPropertyCheckboxStateDelegate,
		FOnColumnCheckboxChanged OnPropertyBoxToggledDelegate,
		FText DefaultLabel = FText::GetEmpty(),
		FText ToolTipText = FText::GetEmpty(),
		const float ColumnWidth = 20.f,
		const int32 Priority = 0
		);
}
