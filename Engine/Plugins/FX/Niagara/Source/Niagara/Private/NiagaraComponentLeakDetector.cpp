// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponentLeakDetector.h"
#include "NiagaraDebugHud.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"

#if WITH_NIAGARA_LEAK_DETECTOR

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraLeakDetector, Log, All);

namespace NiagaraComponentLeakDetectorPrivate
{
	static bool		GEnabled = false;
	static int32	GImmediateReport = 2;
	static int32	GGCReport = 1;
	static float	GTickDeltaSeconds = 1.0f;
	static int32	GGrowthCountThreshold = 16;
	static float	GDebugMessageTime = 5.0f;

	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("fx.Niagara.LeakDetector.Enabled"),
		GEnabled,
		TEXT("Enables or disables the leak detector."),
		ECVF_Default
	);

	static FAutoConsoleVariableRef CVarTickDeltaSeconds(
		TEXT("fx.Niagara.LeakDetector.TickDeltaSeconds"),
		GTickDeltaSeconds,
		TEXT("The time in seconds that must pass before we sample the component information."),
		ECVF_Default
	);

	static FAutoConsoleVariableRef CVarGrowthCountThreshold(
		TEXT("fx.Niagara.LeakDetector.GrowthCountThreshold"),
		GGrowthCountThreshold,
		TEXT("We need to see growth this many times without a drop in count before we consider it a leak."),
		ECVF_Default
	);

	static FAutoConsoleVariableRef CVarImmediateReport(
		TEXT("fx.Niagara.LeakDetector.ImmediateReport"),
		GImmediateReport,
		TEXT("Controls how we report leaks as we scan information.\n")
		TEXT("0 - Never report during gameplay.\n")
		TEXT("1 - Report all leaks.")
		TEXT("2 - Report active leaks only. (default).\n"),
		ECVF_Default
	);

	static FAutoConsoleVariableRef CVarGGCReport(
		TEXT("fx.Niagara.LeakDetector.GCReport"),
		GGCReport,
		TEXT("Controls how we report leak after GC\n")
		TEXT("0 - Never report after GC.\n")
		TEXT("1 - Report all leaks. (default)")
		TEXT("2 - Report active leaks only.\n"),
		ECVF_Default
	);

	static FAutoConsoleVariableRef CVarDebugMessageTime(
		TEXT("fx.Niagara.LeakDetector.DebugMessageTime"),
		GDebugMessageTime,
		TEXT("Time we display the debug message for on screen."),
		ECVF_Default
	);

	void AddLeakWarning(UWorld* World, FName MessageKey, const FString& Message)
	{
		UE_LOG(LogNiagaraLeakDetector, Warning, TEXT("%s"), *Message);
#if WITH_NIAGARA_DEBUGGER
		if (FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World))
		{
			if (FNiagaraDebugHud* DebugHud = WorldManager->GetNiagaraDebugHud())
			{
				DebugHud->AddMessage(MessageKey, FNiagaraDebugMessage(ENiagaraDebugMessageType::Warning, Message, GDebugMessageTime));
			}
		}
#endif
	}
}

