// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "InstanceDataObjectUtilsTest.generated.h"

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
