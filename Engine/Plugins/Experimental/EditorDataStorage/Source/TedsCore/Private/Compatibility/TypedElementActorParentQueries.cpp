// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorParentQueries.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameFramework/Actor.h"

namespace UE::TypedElementActorParentQueries::Local
{
	static bool bAddParentColumnToActors = false;
	
	// Cvar to allow the TEDS-Outliner to automatically take over the level editor's 4 Outliners instead of appearing as a separate tab
	static FAutoConsoleVariableRef CVarUseTEDSOutliner(
		TEXT("TEDS.AddParentColumnToActors"),
		bAddParentColumnToActors,
		TEXT("Mirror parent information for actors to TEDS (only works when set on startup)"));
};

void UTypedElementActorParentFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	if(UE::TypedElementActorParentQueries::Local::bAddParentColumnToActors)
	{
		RegisterAddParentColumn(DataStorage);
		RegisterUpdateOrRemoveParentColumn(DataStorage);
	}
}

void UTypedElementActorParentFactory::RegisterAddParentColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Add parent column to actor"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
			.ForceToGameThread(true),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Actor)
			{
				if (const AActor* ActorInstance = Cast<AActor>(Actor.Object))
				{
					if (AActor* Parent = ActorInstance->GetAttachParentActor())
					{
						uint64 IdHash = GenerateIndexHash(Parent);
						RowHandle ParentRow = Context.FindIndexedRow(IdHash);
						if (Context.IsRowAvailable(ParentRow))
						{
							Context.AddColumn(Row, FTableRowParentColumn{ .Parent = ParentRow });
						}
						else
						{
							Context.AddColumn(Row, FUnresolvedTableRowParentColumn{ .ParentIdHash = IdHash });
						}
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag, FTypedElementActorTag>()
			.None<FTableRowParentColumn, FUnresolvedTableRowParentColumn>()
		.Compile()
	);
}

void UTypedElementActorParentFactory::RegisterUpdateOrRemoveParentColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync actor's parent to column"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[](IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Actor, FTableRowParentColumn& Parent)
			{
				if (const AActor* ActorInstance = Cast<AActor>(Actor.Object))
				{
					if (AActor* ParentActor = ActorInstance->GetAttachParentActor())
					{
						uint64 IdHash = GenerateIndexHash(ParentActor);
						RowHandle ParentRow = Context.FindIndexedRow(IdHash);

						if (Parent.Parent != ParentRow)
						{
							if (Context.IsRowAvailable(ParentRow))
							{
								Parent.Parent = ParentRow;
							}
							else
							{
								Context.RemoveColumns<FTableRowParentColumn>(Row);
								Context.AddColumn(Row, FUnresolvedTableRowParentColumn{ .ParentIdHash = IdHash });
							}
						}
						return;
					}
				}
				Context.RemoveColumns<FTableRowParentColumn>(Row);
			}
		)
		.Where()
			.All<FTypedElementActorTag, FTypedElementSyncFromWorldTag>()
		.Compile()
	);
}
