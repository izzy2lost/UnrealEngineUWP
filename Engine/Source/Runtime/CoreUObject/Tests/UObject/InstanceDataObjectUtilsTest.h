// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/Object.h"
#include "InstanceDataObjectUtilsTest.generated.h"

UENUM()
enum ETestInstanceDataObjectBird : uint8
{
	TIDOB_None = 0,
	TIDOB_Cardinal,
	TIDOB_Crow,
	TIDOB_Eagle,
	TIDOB_Hawk,
	TIDOB_Owl,
	TIDOB_Raven,
};

UENUM()
namespace ETestInstanceDataObjectGrain
{
	enum Type : uint8
	{
		None = 0,
		Barley,
		Corn,
		Quinoa,
		Rice,
		Wheat,
	};
}

UENUM()
namespace ETestInstanceDataObjectGrainAlternate
{
	enum Type : uint8
	{
		None = 0,
		Corn,
		Rice,
		Rye,
		Wheat,
	};
}

UENUM()
enum class ETestInstanceDataObjectFruit : uint8
{
	None = 0,
	Apple,
	Banana,
	Orange,
};

UENUM()
enum class ETestInstanceDataObjectFruitAlternate : uint8
{
	None = 0,
	Apple,
	Cherry,
	Orange,
	Pear,
};

UENUM(Flags)
enum class ETestInstanceDataObjectDirection : uint16
{
	None = 0,
	North = 1 << 0,
	East = 1 << 1,
	South = 1 << 2,
	West = 1 << 3,
};

ENUM_CLASS_FLAGS(ETestInstanceDataObjectDirection);

UENUM(Flags)
enum class ETestInstanceDataObjectDirectionAlternate : uint16
{
	None = 0,
	Up = 1 << 0,
	Down = 1 << 1,
	North = 1 << 2,
	East = 1 << 3,
	South = 1 << 4,
	West = 1 << 5,
};

ENUM_CLASS_FLAGS(ETestInstanceDataObjectDirectionAlternate);

USTRUCT()
struct FTestInstanceDataObjectPoint
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 X = 0;

	UPROPERTY()
	int32 Y = 0;

	UPROPERTY()
	int32 Z = 0;

	UPROPERTY()
	int32 W = 0;
};

USTRUCT()
struct FTestInstanceDataObjectPointAlternate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 U = 0;

	UPROPERTY()
	int32 V = 0;

	UPROPERTY()
	int32 W = 0;
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

	UPROPERTY()
	TEnumAsByte<ETestInstanceDataObjectBird> Bird = TIDOB_None;

	UPROPERTY()
	TEnumAsByte<ETestInstanceDataObjectGrain::Type> Grain = ETestInstanceDataObjectGrain::None;

	UPROPERTY()
	ETestInstanceDataObjectFruit Fruit = ETestInstanceDataObjectFruit::None;

	UPROPERTY()
	ETestInstanceDataObjectDirection Direction = ETestInstanceDataObjectDirection::None;

	UPROPERTY()
	FTestInstanceDataObjectPoint Point;
};

USTRUCT()
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

	UPROPERTY()
	TEnumAsByte<ETestInstanceDataObjectBird> Bird = TIDOB_None;

	UPROPERTY(meta=(OriginalType="ETestInstanceDataObjectGrain(/Script/CoreUObject)"))
	TEnumAsByte<ETestInstanceDataObjectGrainAlternate::Type> Grain = ETestInstanceDataObjectGrainAlternate::None;

	UPROPERTY(meta=(OriginalType="ETestInstanceDataObjectFruit(/Script/CoreUObject)"))
	ETestInstanceDataObjectFruitAlternate Fruit = ETestInstanceDataObjectFruitAlternate::None;

	UPROPERTY(meta=(OriginalType="ETestInstanceDataObjectDirection(/Script/CoreUObject)"))
	ETestInstanceDataObjectDirectionAlternate Direction = ETestInstanceDataObjectDirectionAlternate::None;

	UPROPERTY(meta=(OriginalType="TestInstanceDataObjectPoint(/Script/CoreUObject)"))
	FTestInstanceDataObjectPointAlternate Point;
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
