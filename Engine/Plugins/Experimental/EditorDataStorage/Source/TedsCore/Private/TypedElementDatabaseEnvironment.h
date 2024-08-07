// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicColumnGenerator.h"
#include "MassEntityManager.h"
#include "MassProcessingPhaseManager.h"
#include "TypedElementDatabaseCommandBuffer.h"
#include "TypedElementDatabaseScratchBuffer.h"
#include "TypedElementDatabaseIndexTable.h"
#include "Memento/TypedElementMementoSystem.h"
#include "Queries/TypedElementExtendedQueryStore.h"

class UTypedElementDatabase;

namespace UE::Editor::DataStorage
{
	class FEnvironment final
	{
	public:
		FEnvironment(UTypedElementDatabase& InDataStorage,
			FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager);

		Legacy::FCommandBuffer& GetDirectDeferredCommands();
		const Legacy::FCommandBuffer& GetDirectDeferredCommands() const;

		FIndexTable& GetIndexTable();
		const FIndexTable& GetIndexTable() const;

		FScratchBuffer& GetScratchBuffer();
		const FScratchBuffer& GetScratchBuffer() const;

		FExtendedQueryStore& GetQueryStore();
		const FExtendedQueryStore& GetQueryStore() const;

		UTypedElementMementoSystem& GetMementoSystem();
		const UTypedElementMementoSystem& GetMementoSystem() const;

		FMassEntityManager& GetMassEntityManager();
		const FMassEntityManager& GetMassEntityManager() const;

		FMassArchetypeHandle LookupMassArchetype(TypedElementDataStorage::TableHandle TableHandle) const;

		FMassProcessingPhaseManager& GetMassPhaseManager();
		const FMassProcessingPhaseManager& GetMassPhaseManager() const;

		FConstSharedStruct GenerateDynamicTag(const FDynamicTag& Tag, const FName& Value);
		const UScriptStruct* GenerateColumnType(const FDynamicTag& Tag);

		void NextUpdateCycle();
		uint64 GetUpdateCycleId() const;

	private:
		UTypedElementDatabase& DataStorage;
		Legacy::FCommandBuffer DirectDeferredCommands;
		FIndexTable IndexTable;
		FScratchBuffer ScratchBuffer;
		FExtendedQueryStore Queries;
		UTypedElementMementoSystem MementoSystem;
		FDynamicColumnGenerator DynamicColumnGenerator;
		FDynamicTagManager DynamicTagManager;

		FMassEntityManager& MassEntityManager;
		FMassProcessingPhaseManager& MassPhaseManager;

		uint64 UpdateCycleId = 0;
	};
} // namespace UE::Editor::DataStorage
