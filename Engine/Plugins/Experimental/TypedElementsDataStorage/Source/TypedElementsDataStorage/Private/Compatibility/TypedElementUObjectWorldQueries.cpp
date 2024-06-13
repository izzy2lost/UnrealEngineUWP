// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementUObjectWorldQueries.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Engine/World.h"

void UTypedElementUObjectWorldFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterAddWorldColumn(DataStorage);
	RegisterUpdateOrRemoveWorldColumn(DataStorage);
}

void UTypedElementUObjectWorldFactory::RegisterAddWorldColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Add world column to UObject"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
			.ForceToGameThread(true),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object)
			{
				// Not all objects, in particular actors, are always correctly cleaned up, resulting in dangling
				// pointers in TEDS.
				if (UObject* ObjectInstance = Object.Object.Get())
				{
					if (UWorld* World = ObjectInstance->GetWorld())
					{
						Context.AddColumn(Row, FTypedElementWorldColumn{ .World = World });
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag>()
			.None<FTypedElementWorldColumn, FTypedElementClassDefaultObjectTag>()
		.Compile()
	);
}

void UTypedElementUObjectWorldFactory::RegisterUpdateOrRemoveWorldColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync UObject's world to column"),
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
			.ForceToGameThread(true),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object, FTypedElementWorldColumn& World)
			{
				// Not all objects, in particular actors, are always correctly cleaned up, resulting in dangling
				// pointers in TEDS.
				if (UObject* ObjectInstance = Object.Object.Get())
				{
					if (UWorld* UObjectWorld = ObjectInstance->GetWorld())
					{
						World.World = UObjectWorld;
						return;
					}
				}
				Context.RemoveColumns<FTypedElementWorldColumn>(Row);
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag>()
		.Compile()
	);
}
