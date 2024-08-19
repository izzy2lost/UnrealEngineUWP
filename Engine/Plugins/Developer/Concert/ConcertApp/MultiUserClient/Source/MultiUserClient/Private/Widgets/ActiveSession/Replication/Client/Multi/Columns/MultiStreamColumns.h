// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Editor/View/Column/IObjectTreeColumn.h"

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"

class IConcertClient;

namespace UE::MultiUserClient::Replication
{
	class FMuteChangeTracker;
	class FReassignObjectPropertiesLogic;
	class FOnlineClientManager;
	class FUnifiedClientView;
}

namespace UE::ConcertSharedSlate
{
	class IObjectHierarchyModel;
	class IReplicationStreamModel;
	class IMultiReplicationStreamEditor;
	class IReplicationStreamViewer;
}

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	const extern FName MuteToggleColumnId;
	const extern FName AssignedClientsColumnId;
	const extern FName AssignPropertyColumnId;
	
	/* @see ETopLevelColumnOrder and EReplicationPropertyColumnOrder */
	enum class EColumnSortOrder
	{
		MuteToggle = 0,
		AssignPropertyColumn = 30,
		ReassignOwnership = 40
	};

	/**
	 * Mutes and unmutes the object and its child objects.
	 * 
	 * @param MuteChangeTracker Tells us the mute state and changes it.
	 * @param ColumnsSortPriority The order relative to the other columns
	 * @return A checkbox with pause and unpause icons.
	 */
	ConcertSharedSlate::FObjectColumnEntry MuteToggleColumn(
		FMuteChangeTracker& MuteChangeTracker UE_LIFETIMEBOUND,
		int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::MuteToggle)
		);

	/**
	 * A combo box which displays all current owners for an object and allows bulk reassigning properties.
	 *
	 * @param ConcertClient Used to look up client names
	 * @param MultiStreamModelAttribute Used for requesting resorting the column
	 * @param ObjectHierarchy Used to display clients when a child object is replicated
	 * @param ClientView Used to access all clients' stream content for display in the column
	 * @param ColumnsSortPriority The order relative to the other columns
	 */
	ConcertSharedSlate::FObjectColumnEntry AssignedClientsColumn(
		const TSharedRef<IConcertClient>& ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy UE_LIFETIMEBOUND,
		FUnifiedClientView& ClientView UE_LIFETIMEBOUND,
		int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::ReassignOwnership)
		);
	
	/**
	 * Creates a property column which assigns the property to the stream selected in the combo box.
	 * 
	 * @param MultiStreamEditor Used to determine the selected objects.
	 * @param ClientView Used to obtain the clients that can be assigned to and display them
	 * @param ColumnsSortPriority The order relative to the other columns
	 * 
	 * @return A column that spawns a combo box for assigning properties
	 */
	ConcertSharedSlate::FPropertyColumnEntry AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		FUnifiedClientView& ClientView UE_LIFETIMEBOUND,
		const int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::AssignPropertyColumn)
		);
}
