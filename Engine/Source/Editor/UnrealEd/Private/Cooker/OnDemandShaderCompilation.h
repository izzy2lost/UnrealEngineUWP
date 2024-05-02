// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"

class UMaterialInterface;

namespace UE::Cook
{

class FODSCClientData
{
public:
	void OnClientConnected(const void* ConnectionPtr);
	void OnClientDisconnected(const void* ConnectionPtr); 
	void KeepClientPersistentData(const void* ConnectionPtr, const TArray<TStrongObjectPtr<UMaterialInterface>>& LoadedMaterialsToRecompile);

private:
	struct FODSCClientPersistentData
	{
		TSet<TStrongObjectPtr<UMaterialInterface> > MaterialsKeptAlive;
	};

	TMap<const void*, FODSCClientPersistentData> ODSCClientPersistentDataMap;
	FCriticalSection ODSCClientPersistentDataMapLock;
};

}