// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StormSyncImportBufferTask.h"

#include "StormSyncImportLog.h"
#include "Subsystems/StormSyncImportSubsystem.h"

void FStormSyncImportBufferTask::Run()
{
	if (!Buffer.IsValid())
	{
		STORM_SYNC_IMPORT_LOG(Error, TEXT("FStormSyncImportBufferTask::Run failed on invalid buffer"))
		return;
	}

	STORM_SYNC_IMPORT_LOG(Display, TEXT("FStormSyncImportBufferTask::Run for buffer of size %d"), Buffer->Num())
	UStormSyncImportSubsystem::Get().PerformBufferImport(PackageDescriptor, MoveTemp(Buffer));
}
