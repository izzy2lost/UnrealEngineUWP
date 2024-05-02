// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/OnDemandShaderCompilation.h"

#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"


namespace UE::Cook
{

void FODSCClientData::OnClientConnected(const void* ConnectionPtr)
{
	FScopeLock PollablesScopeLock(&ODSCClientPersistentDataMapLock);
	ODSCClientPersistentDataMap.FindOrAdd(ConnectionPtr);
}

void FODSCClientData::OnClientDisconnected(const void* ConnectionPtr)
{
	FScopeLock PollablesScopeLock(&ODSCClientPersistentDataMapLock);

	FODSCClientPersistentData* ODSCClientPersistentData = ODSCClientPersistentDataMap.Find(ConnectionPtr);
	if (ODSCClientPersistentData)
	{
		// We can delete TStrongObjectPtr only on the game thread
		TSet<TStrongObjectPtr<UMaterialInterface>>* MaterialsToRemove = new TSet<TStrongObjectPtr<UMaterialInterface>>;
		*MaterialsToRemove = MoveTemp(ODSCClientPersistentData->MaterialsKeptAlive);
		AsyncTask(ENamedThreads::GameThread, [MaterialsToRemove]()
				  {
					  delete MaterialsToRemove;
				  });

		ODSCClientPersistentDataMap.Remove(ConnectionPtr);
	}
}

void FODSCClientData::KeepClientPersistentData(const void* ConnectionPtr, const TArray<TStrongObjectPtr<UMaterialInterface>>& LoadedMaterialsToRecompile)
{
	FScopeLock PollablesScopeLock(&ODSCClientPersistentDataMapLock);
	FODSCClientPersistentData* ODSCClientPersistentData = ODSCClientPersistentDataMap.Find(ConnectionPtr);
	if (ODSCClientPersistentData)
	{
		for (const TStrongObjectPtr<UMaterialInterface>& MaterialInterface : LoadedMaterialsToRecompile)
		{
			ODSCClientPersistentData->MaterialsKeptAlive.Add(MaterialInterface);
		}
	}
}

}