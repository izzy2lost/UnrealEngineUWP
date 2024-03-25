// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldMetrics.h"

#include "Engine/World.h"
#include "WorldMetricInterface.h"
#include "WorldMetricsExtension.h"
#include "WorldMetricsLog.h"
#include "WorldMetricsSubsystem.h"

//---------------------------------------------------------------------------------------------------------------------
// UE::WorldMetrics
//---------------------------------------------------------------------------------------------------------------------

namespace UE::WorldMetrics
{

namespace Private
{

/**
 * World Metrics subsystem inline helper that logs the caller function whenever the world or subsystem are null.
 *
 * @return A valid pointer to the world metric subsystem or nullptr otherwise.
 */
FORCEINLINE UWorldMetricsSubsystem* GetSubsystem(const UWorld* World)
{
	if (UNLIKELY(!World))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected null World"), __FUNCTION__);
		return nullptr;
	}

	UWorldMetricsSubsystem* Subsystem = World->GetSubsystem<UWorldMetricsSubsystem>();
	if (UNLIKELY(!Subsystem))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected null World Metrics Subsystem"), __FUNCTION__);
	}
	return Subsystem;
}
}  // namespace Private

UWorldMetricsSubsystem* GetSubsystem(const UWorld* World)
{
	return Private::GetSubsystem(World);
}

UWorldMetricInterface* GetMetric(const UWorld* World, const TSubclassOf<UWorldMetricInterface>& MetricClass)
{
	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return nullptr;
	}

	return Subsystem->GetMetric(MetricClass);
}

bool AddMetric(const UWorld* World, const TSubclassOf<UWorldMetricInterface>& MetricClass)
{
	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return false;
	}

	return Subsystem->AddMetric(MetricClass);
}

bool RemoveMetric(const UWorld* World, const TSubclassOf<UWorldMetricInterface>& MetricClass)
{
	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(World);
	if (UNLIKELY(!Subsystem))
	{
		return false;
	}

	return Subsystem->RemoveMetric(MetricClass);
}

UWorldMetricsExtension* AcquireExtension(
	UWorldMetricInterface* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected invalid owner"), __FUNCTION__);
		return nullptr;
	}

	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(InOwner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return nullptr;
	}

	return Subsystem->AcquireExtension(InOwner, InExtensionClass);
}

UWorldMetricsExtension* AcquireExtension(
	UWorldMetricsExtension* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected invalid owner"), __FUNCTION__);
		return nullptr;
	}

	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(InOwner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return nullptr;
	}

	return Subsystem->AcquireExtension(InOwner, InExtensionClass);
}

bool ReleaseExtension(UWorldMetricInterface* InOwner, const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected invalid owner"), __FUNCTION__);
		return false;
	}

	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(InOwner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return false;
	}

	return Subsystem->ReleaseExtension(InOwner, InExtensionClass);
}

bool ReleaseExtension(UWorldMetricsExtension* InOwner, const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected invalid owner"), __FUNCTION__);
		return false;
	}

	UWorldMetricsSubsystem* Subsystem = Private::GetSubsystem(InOwner->GetWorld());
	if (UNLIKELY(!Subsystem))
	{
		return false;
	}

	return Subsystem->ReleaseExtension(InOwner, InExtensionClass);
}

}  // namespace UE::WorldMetrics
