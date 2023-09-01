// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "WorldPartitionSettings.generated.h"

UENUM()
enum class EDataLayerLogicOperator : uint8
{
	Or,
	And
};

UCLASS(config = Engine, defaultconfig, DisplayName = "World Partition")
class ENGINE_API UWorldPartitionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldPartitionSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UWorldPartitionSettings* Get() { return CastChecked<UWorldPartitionSettings>(UWorldPartitionSettings::StaticClass()->GetDefaultObject()); }

	EDataLayerLogicOperator GetDefaultDataLayerOperator() const { return DefaultDataLayerOperator; }

protected:
	UPROPERTY(EditAnywhere, config, Category = WorldPartition)
	EDataLayerLogicOperator DefaultDataLayerOperator = EDataLayerLogicOperator::Or;
};