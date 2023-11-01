// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Widget which views a replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTCLIENTSHAREDSLATE_API IReplicationStreamViewer : public SCompoundWidget
	{
	public:

		/** Call after the data underlying the model was externally changed and needs to be redisplayed in the UI. */
		virtual void Refresh() = 0;

		/** @return The objects selected in the top of the viewer; these are the objects that IReplicationSubobjectView bases its view of. */
		virtual TArray<FSoftObjectPath> GetSelectedTopLevelObjects() const = 0;
		/**
		 * @return The objects for which the properties are being edited / displayed.
		 * If there is an IReplicationSubobjectView, this is IReplicationSubobjectView::GetSelectedObjects.
		 * Otherwise it is IReplicationStreamViewer::GetSelectedTopLevelObjects.
		 */
		virtual TArray<FSoftObjectPath> GetObjectsBeingPropertyEdited() const = 0;

		virtual ~IReplicationStreamViewer() = default;
	};
}