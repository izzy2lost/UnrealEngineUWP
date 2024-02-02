// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kinematic/Modes/WalkingMode.h"
#include "Curves/CurveFloat.h"
#include "AdvancedWalkingMode.generated.h"

/**
 * AdvancedWalkingMode: advanced example mode that expands tunability of movement input, speed, turning, etc.
 */
UCLASS(Blueprintable, BlueprintType)
class UAdvancedWalkingMode : public UWalkingMode
{
	GENERATED_UCLASS_BODY()

public:
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	// Maps ratio of Current Speed / Max Speed (range [0, 1]) to a turning rate (degrees per second)
	UPROPERTY(EditAnywhere, Category = "Movement Tuning")
	FRuntimeFloatCurve MaxTurningRateBySpeedPct;
};
