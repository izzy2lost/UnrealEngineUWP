// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "MassProcessingPhaseManager.h"
#include "TypedElementDatabaseCommandBuffer.h"
#include "TypedElementDatabaseScratchBuffer.h"
#include "TypedElementDatabaseIndexTable.h"
#include "Queries/TypedElementExtendedQueryStore.h"

class FTypedElementDatabaseEnvironment final
{
public:
	FTypedElementDatabaseEnvironment(FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager);

	FTypedElementDatabaseCommandBuffer& GetDirectDeferredCommands();
	const FTypedElementDatabaseCommandBuffer& GetDirectDeferredCommands() const;
	
	FTypedElementDatabaseIndexTable& GetIndexTable();
	const FTypedElementDatabaseIndexTable& GetIndexTable() const;

	FTypedElementDatabaseScratchBuffer& GetScratchBuffer();
	const FTypedElementDatabaseScratchBuffer& GetScratchBuffer() const;

	FTypedElementExtendedQueryStore& GetQueryStore();
	const FTypedElementExtendedQueryStore& GetQueryStore() const;

	FMassEntityManager& GetMassEntityManager();
	const FMassEntityManager& GetMassEntityManager() const;
	
	FMassProcessingPhaseManager& GetMassPhaseManager();
	const FMassProcessingPhaseManager& GetMassPhaseManager() const;

	void NextUpdateCycle();
	uint64 GetUpdateCycleId() const;

private:
	FTypedElementDatabaseCommandBuffer DirectDeferredCommands;
	FTypedElementDatabaseIndexTable IndexTable;
	FTypedElementDatabaseScratchBuffer ScratchBuffer;
	FTypedElementExtendedQueryStore Queries;
	
	FMassEntityManager& MassEntityManager;
	FMassProcessingPhaseManager& MassPhaseManager;

	uint64 UpdateCycleId = 0;
};
