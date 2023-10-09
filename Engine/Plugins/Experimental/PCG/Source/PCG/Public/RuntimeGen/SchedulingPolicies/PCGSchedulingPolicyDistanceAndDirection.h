// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSchedulingPolicyBase.h"

#include "PCGSchedulingPolicyDistanceAndDirection.generated.h"

class IPCGGenSourceBase;

/**
 * SchedulingPolicyDistanceAndDirection uses distance from the generating volume 
 * and alignment with view direction to choose the most important volumes to generate.
 *
 * Distance and Direction are calculated with respect to the Generation Source.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSchedulingPolicyDistanceAndDirection : public UPCGSchedulingPolicyBase
{
	GENERATED_BODY()

public:
	virtual double CalculatePriority(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const override;

public:
	/** Toggle whether or not distance is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides))
	bool bUseDistance = true;

	/** Scalar value used to increase/decrease the impact of distance in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDistance", EditConditionHides))
	float DistanceWeight = 1.0f;

	/** Toggle whether or not direction is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides))
	bool bUseDirection = true;

	/** Scalar value used to increase/decrease the impact of direction in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDirection", EditConditionHides))
	float DirectionWeight = 0.0025f;
};
