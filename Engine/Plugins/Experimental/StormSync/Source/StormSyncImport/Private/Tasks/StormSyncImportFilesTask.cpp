// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StormSyncImportFilesTask.h"

#include "StormSyncCoreDelegates.h"
#include "StormSyncImportLog.h"
#include "Subsystems/StormSyncImportSubsystem.h"

void FStormSyncImportFilesTask::Run()
{
	STORM_SYNC_IMPORT_LOG(Display, TEXT("FStormSyncImportFilesTask::Run for %s"), *Filename)
	UStormSyncImportSubsystem::Get().PerformFileImport(Filename);
	FStormSyncCoreDelegates::OnFileImported.Broadcast(Filename);
}
