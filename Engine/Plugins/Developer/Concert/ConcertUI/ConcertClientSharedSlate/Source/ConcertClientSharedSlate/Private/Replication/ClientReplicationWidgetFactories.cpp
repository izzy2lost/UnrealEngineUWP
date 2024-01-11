// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ClientReplicationWidgetFactories.h"

#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/ReplicationStreamObject.h"
#include "Replication/Editor/Model/Subobject/ComponentHierarchySubobjectModel.h"
#include "Replication/Editor/Model/TransactionalReplicationStreamModel.h"
#include "Replication/Editor/View/ObjectEditor/SDefaultReplicationStreamEditor.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertClientSharedSlate
{
	TSharedRef<ConcertSharedSlate::ISubobjectModel> CreateSubobjectModelForComponentHierarchy()
	{
		return MakeShared<FComponentHierarchySubobjectModel>();
	}

	TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel(
		TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> BaseModel,
		UObject& OwnerObject
		)
	{
		return MakeShared<ConcertSharedSlate::FTransactionalReplicationStreamModel>(
			MoveTemp(BaseModel),
			OwnerObject
			);
	}

	TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel()
	{
		const EObjectFlags Flags = RF_Transient | RF_Transactional;
		UReplicationStreamObject* Object = NewObject<UReplicationStreamObject>(GetTransientPackage(), NAME_None, Flags);

		TAttribute<FConcertObjectReplicationMap*> Attribute = TAttribute<FConcertObjectReplicationMap*>::CreateLambda([WeakPtr = TWeakObjectPtr<UReplicationStreamObject>(Object)]() -> FConcertObjectReplicationMap* 
		{
			if (WeakPtr.IsValid())
			{
				return &WeakPtr->ReplicationMap;
			}
			return nullptr;
		});

		return CreateTransactionalStreamModel(ConcertSharedSlate::CreateBaseStreamModel(MoveTemp(Attribute)), *Object);
	}
	
	TSharedRef<ConcertSharedSlate::IReplicationStreamEditor> CreateDefaultStreamEditor(ConcertSharedSlate::FCreateEditorParams Params)
	{
		return SNew(SDefaultReplicationStreamEditor, MoveTemp(Params));
	}
}