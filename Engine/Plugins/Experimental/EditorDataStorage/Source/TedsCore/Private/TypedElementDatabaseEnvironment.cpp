// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseEnvironment.h"
#include "TypedElementDatabase.h"

namespace UE::Editor::DataStorage
{
	FEnvironment::FEnvironment(UTypedElementDatabase& InDataStorage,
		FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager)
		: DataStorage(InDataStorage)
		, DirectDeferredCommands(*this)
		, MementoSystem(InDataStorage)
		, DynamicTagManager(DynamicColumnGenerator)
		, MassEntityManager(InMassEntityManager)
		, MassPhaseManager(InMassPhaseManager)
	{
	}

	Legacy::FCommandBuffer& FEnvironment::GetDirectDeferredCommands()
	{
		return DirectDeferredCommands;
	}

	const Legacy::FCommandBuffer& FEnvironment::GetDirectDeferredCommands() const
	{
		return DirectDeferredCommands;
	}

	FIndexTable& FEnvironment::GetIndexTable()
	{
		return IndexTable;
	}

	const FIndexTable& FEnvironment::GetIndexTable() const
	{
		return IndexTable;
	}

	FScratchBuffer& FEnvironment::GetScratchBuffer()
	{
		return ScratchBuffer;
	}

	const FScratchBuffer& FEnvironment::GetScratchBuffer() const
	{
		return ScratchBuffer;
	}

	FExtendedQueryStore& FEnvironment::GetQueryStore()
	{
		return Queries;
	}

	const FExtendedQueryStore& FEnvironment::GetQueryStore() const
	{
		return Queries;
	}

	UTypedElementMementoSystem& FEnvironment::GetMementoSystem()
	{
		return MementoSystem;
	}

	const UTypedElementMementoSystem& FEnvironment::GetMementoSystem() const
	{
		return MementoSystem;
	}

	FMassEntityManager& FEnvironment::GetMassEntityManager()
	{
		return MassEntityManager;
	}

	const FMassEntityManager& FEnvironment::GetMassEntityManager() const
	{
		return MassEntityManager;
	}

	FMassArchetypeHandle FEnvironment::LookupMassArchetype(TypedElementDataStorage::TableHandle TableHandle) const
	{
		return DataStorage.LookupArchetype(TableHandle);
	}

	FMassProcessingPhaseManager& FEnvironment::GetMassPhaseManager()
	{
		return MassPhaseManager;
	}

	const FMassProcessingPhaseManager& FEnvironment::GetMassPhaseManager() const
	{
		return MassPhaseManager;
	}

	FConstSharedStruct FEnvironment::GenerateDynamicTag(const FDynamicTag& Tag, const FName& Value)
	{
		return DynamicTagManager.GenerateDynamicTag(Tag, Value);
	}

	const UScriptStruct* FEnvironment::GenerateColumnType(const FDynamicTag& Tag)
	{
		return DynamicTagManager.GenerateColumnType(Tag);
	}

	void FEnvironment::NextUpdateCycle()
	{
		Queries.UpdateActivatableQueries();
		ScratchBuffer.BatchDelete();
		UpdateCycleId++;
	}

	uint64 FEnvironment::GetUpdateCycleId() const
	{
		return UpdateCycleId;
	}
} // namespace UE::Editor::DataStorage
