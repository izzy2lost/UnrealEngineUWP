// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "WorldMetricsDebug.h"

#if WITH_WORLDMETRICS_DEBUG

struct IConsoleManager;
class IConsoleObject;

namespace UE::WorldMetrics
{

/**
 * Registers common console commands for all World Metric classes.
 *
 * @param ConsoleManager A reference to the IConsoleManager interface, which the console commands will be registered
 * with.
 * @param OutConsoleObjects An array of IConsoleObject pointers to store references to registered console commands.
 */
void RegisterCommonMetricConsoleCommands(IConsoleManager& ConsoleManager, TArray<IConsoleObject*>& OutConsoleObjects);

}  // namespace UE::WorldMetrics

#endif	// WITH_WORLDMETRICS_DEBUG
