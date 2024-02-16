// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NavModifierComponent.h"

#include "SplineNavModifierComponent.generated.h"

struct FNavigationRelevantData;

UENUM()
enum class ESubdivisionLOD
{
	Low,
	Medium,
	High,
	Ultra,
};

/**
 *	Assign a chosen NavArea in the vicinity of a spline
 *	This component must only be attached to an actor with a USplineComponent
 */
UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent), hidecategories = (Variable, Tags, Cooking, Collision), MInimalAPI)
class USplineNavModifierComponent : public UNavModifierComponent
{
	GENERATED_BODY()

	/** Width of the generated tube-shaped NavModifierVolume enclosing the spline */
	UPROPERTY(EditAnywhere, Category = Navigation, Meta=(UIMin="10", ClampMin="10"))
	double StrokeWidth = 500.0f;
	
	/** Height of the generated tube-shaped nav modifier volume enclosing the spline, must be tall enough to pass through the desired surface beneath the spline */
	UPROPERTY(EditAnywhere, Category = Navigation, Meta=(UIMin="10", ClampMin="10"))
	double StrokeHeight = 500.0f;
		
	/** Higher LOD will capture finer details in the spline */
	UPROPERTY(EditAnywhere, Category = Navigation)
	ESubdivisionLOD SubdivisionLOD = ESubdivisionLOD::Medium;
	
	virtual void CalculateBounds() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;

protected:
	float GetSudivisionThreshold() const;
};
