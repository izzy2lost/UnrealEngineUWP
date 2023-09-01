// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartitionSettings.generated.h"

UCLASS(config = Engine, defaultconfig, DisplayName = "World Partition")
class ENGINE_API UWorldPartitionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldPartitionSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UWorldPartitionSettings* Get() { return CastChecked<UWorldPartitionSettings>(UWorldPartitionSettings::StaticClass()->GetDefaultObject()); }

	EWorldPartitionDataLayersLogicOperator GetNewMapsDataLayersLogicOperator() const { return NewMapsDataLayersLogicOperator; }

protected:
	/** Set the default logical operator for actor data layers activation for new maps */
	UPROPERTY(EditAnywhere, config, Category = WorldPartition)
	EWorldPartitionDataLayersLogicOperator NewMapsDataLayersLogicOperator = EWorldPartitionDataLayersLogicOperator::Or;
};