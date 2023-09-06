// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamId.h"
#include "Misc/ScopeRWLock.h"
#include "HAL/ThreadSingleton.h"

namespace UE::AnimNext
{

static FRWLock GParamIdLock;

struct FParamIdGlobalData
{
	TArray<FName> ParamIdToName;
	TMap<FName, uint32> NameToParamId;
};

static FParamIdGlobalData GParamIdGlobalData;

#if WITH_DEV_AUTOMATION_TESTS
static FParamIdGlobalData GSandboxedParamIdGlobalData;
static std::atomic<bool> bGParamIdSandboxed = false;
#endif

static FParamIdGlobalData& GetParamIdData()
{
#if WITH_DEV_AUTOMATION_TESTS		
	if (bGParamIdSandboxed.load() == true)
	{
		return GSandboxedParamIdGlobalData;
	}
	else
#endif
	{
		return GParamIdGlobalData;
	}
}

FParamId::FParamId(FName InName)
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	if (const uint32* FoundIndex = GetParamIdData().NameToParamId.Find(InName))
	{
		ParameterIndex = *FoundIndex;
	}
	else
	{
		ParameterIndex = GetParamIdData().ParamIdToName.Num();
		GetParamIdData().NameToParamId.Add(InName, ParameterIndex);
		GetParamIdData().ParamIdToName.Add(InName);
	}
}

FName FParamId::ToName() const
{
	FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
	return GetParamIdData().ParamIdToName[ParameterIndex];
}

FParamId FParamId::GetMaxParamId()
{
	FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
	return FParamId((uint32)GetParamIdData().ParamIdToName.Num());
}

#if WITH_DEV_AUTOMATION_TESTS
void FParamId::BeginTestSandbox()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	check(bGParamIdSandboxed.load() == false);
	bGParamIdSandboxed.exchange(true);
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamIdToName.Empty();
}

void FParamId::EndTestSandbox()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	check(bGParamIdSandboxed.load() == true);
	bGParamIdSandboxed.exchange(false);
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamIdToName.Empty();
}
#endif

}