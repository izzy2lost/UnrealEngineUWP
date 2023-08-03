// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserInheritableClassOption.h"
#include "UObject/SoftObjectPath.h"
#include "MultiUserDefaultSubobjectSelection.generated.h"

UENUM()
enum class EMultiUserIncludeAllSubobjectsType : uint8
{
	/** Include nothing by default */
	None,
	/** Include all components */
	AllComponents,
	/** Include all subobjects recursively (includes components, their subobjects, etc. ) */
	AllSubobjects
};

/** Settings for components that are supposed to be included  */
USTRUCT()
struct FMultiUserDefaultSubobjectSelection : public FMultiUserInheritableClassOption
{
	GENERATED_BODY()
	
	/**
	 * A list of subobject classes to add by default.
	 * For example, when you add an actor, you want to also add certain component classes.
	 */
	UPROPERTY(EditAnywhere, Category = "Config")
	TSet<FSoftClassPath> IncludeClasses;

	/**
	 * Components that match any of this regex will be excluded from DefaultSelectedComponentClasses.
	 *
	 * Tips:
	 * - If you want to include "Component" but not "ComponentName" you can use boundaries "\bComponent\b".
	 * . ".*" matches any sequence of characters.
	 */
	UPROPERTY(EditAnywhere, Category = "Config")
	TSet<FString> ExcludeSubobjectRegex;

	/**
	 * Components that match any of this regex will be included as well.
	 * 
	 * Tips:
	 * - If you want to include "Component" but not "ComponentName" you can use boundaries "\bComponent\b".
	 * . ".*" matches any sequence of characters. 
	 */
	UPROPERTY(EditAnywhere, Category = "Config")
	TSet<FString> IncludeSubobjectRegex;

	/** Behaviour to configure for including all of a certain type of subobject. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Config")
	EMultiUserIncludeAllSubobjectsType IncludeAllOption = EMultiUserIncludeAllSubobjectsType::None;
};
