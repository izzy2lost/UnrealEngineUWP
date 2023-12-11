// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NetworkMetricsConfig.generated.h"

USTRUCT()
struct FNetworkMetricConfig
{
	GENERATED_BODY()

public:
	/** The name of the metric to register the listener. */
	UPROPERTY()
	FName MetricName;

	/** A sub-class of UNetworkMetricBaseListener. */
	UPROPERTY()
	TSoftClassPtr<class UNetworkMetricsBaseListener> Class;
};

UCLASS(Config=Engine)
class UNetworkMetricsConfig : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(Config)
	TArray<FNetworkMetricConfig> Listeners;
};
