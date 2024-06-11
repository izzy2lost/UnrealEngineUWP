// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "InstanceDataObjectUtilsTest.generated.h"

UENUM()
enum class ETestInstanceDataObjectFruit : uint8
{
	None = 0,
	Apple,
	Banana,
	Orange,
};

UENUM(Flags)
enum class ETestInstanceDataObjectFlags : uint16
{
	None = 0,
	North = 1 << 0,
	East = 1 << 1,
	South = 1 << 2,
	West = 1 << 3,
};

USTRUCT()
struct FTestInstanceDataObjectStruct
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 A = -1;

	UPROPERTY()
	int32 B = -1;

	UPROPERTY()
	int32 C = -1;

	UPROPERTY()
	int32 D = -1;
};

USTRUCT(meta=(OriginalType="TestInstanceDataObjectStruct"))
struct FTestInstanceDataObjectStructAlternate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float B = -1;

	UPROPERTY()
	int64 C = -1;

	UPROPERTY()
	int32 D = -1;

	UPROPERTY()
	int32 E = -1;
};

UCLASS()
class UTestInstanceDataObjectClass : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Int32 = -1;

	UPROPERTY()
	FTestInstanceDataObjectStruct Struct;
};