void FNiagaraComponentLeakDetector::Tick(UWorld* World)
{
	using namespace NiagaraComponentLeakDetectorPrivate;

	if (!GEnabled)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime < NextUpdateTime)
	{
		return;
	}
	NextUpdateTime = CurrentTime + double(GTickDeltaSeconds);

	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		UNiagaraSystem* System = Component ? Component->GetAsset() : nullptr;
		if (!System)
		{
			continue;
		}

		if ( Component->GetWorld() != World || Component->PoolingMethod == ENCPoolMethod::FreeInPool)
		{
			continue;
		}

		const FName SystemName = System->GetFName();
		FSystemData& SystemData = PerSystemData.FindOrAdd(SystemName);
		++SystemData.TotalCurrCount;
		SystemData.ActiveCurrCount += Component->IsActive();
	}

	const bool bReportTotalLeaks = GImmediateReport == 1;
	const bool bReportActiveLeaks = GImmediateReport != 0;

	for (auto SystemDataIt=PerSystemData.CreateIterator(); SystemDataIt; ++SystemDataIt)
	{
		const FName SystemName = SystemDataIt.Key();
		FSystemData& SystemData = SystemDataIt.Value();

		// Monitor total count
		SystemData.TotalGrowthCounter += SystemData.TotalCurrCount > SystemData.TotalPrevCount ? 1 : 0;
		SystemData.TotalShrinkCounter += SystemData.TotalCurrCount < SystemData.TotalPrevCount ? 1 : 0;
		SystemData.TotalPrevCount = SystemData.TotalCurrCount;
		SystemData.TotalCurrCount = 0;

		// Monitor active count
		SystemData.ActiveGrowthCounter += SystemData.ActiveCurrCount > SystemData.ActivePrevCount ? 1 : 0;
		SystemData.ActiveShrinkCounter += SystemData.ActiveCurrCount < SystemData.ActivePrevCount ? 1 : 0;
		SystemData.ActivePrevCount = SystemData.ActiveCurrCount;
		SystemData.ActiveCurrCount = 0;

		// Report any leaks
		ReportLeak(World, SystemName, SystemData, bReportTotalLeaks, bReportActiveLeaks);
	}
}

void FNiagaraComponentLeakDetector::ReportLeaks(UWorld* World)
{
	using namespace NiagaraComponentLeakDetectorPrivate;

	if (!GEnabled || GGCReport == 0)
	{
		return;
	}

	const bool bReportTotalLeaks = GGCReport == 1;
	const bool bReportActiveLeaks = GGCReport != 0;

	for (auto SystemDataIt=PerSystemData.CreateIterator(); SystemDataIt; ++SystemDataIt)
	{
		const FName SystemName = SystemDataIt.Key();
		FSystemData& SystemData = SystemDataIt.Value();
		ReportLeak(World, SystemName, SystemData, bReportTotalLeaks, bReportActiveLeaks);
	}
}

void FNiagaraComponentLeakDetector::ReportLeak(UWorld* World, FName SystemName, FSystemData& SystemData, bool bReportTotalLeaks, bool bReportActiveLeaks)
{
	using namespace NiagaraComponentLeakDetectorPrivate;

	if (bReportTotalLeaks)
	{
		if (!SystemData.bTotalHasWarned)
		{
			if (SystemData.TotalShrinkCounter == 0 && SystemData.TotalGrowthCounter >= GGrowthCountThreshold)
			{
				SystemData.bTotalHasWarned = true;
				AddLeakWarning(World, SystemName, FString::Printf(TEXT("Potential componment leak System(%s) (Total:%d), please investigate."), *SystemName.ToString(), SystemData.TotalPrevCount));
			}
		}
		else if (!SystemData.bTotalFalseWarning && (SystemData.TotalShrinkCounter > 0))
		{
			SystemData.bTotalFalseWarning = true;
			AddLeakWarning(World, SystemName, FString::Printf(TEXT("Potential invalid component leak reported for System(%s) (Total:%d)."), *SystemName.ToString(), SystemData.TotalPrevCount));
		}
	}

	if (bReportActiveLeaks)
	{
		if (!SystemData.bActiveHasWarned)
		{
			if (SystemData.ActiveShrinkCounter == 0 && SystemData.ActiveGrowthCounter >= GGrowthCountThreshold)
			{
				SystemData.bActiveHasWarned = true;
				AddLeakWarning(World, SystemName, FString::Printf(TEXT("Potential Active Component leak System(%s) (Active:%d), please investigate."), *SystemName.ToString(), SystemData.ActivePrevCount));
			}
		}
		else if ( !SystemData.bActiveFalseWarning && (SystemData.ActiveShrinkCounter > 0) )
		{
			SystemData.bActiveFalseWarning = true;
			AddLeakWarning(World, SystemName, FString::Printf(TEXT("Potential invalid active component leak reported for System(% s) (Active: % d)."), *SystemName.ToString(), SystemData.ActivePrevCount));
		}
	}
}

#endif //WITH_NIAGARA_LEAK_DETECTOR
