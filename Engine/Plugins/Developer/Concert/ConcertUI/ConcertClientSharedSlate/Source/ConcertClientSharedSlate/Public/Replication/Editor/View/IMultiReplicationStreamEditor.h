// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	class IEditableMultiReplicationStreamModel;
	class IReplicationStreamEditor;
	
	/**
	 * Widget which edits multiple replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTCLIENTSHAREDSLATE_API IMultiReplicationStreamEditor : public SCompoundWidget
	{
	public:

		/** @return The widget drawing the consolidate model. */
		virtual IReplicationStreamEditor& GetEditorBase() const = 0;

		/** @return The underlying model */
		virtual IEditableMultiReplicationStreamModel& GetModel() const = 0;
	};
}
