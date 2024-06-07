// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectFixupToolTedsQueries.h"

#include "Elements/Columns/TypedElementAlertColumns.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Engine/World.h"
#include "UObject/PropertyBagRepository.h"
#include "InstanceDataObjectFixupToolModule.h"

void UInstanceDataObjectFixupToolTedsQueryFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace UE;
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Add fix-up tool to serialization placeholder alerts"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage)),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object)
			{
				Context.AddColumn(Row, FTypedElementAlertActionColumn{ .Action = ShowFixUpToolForPlaceholders });
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag, FTypedElementAlertColumn, FTypedElementPropertyBagPlaceholderTag>()
			.None<FTypedElementAlertActionColumn>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Add fix-up tool to serialization loose property alerts"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage)),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object)
			{
				Context.AddColumn(Row, FTypedElementAlertActionColumn{ .Action = ShowFixUpToolForLooseProperties });
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag, FTypedElementAlertColumn, FTypedElementLoosePropertyTag>()
			.None<FTypedElementAlertActionColumn>()
		.Compile());
}

void UInstanceDataObjectFixupToolTedsQueryFactory::ShowFixUpToolForPlaceholders(TypedElementDataStorage::RowHandle Row)
{
	ShowFixUpTool(Row, false);
}

void UInstanceDataObjectFixupToolTedsQueryFactory::ShowFixUpToolForLooseProperties(TypedElementDataStorage::RowHandle Row)
{
	ShowFixUpTool(Row, true);
}

void UInstanceDataObjectFixupToolTedsQueryFactory::ShowFixUpTool(TypedElementDataStorage::RowHandle Row, bool bRecurseIntoObject)
{
	ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
	if (FTypedElementUObjectColumn* ObjectColumn = DataStorage->GetColumn<FTypedElementUObjectColumn>(Row))
	{
		if (bRecurseIntoObject)
		{
			UObject* Owner = ObjectColumn->Object.Get();
			UE::FPropertyBagRepository::Get().FindNestedInstanceDataObject(Owner, true,
				[Owner](UObject* NestedObject)
				{
					FInstanceDataObjectFixupToolModule::Get().CreateInstanceDataObjectFixupDialog({NestedObject}, Owner);
				});
		}
		else
		{
			if (TObjectPtr<UObject> InstanceDataObject = UE::FPropertyBagRepository::Get().FindInstanceDataObject(ObjectColumn->Object.Get()))
			{
				FInstanceDataObjectFixupToolModule::Get().CreateInstanceDataObjectFixupDialog({ InstanceDataObject });
			}
		}
	}
}
