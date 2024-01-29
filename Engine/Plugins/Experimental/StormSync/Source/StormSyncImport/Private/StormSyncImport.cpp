// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncImport.h"

#include "StormSyncImportLog.h"

#define LOCTEXT_NAMESPACE "FStormSyncImportModule"

void FStormSyncImportModule::StartupModule()
{
	STORM_SYNC_IMPORT_LOG(Verbose, TEXT("Started StormSyncImport module ..."))
}

void FStormSyncImportModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FStormSyncImportModule, StormSyncImport)