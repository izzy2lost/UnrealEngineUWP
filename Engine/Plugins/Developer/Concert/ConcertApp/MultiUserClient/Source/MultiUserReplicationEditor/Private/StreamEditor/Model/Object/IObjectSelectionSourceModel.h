// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IObjectSourceModel.h"
#include "Model/Item/SourceSelectionCategory.h"
#include "Templates/SharedPointer.h"

namespace UE::MultiUserReplicationEditor
{
	using FObjectSourceCategory = ConcertSharedSlate::TSourceSelectionCategory<FSelectableObjectInfo>;
	
	enum class EObjectItemValidity : uint8
	{
		/** The item is allowed */
		Valid,
		/** The referenced object no longer exists. E.g. the actor was removed. */
		DoesNotExist,
		/** Any other reason, e.g. the object was allowed in a previous version but no longer is allowed now. */
		Invalid
	};
	
	/** Decides which objects can be added to a IEditableObjectToPropertiesModel. */
	class IObjectSelectionSourceModel : public TSharedFromThis<IObjectSelectionSourceModel>
	{
	public:

		/** Gets the "root set" of sources. Each one should be a button next to the search bar, e.g. "Add actor". */
		virtual TArray<FObjectSourceCategory> GetRootSources() const = 0;

		/** @return The sources that should be displayed in the right-click context menu when clicked in the menu. */
		virtual TArray<TSharedRef<IObjectSourceModel>> GetContextMenuOptions(const FSoftObjectPath& Items) = 0;

		/**
		 * Checks whether the selected item is valid to be added to the IEditableObjectToPropertiesModel.
		 *
		 * This can be used to validate the asset when it is initially displayed but could also be useful for validating
		 * actors / assets that were drag-dropped into the editor.
		 */
		virtual EObjectItemValidity GetItemValidity(const FSoftObjectPath& Item) const = 0;
		
		virtual ~IObjectSelectionSourceModel() = default;
	};
}
