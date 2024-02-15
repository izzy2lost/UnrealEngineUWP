// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"

#include "WorldMetricsSubsystem.generated.h"

class UWorldMetricInterface;
class UWorldMetricsExtension;
class UWorldMetricsSubsystem;

/**
 * World metrics subsystem
 *
 * This subsystem provides an interface to add and remove world metrics implementing the UWorldMetricInterface class.
 * Both the subsystem and individual metrics can be enabled on demand. The World Metrics subsystem is the owner of all
 * the added metrics and these get automatically garbage collected after removal unless another system holds a
 * hard-reference to them.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig)
class UWorldMetricsSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	[[nodiscard]] WORLDMETRICSCORE_API static UWorldMetricsSubsystem* Get(const UWorld* World);

	//~ Begin USubsystem
	WORLDMETRICSCORE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	WORLDMETRICSCORE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	WORLDMETRICSCORE_API virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin UObject
	WORLDMETRICSCORE_API virtual void BeginDestroy() override;
	//~ End UObject

	/**
	 * Enables or disables the subsystem. When enabled the subsystem uses an update ticker to update each of the added
	 * world metrics. All metrics are automatically initialized when the system is enabled and deinitialized when is
	 * disabled. The subsystem is automatically disabled on deinitialization.
	 *
	 * @param bEnable: true for enabling the system and false otherwise.
	 */
	WORLDMETRICSCORE_API void Enable(bool bEnable);

	[[nodiscard]] bool IsEnabled() const
	{
		return UpdateTickerHandle.IsValid();
	}

	WORLDMETRICSCORE_API void Clear();

	/**
	 * Sets the subsystem update ticker rate in seconds. This method automatically restarts the subsystem if it's
	 * already enabled. Changing the update rate value on an enabled subsystem causes all metrics to be reinitialized.
	 *
	 * @requirement: parameter InSeconds value must be equal or greater than zero.
	 * @param InSeconds: the update rate value in seconds.
	 */
	WORLDMETRICSCORE_API void SetUpdateRateInSeconds(float InSeconds);

	/**
	 * @return the number of metrics currently contained by the subsystem.
	 */
	WORLDMETRICSCORE_API int32 NumMetrics() const;

	/**
	 * @return true if the subsystem contains any metric.
	 */
	WORLDMETRICSCORE_API bool HasAnyMetric() const;

	/**
	 * @return the number of extensions currently contained by the subsystem.
	 */
	WORLDMETRICSCORE_API int32 NumExtensions() const;

	/**
	 * @return true if the subsystem contains any extension.
	 */
	WORLDMETRICSCORE_API bool HasAnyExtension() const;

	/**
	 * Gets a world metric instance from the subsystem using the parameter metric
	 * class.
	 *
	 * @param InMetricClass: the class of the world metric to be retrieved.
	 * @return A valid pointer to the world metric if found, and nullptr otherwise.
	 */
	[[nodiscard]] WORLDMETRICSCORE_API UWorldMetricInterface* GetMetric(
		const TSubclassOf<UWorldMetricInterface>& InMetricClass) const;

	template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
	[[nodiscard]] MetricClass* GetMetric() const
	{
		return static_cast<MetricClass*>(GetMetric(MetricClass::StaticClass()));
	}

	/**
	 * Adds a metric instance of the parameter class to the subsystem unless a metric of the same class and/or same ID
	 * already exists. The system becomes the owner of all added metrics.
	 *
	 * @param InMetricClass: the class of the metric to add.
	 * @param InMetricId: the ID of the metric to add.
	 * @return true if a metric of the parameter metric class is added and false otherwise.
	 */
	WORLDMETRICSCORE_API bool AddMetric(const TSubclassOf<UWorldMetricInterface>& InMetricClass);

	template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
	bool AddMetric()
	{
		return AddMetric(MetricClass::StaticClass());
	}

	/**
	 * Removes the metric matching the parameter class and/or parameter ID from the subsystem.
	 * The metric object gets automatically garbage-collected unless other system holds a hard-reference to it.
	 *
	 * @param InMetricClass: the class corresponding to the world metric to remove.
	 * @param InMetricId: the ID corresponding to the world metric to remove.
	 * @return true if a metric matching the parameter class is removed and false otherwise.
	 */
	WORLDMETRICSCORE_API bool RemoveMetric(const TSubclassOf<UWorldMetricInterface>& InMetricClass);

	template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
	bool RemoveMetric()
	{
		return RemoveMetric(MetricClass::StaticClass());
	}

	WORLDMETRICSCORE_API void RemoveAllMetrics();

	/**
	 * Invokes the parameter function on each of the metrics contained by the subsystem.
	 * @param Func The function which will be invoked for each metric. The function should return true to continue
	 * execution, or false otherwise.
	 */
	WORLDMETRICSCORE_API void ForEachMetric(const TFunctionRef<bool(const UWorldMetricInterface*)>& Func) const;
	WORLDMETRICSCORE_API void ForEachMetric(const TFunctionRef<bool(UWorldMetricInterface*)>& Func);

	/**
	 * Invokes the parameter function on each metric of the template argument class contained by the subsystem.
	 * @param Func The function which will be invoked for each metric. The function should return true to continue
	 * execution, or false otherwise.
	 */
	template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
	void ForEachMetricOfClass(const TFunctionRef<bool(const MetricClass*)>& Func) const
	{
		for (const UWorldMetricInterface* Metric : Metrics)
		{
			if (const MetricClass* TypedMetric = Cast<MetricClass>(Metric))
			{
				if (!Func(TypedMetric))
				{
					break;
				}
			}
		}
	}

	template <typename MetricClass UE_REQUIRES(std::is_base_of_v<UWorldMetricInterface, MetricClass>)>
	void ForEachMetricOfClass(const TFunctionRef<bool(MetricClass*)>& Func)
	{
		for (UWorldMetricInterface* Metric : Metrics)
		{
			if (MetricClass* TypedMetric = Cast<MetricClass>(Metric))
			{
				if (!Func(TypedMetric))
				{
					break;
				}
			}
		}
	}

	/**
	 * Acquires an extension on behalf of a object. The extension will be created the first time it is acquired,
	 * and conversely it will be disabled once released by all owners.
	 *
	 * @param InOwner: The object acquiring the extension.
	 * @param InExtensionClass: The class of the desired extension.
	 * @return A pointer to the extension. The lifetime of the extension is managed by the subsystem.
	 */
	WORLDMETRICSCORE_API UWorldMetricsExtension* AcquireExtension(
		UWorldMetricInterface* InOwner,
		const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	template <class ExtensionClass UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
	ExtensionClass* AcquireExtension(UWorldMetricInterface* InMetricOwner)
	{
		return static_cast<ExtensionClass*>(AcquireExtension(InMetricOwner, ExtensionClass::StaticClass()));
	}

	WORLDMETRICSCORE_API UWorldMetricsExtension* AcquireExtension(
		UWorldMetricsExtension* InOwner,
		const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	template <class ExtensionClass UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
	ExtensionClass* AcquireExtension(UWorldMetricsExtension* InExtensionOwner)
	{
		return static_cast<ExtensionClass*>(AcquireExtension(InExtensionOwner, ExtensionClass::StaticClass()));
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

	template <class ExtensionClass UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
	bool ReleaseExtension(UWorldMetricInterface* InOwner)
	{
		return ReleaseExtension(InOwner, ExtensionClass::StaticClass());
	}

	template <class ExtensionClass UE_REQUIRES(std::is_base_of_v<UWorldMetricsExtension, ExtensionClass>)>
	bool ReleaseExtension(UWorldMetricsExtension* InExtensionOwner)
	{
		return ReleaseExtension(InExtensionOwner, ExtensionClass::StaticClass());
	}

private:
	static constexpr int32 DefaultExtensionCapacity = 16;
	static constexpr int32 DefaultOwnerListCapacity = 32;

	FTSTicker::FDelegateHandle UpdateTickerHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWorldMetricInterface>> Metrics;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWorldMetricsExtension>> Extensions;

	using FOwnerList = TSet<UObject*, DefaultKeyFuncs<UObject*>, TInlineSetAllocator<DefaultOwnerListCapacity>>;
	TArray<FOwnerList, TInlineAllocator<DefaultExtensionCapacity>> IndexedOwners;

public:
	UPROPERTY(Config)
	float UpdateRateInSeconds = 0.f;

	/** The number of frames the subsystem waits to update added metrics after their initialization. */
	UPROPERTY(Config)
	int32 WarmUpFrames = 8;

private:
	int32 PendingWarmUpFrames = 0;

	void InitializeMetrics();
	void DeinitializeMetrics();

	void OnUpdate(float DeltaTimeInSeconds);

	/**
	 * Returns the live world metric list index corresponding to the parameter metric class.
	 *
	 * @param InMetricClass: the class type of the world metric.
	 * @return a valid index value or INDEX_NONE if not matching metric exists.
	 */
	int32 GetMetricIndex(const TSubclassOf<UWorldMetricInterface>& InMetricClass) const;

	/**
	 * Returns the live extension list index corresponding to the parameter extension class.
	 *
	 * @param InExtensionClass: the class type of the extension.
	 * @return a valid index value or INDEX_NONE if not matching extension exists.
	 */
	int32 GetExtensionIndex(const TSubclassOf<UWorldMetricsExtension>& InExtensionClass) const;

	UWorldMetricsExtension* AcquireExtensionInternal(
		UObject* InOwner,
		const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	UWorldMetricsExtension* AcquireExistingExtension(
		UObject* InOwner,
		const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	UWorldMetricsExtension* AddExtension(UObject* InOwner, const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	bool ReleaseExtensionInternal(UObject* InOwner, const TSubclassOf<UWorldMetricsExtension>& InExtensionClass);

	bool TryRemoveExtensionAt(int32 ExtensionIndex);

	/**
	 * Verifies the parameter metric has released any acquired extensions, releasing any pending ones. It's the metric's
	 * responsibility to release all previously acquired resources. This method logs all extensions owned by the
	 * parameter metric that hasn't been released.
	 *
	 * @param InMetric: the metric to verify.
	 */
	void VerifyMetricReleasedAllExtensions(UWorldMetricInterface* InMetric);

	/**
	 * Verifies there are no orphan extensions. These are extensions that are still alive without any live metric. This
	 * method logs and removes all orphan extensions, addressing the scenario where extensions that acquired other
	 * extensions didn't release them. Extensions, like metrics, are responsible for releasing acquired extensions.
	 */
	void VerifyRemoveOrphanExtensions();
};
