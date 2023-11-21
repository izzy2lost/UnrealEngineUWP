// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/ReplicationColumn.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"

namespace UE::ConcertClientSharedSlate::ReplicationColumns
{
	/** Rows displayed in the outliner */
	using FReplicationTopLevelObjectColumn = TReplicationColumn<FReplicatedObjectData>;
	using FReplicationPropertyColumn = TReplicationColumn<FReplicatedPropertyData>;
}
