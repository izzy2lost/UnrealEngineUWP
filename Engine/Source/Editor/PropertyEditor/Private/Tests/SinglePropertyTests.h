// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

//#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Actor.h"
#include "Editor/PropertyEditorTestObject.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "PropertyEditorModule.h"
#include "UObject/Object.h"

#include "SinglePropertyTests.generated.h"

UCLASS()
class UPropertyEditorSinglePropertyTestClass : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Properties")
	FVector Vector;
};

//#endif