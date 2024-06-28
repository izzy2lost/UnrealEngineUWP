// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"
#include "ConnectableValue.generated.h"

USTRUCT()
struct FChaosClothAssetConnectableStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value")
	FString StringValue;

	UE_DEPRECATED(5.5, "Override properties are no longer used.")
	UPROPERTY(Transient)
	mutable FString StringValue_Override;
};

USTRUCT()
struct FChaosClothAssetConnectableIStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (DataflowInput))
	FString StringValue;

	/**
	  * Whether the property could import fabrics datas or not
	  */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Value")
	bool bCouldUseFabrics = false;

	UE_DEPRECATED(5.5, "Override properties are no longer used.")
	UPROPERTY(Transient)
	mutable FString StringValue_Override;
	
	/**
	 * Whether the property can override the weight map based on the imported fabrics
	 */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (EditCondition = "bCouldUseFabrics", EditConditionHides))
	bool bBuildFabricMaps = false;
};

USTRUCT()
struct FChaosClothAssetConnectableIOStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "StringValue"))
	FString StringValue;

	UE_DEPRECATED(5.5, "Override properties are no longer used.")
	UPROPERTY(Transient)
	mutable FString StringValue_Override;
};
