// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/OnDemandShaderCompilation.h"

#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"


int32 GODSCShaderMapsLifetime = 25;
static FAutoConsoleVariableRef CVarODSCShaderMapsLifetime(
	TEXT("odsc.shadermaps.lifetime"),
	GODSCShaderMapsLifetime,
	TEXT("Controls how many shader recompiles can happen before deleting an unused shadermap. Higher values means more memory, but faster iteration time\n")
	TEXT("-1 means we never delete shader maps\n"),
	ECVF_Default);

int32 GODSCNumShaderMapsBeforeGC = 5000;
static FAutoConsoleVariableRef CVarODSCNumShaderMapsBeforeGC(
	TEXT("odsc.shadermaps.numbeforegc"),
	GODSCNumShaderMapsBeforeGC,
	TEXT("Controls how many shader maps we keep in memory before we start deleting them. Higher values means more memory, but faster iteration time\n")
	TEXT("-1 means we never delete shader maps\n"),
	ECVF_Default);


namespace UE::Cook
{

void FODSCClientData::OnClientConnected(const void* ConnectionPtr)
{
}

void FODSCClientData::OnClientDisconnected(const void* ConnectionPtr)
{
}

void FODSCClientData::PurgeMaterialShaderMaps(int32 Lifetime, int32 NumMapsToDelete, FODSCClientPersistentData::Value& MaterialShaderMapsKeptAlive)
{
	// Don't start counting shader lifetime until we go over the limit of shadermaps we want to keep in memory
	if (NumMapsToDelete <= 0)
	{
		return;
	}

	for (FODSCClientPersistentData::Value::TIterator Iter = MaterialShaderMapsKeptAlive.CreateIterator(); Iter; ++Iter)
	{
		int32& ShaderMapLifetime = Iter.Value();

		++ShaderMapLifetime;
		FMaterialShaderMap* MaterialShaderMap = Iter.Key();
		if ((Lifetime >= 0) && (ShaderMapLifetime > Lifetime) && (NumMapsToDelete > 0))
		{
			MaterialShaderMap->RemoveCompilingMaterialExternalDependency();
			Iter.RemoveCurrent();
			--NumMapsToDelete;
		}
	}
}

void FODSCClientData::FlushClientPersistentData(const void* ConnectionPtr)
{
	FScopeLock PollablesScopeLock(&ODSCClientPersistentDataLock);
	int32 NumMapsToDelete = (GODSCNumShaderMapsBeforeGC >= 0) ? ODSCClientPersistentData.MaterialShaderMapsKeptAlive.Num() - GODSCNumShaderMapsBeforeGC : 0;
	PurgeMaterialShaderMaps(GODSCShaderMapsLifetime, NumMapsToDelete, ODSCClientPersistentData.MaterialShaderMapsKeptAlive);
}

void FODSCClientData::KeepClientPersistentData(const void* ConnectionPtr, const TArray<TStrongObjectPtr<UMaterialInterface>>& LoadedMaterialsToRecompile)
{
	FScopeLock PollablesScopeLock(&ODSCClientPersistentDataLock);
	for (const TStrongObjectPtr<UMaterialInterface>& MaterialInterface : LoadedMaterialsToRecompile)
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex <= EMaterialQualityLevel::Num; ++QualityLevelIndex)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex <= ERHIFeatureLevel::Num; ++FeatureLevelIndex)
			{
				const FMaterialResource* MaterialResource = MaterialInterface->GetMaterialResource((ERHIFeatureLevel::Type)FeatureLevelIndex, (EMaterialQualityLevel::Type)QualityLevelIndex);
				if (!MaterialResource)
				{
					continue;
				}
				FMaterialShaderMap* CompilingShaderMap = FMaterialShaderMap::FindCompilingShaderMap(MaterialResource->GetGameThreadCompilingShaderMapId());
				if (!CompilingShaderMap)
				{
					continue;
				}

				int32& Lifetime = ODSCClientPersistentData.MaterialShaderMapsKeptAlive.FindOrAdd(CompilingShaderMap, -1);
				// On first insertion, we add the external dependency, and set to 0 such that the call to PurgeMaterialShaderMaps will always work with positive value
				if (Lifetime == -1)
				{
					CompilingShaderMap->AddCompilingMaterialExternalDependency();
				}
				Lifetime = 0;
			}
		}
	}
}

}