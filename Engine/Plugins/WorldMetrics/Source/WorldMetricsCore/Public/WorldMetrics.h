// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"

class UWorld;
class UWorldMetricInterface;
class UWorldMetricsExtension;
class UWorldMetricsSubsystem;

namespace UE::WorldMetrics
{

/**
 * Gets the World Metrics subsystem. This method expects the parameter world to have the subsystem and will log an error
 * if it isn't available.
 *
 * @return A valid pointer to the world metric subsystem or nullptr otherwise.
 */
[[nodiscard]] WORLDMETRICSCORE_API UWorldMetricsSubsystem* GetSubsystem(const UWorld* World);

/**
 * Gets a world metric from the parameter world's World Metrics subsystem using the parameter metric
 * class.
 *
 * @param World: the world owning the destination World Metrics subsystem.
 * @param MetricClass: the class of the world metric to be retrieved.
 * @return A valid pointer to the world metric if found, and nullptr otherwise.
 */
[[nodiscard]] WORLDMETRICSCORE_API UWorldMetricInterface* GetMetric(
	const UWorld* World,
	const TSubclassOf<UWorldMetricInterface>& MetricClass);

template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
[[nodiscard]] MetricClass* GetMetric(const UWorld* World)
{
	return static_cast<MetricClass*>(GetMetric(World, MetricClass::StaticClass()));
}

/**
 * Adds a new world metric to the parameter world's World Metrics subsystem with the parameter metric class.
 * A new world metric is instanced only if a metric with the same class doesn't already exist. The destination world
 * metrics subsystem becomes the owner of the created metric.
 *
 * @param World: the world owning the destination World Metrics subsystem.
 * @param MetricClass: the class of the world metric to be added.
 * @return true if a new metric is created and false otherwise.
 */
WORLDMETRICSCORE_API bool AddMetric(const UWorld* World, const TSubclassOf<UWorldMetricInterface>& MetricClass);

template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
bool AddMetric(const UWorld* World)
{
	return AddMetric(World, MetricClass::StaticClass());
}

/**
 * Removes a new world metric to the parameter world's World Metrics subsystem with the parameter metric class.
 * A world metric can only be removed if a metric of the class type exists.
 *
 * @param World: the world owning the destination World Metrics subsystem.
 * @param MetricClass: the class of the world metric to remove.
 * @return true if an existing metric is removed and false otherwise.
 */
WORLDMETRICSCORE_API bool RemoveMetric(const UWorld* World, const TSubclassOf<UWorldMetricInterface>& MetricClass);

template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
bool RemoveMetric(const UWorld* World)
{
	return RemoveMetric(World, MetricClass::StaticClass());
}

/**
 * Acquires an extension on behalf of a object. The extension will be created the first time it is acquired,
 * and conversely it will be destroyed once released by all owners.
 *
 * @param InOwner: The object acquiring the extension. Must be a UWorldMetricInterface or UWorldMetricExtension.
 * @param InExtensionClass: The class type of the extension.
 * @return A pointer to the extension. The lifetime of the extension is managed by the subsystem.
 */
WORLDMETRICSCORE_API UWorldMetricsExtension* AcquireExtension(
	UWorldMetricInterface* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

WORLDMETRICSCORE_API UWorldMetricsExtension* AcquireExtension(
	UWorldMetricsExtension* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

template <class ExtensionClass, class OwnerType UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
ExtensionClass* AcquireExtension(OwnerType* InOwner)
{
	return static_cast<ExtensionClass*>(AcquireExtension(InOwner, ExtensionClass::StaticClass()));
}

/**
 * Releases a metric's ownership of an extension. If this was the last reference the extension will be disabled.
 *
 * @param InOwner: The object whose ownership should be released.
 * @param InExtensionClass: The class type of the extension.
 */
WORLDMETRICSCORE_API bool ReleaseExtension(
	UWorldMetricInterface* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

WORLDMETRICSCORE_API bool ReleaseExtension(
	UWorldMetricsExtension* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

template <class ExtensionClass, class OwnerType UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
bool ReleaseExtension(OwnerType* InOwner)
{
	return ReleaseExtension(InOwner, ExtensionClass::StaticClass());
}

}  // namespace UE::WorldMetrics
