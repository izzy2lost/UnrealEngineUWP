// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "AvalancheOutputChannelViewInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAvalancheOutputChannelViewInterface : public UInterface
{
	GENERATED_BODY()
};

class IAvalancheOutputChannelViewInterface
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintNativeEvent, Category = "Motion Design Output Channel")
	void SetChannelName(const FText& InChannelName);
};
