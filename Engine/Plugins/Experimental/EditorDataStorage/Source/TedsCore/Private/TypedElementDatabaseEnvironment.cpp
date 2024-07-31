// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseEnvironment.h"
#include "TypedElementDatabase.h"

FTypedElementDatabaseEnvironment::FTypedElementDatabaseEnvironment(UTypedElementDatabase& InDataStorage,
	FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager)
	: DataStorage(InDataStorage)
	, DirectDeferredCommands(*this)
	, MementoSystem(InDataStorage)
	, DynamicTagManager(DynamicColumnGenerator)
	, MassEntityManager(InMassEntityManager)
	, MassPhaseManager(InMassPhaseManager)
{
}

FTypedElementDatabaseCommandBuffer& FTypedElementDatabaseEnvironment::GetDirectDeferredCommands()
{
	return DirectDeferredCommands;
}

const FTypedElementDatabaseCommandBuffer& FTypedElementDatabaseEnvironment::GetDirectDeferredCommands() const
{
	return DirectDeferredCommands;
}

FTypedElementDatabaseIndexTable& FTypedElementDatabaseEnvironment::GetIndexTable()
{
	return IndexTable;
}

const FTypedElementDatabaseIndexTable& FTypedElementDatabaseEnvironment::GetIndexTable() const
{
	return IndexTable;
}

FTypedElementDatabaseScratchBuffer& FTypedElementDatabaseEnvironment::GetScratchBuffer()
{
	return ScratchBuffer;
}

const FTypedElementDatabaseScratchBuffer& FTypedElementDatabaseEnvironment::GetScratchBuffer() const
{
	return ScratchBuffer;
}

FTypedElementExtendedQueryStore& FTypedElementDatabaseEnvironment::GetQueryStore()
{
	return Queries;
}

const FTypedElementExtendedQueryStore& FTypedElementDatabaseEnvironment::GetQueryStore() const
{
	return Queries;
}

UTypedElementMementoSystem& FTypedElementDatabaseEnvironment::GetMementoSystem()
{
	return MementoSystem;
}

const UTypedElementMementoSystem& FTypedElementDatabaseEnvironment::GetMementoSystem() const
{
	return MementoSystem;
}

FMassEntityManager& FTypedElementDatabaseEnvironment::GetMassEntityManager()
{
	return MassEntityManager;
}

const FMassEntityManager& FTypedElementDatabaseEnvironment::GetMassEntityManager() const
{
	return MassEntityManager;
}

FMassArchetypeHandle FTypedElementDatabaseEnvironment::LookupMassArchetype(TypedElementDataStorage::TableHandle TableHandle) const
{
	return DataStorage.LookupArchetype(TableHandle);
}

FMassProcessingPhaseManager& FTypedElementDatabaseEnvironment::GetMassPhaseManager()
{
	return MassPhaseManager;
}

const FMassProcessingPhaseManager& FTypedElementDatabaseEnvironment::GetMassPhaseManager() const
{
	return MassPhaseManager;
}
FConstSharedStruct GenerateDynamicTag(const UE::EditorDataStorage::FDynamicTag& Tag, const FName& Value);
const UScriptStruct* GenerateColumnType(const UE::EditorDataStorage::FDynamicTag& Tag);
FConstSharedStruct FTypedElementDatabaseEnvironment::GenerateDynamicTag(const UE::EditorDataStorage::FDynamicTag& Tag, const FName& Value)
{
	return DynamicTagManager.GenerateDynamicTag(Tag, Value);
}

const UScriptStruct* FTypedElementDatabaseEnvironment::GenerateColumnType(const UE::EditorDataStorage::FDynamicTag& Tag)
{
	return DynamicTagManager.GenerateColumnType(Tag);
}

void FTypedElementDatabaseEnvironment::NextUpdateCycle()
{
	Queries.UpdateActivatableQueries();
	ScratchBuffer.BatchDelete();
	UpdateCycleId++;
}

uint64 FTypedElementDatabaseEnvironment::GetUpdateCycleId() const
{
	return UpdateCycleId;
}
