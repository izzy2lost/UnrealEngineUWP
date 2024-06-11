// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowTypePolicy.h"

#include "DataflowAnyType.generated.h"

USTRUCT()
struct FDataflowAnyType
{
	using FPolicyType = FDataflowAllTypesPolicy;
	using FStorageType = void;

	GENERATED_USTRUCT_BODY()
	DATAFLOWCORE_API static const FName TypeName;
};

USTRUCT()
struct FDataflowAllTypes : public FDataflowAnyType
{
	using FPolicyType = FDataflowAllTypesPolicy;
	using FStorageType = void;
	GENERATED_USTRUCT_BODY()
};

USTRUCT()
struct FDataflowNumericTypes : public FDataflowAnyType
{
	using FPolicyType = FDataflowNumericTypePolicy;
	using FStorageType = double;

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Value)
	double Value = 0.0;
};

USTRUCT()
struct FDataflowStringTypes : public FDataflowAnyType
{
	using FPolicyType = FDataflowStringTypePolicy;
	using FStorageType = FString;

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Value)
	FString Value;
};

USTRUCT()
struct FDataflowStringConvertibleTypes : public FDataflowAnyType
{
	using FPolicyType = FDataflowStringConvertibleTypePolicy;
	using FStorageType = FString;

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Value)
	FString Value;
};


USTRUCT()
struct FDataflowUObjectConvertibleTypes : public FDataflowAnyType
{
	using FPolicyType = FDataflowUObjectConvertibleTypePolicy;
	using FStorageType = TObjectPtr<UObject>;

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Value)
	TObjectPtr<UObject> Value;
};