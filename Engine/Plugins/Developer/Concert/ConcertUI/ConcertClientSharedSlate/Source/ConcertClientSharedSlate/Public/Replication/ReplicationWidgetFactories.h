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
	class IEditableObjectToPropertiesModel;
	class IObjectSelectionSourceModel;
	class IPropertySelectionSourceModel;
		
	struct FCreateEditorParams
	{
		/** The model that the editor is displaying */
		TSharedRef<IEditableObjectToPropertiesModel> DataModel;

		/** Determines the objects that can be added to the object list. */
		TSharedRef<IObjectSelectionSourceModel> ObjectSource;

		/** Determines the properties that can be added to the property list. */
		TSharedRef<IPropertySelectionSourceModel> PropertySource;

		// TODO DP: Add way to add more columns
	};

	/**
	 * Creates a replication editor.
	 * 
	 * The editor consists of three areas, which share similar workflows as the world outliner and details panel in the level editor:
	 * 1. Root objects,  similar to world outliner: objects (usually actors) are added here. FCreateEditorParams::ObjectSource is used to build a combo button through which new objects can be added.
	 * 2. Subobjects, similar to component view (SSubobjectEditor): Shows subobjects of a root objects selected above; typically components.
	 * 3. Properties, similar to details panel: Shows properties of the selected root object and / or subobjects.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<SWidget> CreateEditor(FCreateEditorParams Params);

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