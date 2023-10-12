// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationStreamViewer.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Widget which edits replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTCLIENTSHAREDSLATE_API IReplicationStreamEditor : public IReplicationStreamViewer
	{};
}