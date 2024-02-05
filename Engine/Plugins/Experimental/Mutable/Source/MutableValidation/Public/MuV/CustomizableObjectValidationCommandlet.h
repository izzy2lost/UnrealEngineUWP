// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObjectInstance.h"

#include "CustomizableObjectValidationCommandlet.generated.h"

// Forward declarations
class UCustomizableObject;
class UCOIUpdater;

UCLASS()
class UCustomizableObjectValidationCommandlet : public UCommandlet 
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
	
private:
	/** Customizable Object to be tested */
	UPROPERTY()
	TObjectPtr<UCustomizableObject> ToTestCustomizableObject = nullptr;

	/** Array of COI to be generated with randomized parameter values */
	UPROPERTY()
	TArray<TObjectPtr<UCustomizableObjectInstance>> InstancesToProcess;
	
	UPROPERTY()
	TObjectPtr<UCOIUpdater> InstanceUpdater;
};