// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "PropertyBagTest.generated.h"

UCLASS()
class UTestPropertyBag1Int : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Text")
	int32 TheInt = -1;
};


UENUM(BlueprintType)
enum class ETestPropertyBagEnum : uint8
{
	ValueA,
	ValueB,
	ValueC,
};

UCLASS()
class UTestPropertyBagABCD : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Text")
	FString A;
	
	UPROPERTY(EditAnywhere, Category = "Text")
	ETestPropertyBagEnum B = ETestPropertyBagEnum::ValueA;
	
	UPROPERTY(EditAnywhere, Category = "Text")
	int32 C = -1;
	
	UPROPERTY(EditAnywhere, Category = "Text")
	float D = 100.f;
};

UCLASS()
class UTestPropertyBagABEF : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Text")
	FString A;

	UPROPERTY(EditAnywhere, Category = "Text")
	ETestPropertyBagEnum B = ETestPropertyBagEnum::ValueA;

	UPROPERTY(EditAnywhere, Category = "Text")
	int32 E = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	float F = 100.f;
};

UCLASS()
class UTestPropertyBagABCDABEF : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Text")
	FString A;

	UPROPERTY(EditAnywhere, Category = "Text")
	TObjectPtr<UTestPropertyBagABCD> ABCD;

	UPROPERTY(EditAnywhere, Category = "Text")
	int32 E = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	TObjectPtr<UTestPropertyBagABEF> ABEF;

	UPROPERTY(EditAnywhere, Category = "Text")
	float F = 100.f;
};

UCLASS()
class UTestPropertyBagArrayOfString : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Text")
	TArray<FString> Arr;
};

UCLASS()
class UTestPropertyBagAllSupportedTypes : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Text")
	TArray<TObjectPtr<UTestPropertyBagAllSupportedTypes>> Arr;

	UPROPERTY(EditAnywhere, Category = "Text")
	TMap<FString, int32> Map;

	UPROPERTY(EditAnywhere, Category = "Text")
	FString A;

	UPROPERTY(EditAnywhere, Category = "Text")
	int32 B = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	int8 B8 = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	int16 B16 = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	int64 B64 = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	float C = 100.f;

	UPROPERTY(EditAnywhere, Category = "Text")
	double D = 100.;

	UPROPERTY(EditAnywhere, Category = "Text")
	uint32 E = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	uint8 E8 = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	uint16 E16 = -1;

	UPROPERTY(EditAnywhere, Category = "Text")
	uint64 E64 = -1;
};
