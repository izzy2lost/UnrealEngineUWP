// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"

class UObject;
class SWidget;

struct FConcertReplicationEditorSettings;
struct FObjectReplicationMap;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationSubobjectView;
}

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
	class IReplicationEditorView;
	class IObjectSelectionSourceModel;
	class IPropertySelectionSourceModel;
		
	struct FCreateEditorParams
	{
		/**
		 * The model that the editor is displaying.
		 * @note The view will keep a strong reference to this.
		 */
		TSharedRef<IEditableObjectToPropertiesModel> DataModel;

		/**
		 * Determines the objects that can be added to the object list. 
		 * @note The view will keep a strong reference to this.
		 */
		TSharedRef<IObjectSelectionSourceModel> ObjectSource;

		/**
		 * Determines the properties that can be added to the property list.
		 * @note The view will keep a strong reference to this.
		 */
		TSharedRef<IPropertySelectionSourceModel> PropertySource;

		// TODO DP: Add way to add more columns

		/**
		 * Optional. This is inserted between the root object outliner and property view.
		 * It e.g. displays the components of the actor selected in the root object view.
		 * 
		 * Exists so it can be customized differently depending on whether used in the editor or on the server-
		 */
		TSharedPtr<IReplicationSubobjectView> SubobjectView;
	};
	
	/** Creates an object that looks like the SSubobjectEditor. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<IReplicationSubobjectView> CreateUnrealEditorSubobjectView();

	/**
	 * Creates a replication editor.
	 * 
	 * The editor consists of three areas, which share similar workflows as the world outliner and details panel in the level editor:
	 * 1. Root objects,  similar to world outliner: objects (usually actors) are added here. FCreateEditorParams::ObjectSource is used to build a combo button through which new objects can be added.
	 * 2. Subobjects (optional), similar to component view (SSubobjectEditor): Shows subobjects of a root objects selected above; typically components.
	 * 3. Properties, similar to details panel: Shows properties of the selected root object and / or subobjects.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<IReplicationEditorView> CreateEditor(FCreateEditorParams Params);

	/** Creates a default IReplicationEditorView view to use in the Unreal Editor (as opposed to on the server, etc.). */
	inline TSharedRef<IReplicationEditorView> CreateEditorForUnrealEditor(FCreateEditorParams BaseParams)
	{
		BaseParams.SubobjectView = CreateUnrealEditorSubobjectView();
		return CreateEditor(MoveTemp(BaseParams));
	}
	
	/**
	 * Creates a model that can be passed to CreateEditor.
	 * 
	 * This model edits a FObjectReplicationMap that is assumed to be within the transactional OwnerObject. The model will respond to undo & redo by triggering the model's update callbacks.
	 * 
	 * @param OwnerObject The object containing the FObjectReplicationMap.
	 * @param ReplicationMapAttribute Getter for extracting the FObjectReplicationMap to edit
	 * 
	 * @return A model that will edit the FObjectReplicationMap.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<IEditableObjectToPropertiesModel> CreatePropertySelectionModel(
		UObject& OwnerObject,
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute,
		TAttribute<const FConcertReplicationEditorSettings*> OptionalReplicationSettingsAttribute = {}
		);
}