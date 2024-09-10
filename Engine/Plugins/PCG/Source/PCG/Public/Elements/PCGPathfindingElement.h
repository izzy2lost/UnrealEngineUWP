// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "Elements/PCGTimeSlicedElementBase.h"
#include "SpatialAlgo/PCGAStar.h"

#include "PCGPathfindingElement.generated.h"

class UPCGPointData;

UENUM(BlueprintType, Blueprintable)
enum class EPCGPathfindingSplineMode : uint8
{
	Curve UMETA(Tooltip = "Interpret the spline as a continuous curve."),
	Linear UMETA(Tooltip = "Interpret the spline as a conjunction of linear segments."),
};

/** Finds the optimal path across the points of a given point cloud--should one exist--when provided a start and goal
 * location, and a maximum jump distance between points. Can return a partial path.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGPathfindingSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PathfindingElement")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPathfindingElement", "NodeTitle", "Pathfinding"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGPathfindingElement", "NodeTooltip", "Finds the optimal path across the points of a given point cloud--should one exist--when provided a start and goal location, and a maximum jump distance between points. Can return a partial path."); }
#endif // WITH_EDITOR

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	//~End UPCGSettings interface

public:
	/** The max distance from each point to search for the next viable point in the path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	double SearchDistance = 1000;

	/** The path's starting location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	FVector Start = FVector::ZeroVector;

	/** The location the pathfinding should attempt to reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	FVector Goal = FVector::ZeroVector;

	/** The heuristic estimates a faster path to speed up processing. A lower heuristic weight can be faster, but it may cease being the optimal path. A weight of 0 is essentially flood fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Algorithm == EPCGPathfindingAlgorithm::AStar", PCG_Overridable))
	double HeuristicWeight = 1.0;

	/** Even if the path is not complete, return the most optimal and viable partial path to the goal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	bool bAcceptPartialPath = true;

	/** The final path will be a spline. If false, the final path will be an ordered point data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bOutputAsSpline = true;

	/** Determines how the output spline's curves will be calculated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bOutputAsSpline", EditConditionHides, PCG_Overridable))
	EPCGPathfindingSplineMode SplineMode = EPCGPathfindingSplineMode::Curve;

	/** Copy the properties and attributes from the originating point input to the output points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bOutputAsSpline", EditConditionHides, PCG_Overridable))
	bool bCopyOriginatingPoints = false;
};

class FPCGPathfindingElement : public TPCGTimeSlicedElementBase<PCGSpatialAlgo::AStar::FSearchSettings, PCGSpatialAlgo::AStar::FSearchState>
{
protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
