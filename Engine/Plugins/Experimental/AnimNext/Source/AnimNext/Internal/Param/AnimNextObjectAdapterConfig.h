// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AnimNextObjectAdapterConfig.generated.h"

UENUM()
enum class EAnimNextObjectAdapterFilterType : int32
{
	// Allow-list of functions and properties
	AllowList,

	// Deny-list of functions and properties
	DenyList,

	// Expose all functions but no properties
	AllowOnlyFunctions,

	// Expose all properties but no functions
	AllowOnlyProperties,

	// Expose all functions and properties
	AllowAllPropertiesAndFunctions,
};

USTRUCT()
struct FAnimNextObjectAdapterConfig
{
	GENERATED_BODY()

	/** The class to expose to AnimNext */
	UPROPERTY(EditAnywhere, Config, Category="Adapter")
	TSubclassOf<UObject> Class;

	/**
	 * The root parameter for the object when exposed to AnimNext
	 * If Root Parameter is set to 'MyObject', then all properties and functions derived from this class will have the
	 * form 'MyObject.Param'.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Adapter", meta = (CustomWidget = "ParamName"))
	FName RootParameter;

	/** How to expose the properties and functions of the class */
	UPROPERTY(EditAnywhere, Config, Category="Adapter")
	EAnimNextObjectAdapterFilterType FilterType = EAnimNextObjectAdapterFilterType::AllowList;

	/** All functions and properties that are allowed/denied */
	UPROPERTY(EditAnywhere, Config, Category="Adapter", meta = (EditCondition = "FilterType == EAnimNextObjectAdapterFilterType::AllowList || FilterType == EAnimNextObjectAdapterFilterType::DenyList", EditConditionHides))
	TArray<FName> FilteredFields;

	/** Whether to expose a tick function (e.g. for a component or actor) along with functions and properties */
	UPROPERTY(EditAnywhere, Config, Category="Adapter")
	bool bRegisterTickFunction = false;
};
