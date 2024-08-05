// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Modules/ModuleManager.h"

#include "TypedElementDataStoragePerformanceTestCommands.generated.h"

/**
 * Column to represent that a row is selected
 */
USTRUCT(meta = (DisplayName = ""))
struct FTest_PingPongPrePhys final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 Value;
};

USTRUCT(meta = (DisplayName = ""))
struct FTest_PingPongDurPhys final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	uint64 Value;
};

USTRUCT(meta = (DisplayName = ""))
struct FTest_PingPongPostPhys final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint64 Value;
};

/**
 * The heads up transform display provides at a glance view in a scene outliner row of abnormal transform characteristics, including:
 *		1. Non-uniform scale
 *		2. Negative scaling on X, Y, or Z axis
 *		3. Unnormalized rotation
 */
UCLASS()
class UTest_PingPongBetweenPhaseFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTest_PingPongBetweenPhaseFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
};