// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationWidgetFactories.h"

#include "Editor/Model/TransactionalPropertySelectionModel.h"
#include "Editor/View/SReplicationStreamEditor.h"
#include "Editor/View/SubobjectView/SUnrealEditorSubobjectView.h"

namespace UE::ConcertClientSharedSlate
{
	TSharedRef<IReplicationSubobjectView> CreateUnrealEditorSubobjectView()
	{
		return SNew(SUnrealEditorSubobjectView);
	}
	
	TSharedRef<IReplicationEditorView> CreateEditor(FCreateEditorParams Params)
	{
		return SNew(SReplicationStreamEditor, MoveTemp(Params));
	}
	
	TSharedRef<IEditableObjectToPropertiesModel> CreatePropertySelectionModel(
		UObject& OwnerObject,
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute,
		TAttribute<const FConcertReplicationEditorSettings*> OptionalReplicationSettingsAttribute
		)
	{
		return MakeShared<FTransactionalPropertySelectionModel>(
			OwnerObject,
			MoveTemp(ReplicationMapAttribute),
			MoveTemp(OptionalReplicationSettingsAttribute)
			);
	}
}

