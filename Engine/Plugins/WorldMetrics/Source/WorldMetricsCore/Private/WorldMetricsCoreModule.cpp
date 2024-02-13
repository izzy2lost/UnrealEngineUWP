// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "WorldMetricsDebug.h"

#if WITH_WORLDMETRICS_DEBUG
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "WorldMetricsConsoleCommands.h"
#endif	// WITH_WORLDMETRICS_DEBUG

namespace UE::WorldMetrics
{

class FWorldMetricsCoreModule : public IModuleInterface
{
protected:
#if WITH_WORLDMETRICS_DEBUG
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

	void OnPostEngineInit();

	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	TArray<IConsoleObject*> ConsoleObjects;
#endif	// WITH_WORLDMETRICS_DEBUG
};

#if WITH_WORLDMETRICS_DEBUG

void FWorldMetricsCoreModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FWorldMetricsCoreModule::OnPostEngineInit);
}

void FWorldMetricsCoreModule::ShutdownModule()
{
	UnregisterConsoleCommands();
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FWorldMetricsCoreModule::OnPostEngineInit()
{
	RegisterConsoleCommands();
}

void FWorldMetricsCoreModule::RegisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	RegisterCommonMetricConsoleCommands(ConsoleManager, ConsoleObjects);
}

void FWorldMetricsCoreModule::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleObject* ConsoleObject : ConsoleObjects)
	{
		if (ConsoleObject)
		{
			ConsoleManager.UnregisterConsoleObject(ConsoleObject);
		}
	}
	ConsoleObjects.Reset();
}
#endif	// WITH_WORLDMETRICS_DEBUG

}  // namespace UE::WorldMetrics

IMPLEMENT_MODULE(UE::WorldMetrics::FWorldMetricsCoreModule, WorldMetricsCore)
