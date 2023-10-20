// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

#include "Containers/ArrayView.h"

namespace UE::ConcertClientSharedSlate
{
	/**
	 * A widget for selecting subobjects on a given root object.
	 * When a user clicks a root entity (usually an actor) this view shows its subobjects (usually components).
	 * 
	 * This is an optional widget injected between the root object selection view and the property selection view.
	 *
	 * In the editor, this widget is backed by SSubobjectEditor.
	 * On the server, a custom widget is necessary since certain classes like SSubobjectEditor and UActorComponent, etc.
	 * are unavailable.
	 * 
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTCLIENTSHAREDSLATE_API IReplicationSubobjectView : public SCompoundWidget
	{
	public:

		/** Sets the root objects for which the subobject list should be shown. */
		virtual void SetTopLevelObjects(const TArray<FSoftObjectPath>& RootObjects) = 0;
		/** Clears the hierarchy so it displays no subobjects. */
		void ClearRootObjects() { SetTopLevelObjects({}); }

		/** Selects the root object in the UI. Executes OnSelectionChanged. */
		virtual void SelectTopLevelObjects() = 0;
		
		/**
		 * Gets the list of selected objects.
		 *
		 * This may return objects that are not currently in the object replication mapping of the underlying model;
		 * this is usually done when the surrounding UI edits the replication data. If the UI is view-only,
		 * GetSelectedObjects should only return objects in the model.
		 */
		virtual TArray<FSoftObjectPath> GetSelectedObjects() const = 0;

		/** Called when the subobject selection changes. */
		DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged);
		virtual FOnSelectionChanged& OnSelectionChanged() = 0;
		
		virtual ~IReplicationSubobjectView() = default;
	};
}