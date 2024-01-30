// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAvalancheInteractiveToolsModeDetailsObjectProvider.generated.h"

class UObject;

UINTERFACE(MinimalAPI)
class UAvalancheInteractiveToolsModeDetailsObjectProvider : public UInterface
{
	GENERATED_BODY()
};

class AVALANCHEINTERACTIVETOOLSRUNTIME_API IAvalancheInteractiveToolsModeDetailsObjectProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, CallInEditor, Category = "Motion Design Interactive Tools")
	UObject* GetModeDetailsObject() const;

	virtual UObject* GetModeDetailsObject_Implementation() const PURE_VIRTUAL(IAvalancheInteractiveToolsModeDetailsObjectProvider::GetModeDetailsObject, return nullptr;)
};
