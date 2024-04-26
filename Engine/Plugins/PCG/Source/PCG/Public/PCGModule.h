// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGDataVisualizationRegistry.h"

#include "Modules/ModuleInterface.h"
#include "Stats/Stats.h"

// Logs
PCG_API DECLARE_LOG_CATEGORY_EXTERN(LogPCG, Log, All);

struct FPCGContext;
class IPCGDataVisualization;

namespace PCGLog
{
	/** Convenience function that would either log error on the graph if there is a context, or in the console if not. */
	PCG_API void LogErrorOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);
	/** Convenience function that would either log warning on the graph if there is a context, or in the console if not. */
	PCG_API void LogWarningOnGraph(const FText& InMsg, const FPCGContext* InContext = nullptr);
}

namespace PCGEngineShowFlags
{
	static constexpr TCHAR Debug[] = TEXT("PCGDebug");
}

// Stats
DECLARE_STATS_GROUP(TEXT("PCG"), STATGROUP_PCG, STATCAT_Advanced);

// CVars

class FPCGModule final : public IModuleInterface
{
public:
	//~ IModuleInterface implementation
#if WITH_EDITOR
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#endif
	virtual bool SupportsDynamicReloading() override { return true; }
	//~ End IModuleInterface implementation

#if WITH_EDITOR
	PCG_API static FPCGModule& GetPCGModuleChecked();

private:
	void RegisterNativeElementDeterminismTests();
	void DeregisterNativeElementDeterminismTests();
#endif

#if WITH_EDITOR
public:
	static const FPCGDataVisualizationRegistry& GetConstPCGDataVisualizationRegistry() { return GetPCGModuleChecked().PCGDataVisualizationRegistry; }
	static FPCGDataVisualizationRegistry& GetMutablePCGDataVisualizationRegistry() { return GetPCGModuleChecked().PCGDataVisualizationRegistry; }

private:
	FPCGDataVisualizationRegistry PCGDataVisualizationRegistry;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#endif
