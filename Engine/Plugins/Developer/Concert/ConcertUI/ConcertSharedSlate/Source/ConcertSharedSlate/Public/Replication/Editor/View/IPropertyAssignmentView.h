// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Templates/SharedPointer.h"

class SWidget;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamModel;
	
	/**
	 * A replication assignment view displays an object's properties.
	 * It is the piece of UI that is displayed on the bottom part of the replication viewer.
	 * 
	 * The replication stream viewer / editor looks like this (@see CreateBaseStreamEditor):
	 * - Object tree view: User can click on an object.
	 * - IPropertyAssignmentView: The clicked object's properties are displayed (the "root objects").
	 *
	 * Right now there is only 1 implementation: SPerObjectPropertyAssignment, which displays the root object's properties only.
	 * We could also add an implementation that also displays the subobject properties, similar how the editor's property matrix does it. 
	 * Generally, you can imagine the view as a tree view which has columns that can be injected (e.g. @see CreateBaseStreamEditor).
	 */
	class CONCERTSHAREDSLATE_API IPropertyAssignmentView
	{
	public:

		/**
		 * Rebuilds all displayed data immediately.
		 *
		 * @param Objects The objects that are supposed to be displayed.
		 * @param Model The model that can be queried for object info.
		 */
		virtual void RefreshData(const TArray<FSoftObjectPath>& Objects, const IReplicationStreamModel& Model) = 0;
		
		/** Reapply the filter function to all items at the end of the frame. Call e.g. when the filters have changed. */
		virtual void RequestRefilter() const = 0;
		
		/**
		 * Requests that the given column be resorted, if it currently affects the row sorting (primary or secondary).
		 * Call e.g. when a sortable attribute of the column has changed.
		 */
		virtual void RequestResortForColumn(const FName& ColumnId) = 0;

		/** Gets the tree view's widget */
		virtual TSharedRef<SWidget> GetWidget() = 0;

		virtual ~IPropertyAssignmentView() = default;
	};
}
