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
static uint32 GParamIdSandboxedThreadId = MAX_uint32;
static bool bGParamIdSandboxed = false;
#endif

static FParamIdGlobalData& GetParamIdData()
{
#if WITH_DEV_AUTOMATION_TESTS		
	if (bGParamIdSandboxed && GParamIdSandboxedThreadId == FPlatformTLS::GetCurrentThreadId())
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
	check(bGParamIdSandboxed == false);
	bGParamIdSandboxed = true;
	GParamIdSandboxedThreadId = FPlatformTLS::GetCurrentThreadId();
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamIdToName.Empty();
}

void FParamId::EndTestSandbox()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	check(bGParamIdSandboxed == true);
	bGParamIdSandboxed = false;
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamIdToName.Empty();
}
#endif

}