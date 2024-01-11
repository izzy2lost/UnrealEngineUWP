// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FConcertStreamObjectAutoBindingRules;
struct FConcertObjectReplicationMap;

namespace UE::ConcertSharedSlate
{
	class IEditableReplicationStreamModel;
	class IReplicationStreamEditor;
	class IStreamExtender;
	class ISubobjectModel;
	struct FCreateEditorParams;
}

namespace UE::ConcertClientSharedSlate
{
	/** Builds a similar tree hierarchy as SSubobjectEditor. Reports only components as subobjects. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::ISubobjectModel> CreateSubobjectModelForComponentHierarchy();
	
	/**
	 * Wraps the passed in BaseModel and makes it transactional.
	 * All calls that modify the underlying model was wrapped with scoped transactions.
	 * 
	 * @param OwnerObject The object containing the FConcertObjectReplicationMap - used for transactions.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel(
		TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> BaseModel,
		UObject& OwnerObject
		);

	/** Simpler CreateTransactionalStreamModel overload that internally creates an UObject and sets it up automatically. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel();
	
	/**
	 * Creates a default IReplicationStreamEditor.
	 *
	 * This editor adds a checkbox to the start of every property row.
	 * - Checking adds the property to the selected objects' property mappings.
	 * - Unchecking removes the property to the selected objects' property mappings
	 * 
	 * @see CreateViewer for a description of the UI layout.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IReplicationStreamEditor> CreateDefaultStreamEditor(ConcertSharedSlate::FCreateEditorParams Params);
}
