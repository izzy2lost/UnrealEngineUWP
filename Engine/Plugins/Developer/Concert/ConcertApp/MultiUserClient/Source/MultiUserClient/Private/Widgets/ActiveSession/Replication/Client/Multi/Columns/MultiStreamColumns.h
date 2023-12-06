// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"

#include "Delegates/Delegate.h"
#include "Misc/EnumClassFlags.h"

class IConcertClient;

namespace UE::MultiUserClient
{
	class FReassignObjectPropertiesLogic;
	class FReplicationClientManager;
}

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamModel;
	class IMultiReplicationStreamEditor;
	class IReplicationStreamViewer;
}

namespace UE::MultiUserClient::MultiStreamColumns
{
	const extern FName ReplicationToggleColumnId;
	const extern FName ReassignOwnershipColumnId;
	const extern FName AssignPropertyColumnId;
	
	/* @see ETopLevelColumnOrder and EReplicationPropertyColumnOrder */
	enum class EColumnSortOrder
	{
		ReplicationToggle = 0,
		AssignPropertyColumn = 30,
		ReassignOwnership = 40
	};

	/**
	 * Toggles replication for all clients assigned to the object (and optionally all children).
	 * 
	 * @param ConcertClient Used to look up client names
	 * @param ConsolidatedStreamModelAttribute Used to get child objects
	 * @param ClientManager Used to access all clients for toggling authority
	 * @param ColumnsSortPriority The order relative to the other columns
	 * 
	 * @return A checkbox for controlling the authority of the object in the row
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReplicationToggle(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<ConcertClientSharedSlate::IReplicationStreamModel*> ConsolidatedStreamModelAttribute,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::ReplicationToggle)
		);

	/**
	 * A combo box which displays all current owners for an object and allows bulk reassigning properties.
	 *
	 * @param ConcertClient Used to look up client names
	 * @param MultiStreamModelAttribute Used to get child objects from the consolidated model and for requesting resorting the column
	 * @param ReassignmentLogic Performs the act of reassigning
	 * @param ClientManager Used to access all clients for display in the combo box drop-down
	 * @param ColumnsSortPriority The order relative to the other columns
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::ReassignOwnership)
		);
	
	/**
	 * Creates a property column which assigns the property to the stream selected in the combo box.
	 * 
	 * @param MultiStreamEditor Used to determine the selected objects.
	 * @param ConcertClient Used to look up client names
	 * @param ClientManager Used to map streams back to client display info
	 * @param ColumnsSortPriority The order relative to the other columns
	 * 
	 * @return A column that spawns a combo box for assigning properties
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		TSharedRef<IConcertClient> ConcertClient,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::AssignPropertyColumn)
		);
}
