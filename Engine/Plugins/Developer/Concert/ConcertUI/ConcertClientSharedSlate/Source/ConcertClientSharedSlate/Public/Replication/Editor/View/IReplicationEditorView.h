// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Widget which edits a set of replication streams.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTCLIENTSHAREDSLATE_API IReplicationEditorView : public SCompoundWidget
	{
	public:

		/** Call after the data underlying the model was externally changed and needs to be redisplayed in the UI. */
		virtual void Refresh() = 0;

		virtual ~IReplicationEditorView() = default;
	};
}