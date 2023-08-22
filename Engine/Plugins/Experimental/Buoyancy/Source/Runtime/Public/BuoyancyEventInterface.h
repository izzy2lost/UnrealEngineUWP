// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Interface.h"
#include "BuoyancyEventInterface.generated.h"

UINTERFACE(Blueprintable)
class BUOYANCY_API UBuoyancyEventInterface : public UInterface
{
	GENERATED_BODY()
};

class BUOYANCY_API IBuoyancyEventInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent, Category = WaterBody)
	void  OnSurfaceTouched(
		class AWaterBody* WaterBodyActor,
		UPrimitiveComponent* WaterComponent,
		UPrimitiveComponent* SubmergedComponent,
		float SubmergedVolume,
		const FVector& SubmergedCenterOfMass,
		const FVector& SubmergedVelocity);

};
