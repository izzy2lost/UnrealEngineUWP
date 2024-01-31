// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Delegates/Delegate.h"

namespace UE::ConcertReplicationScriptingEditor
{
	extern const FName PropertySelectionCheckboxColumnId;
	
	DECLARE_DELEGATE_TwoParams(FOnSelectProperty, const FConcertPropertyChain& PropertyChain, bool bIsSelected);

	/**
	 * Creates a checkbox column.
	 *
	 * @param SelectedProperties The checkbox is checked if the path is in SelectedProperties.
	 * @param OnSelectPropertyDelegate Executed when a checkbox is checked or unchecked.
	 */
	ConcertSharedSlate::ReplicationColumns::FReplicationPropertyColumn MakePropertySelectionCheckboxColumn(
		const TSet<FConcertPropertyChain>& SelectedProperties,
		FOnSelectProperty OnSelectPropertyDelegate,
		bool bIsEditable,
		int32 SortPriority = 0 /* This places it before label column by default */
		);
}

