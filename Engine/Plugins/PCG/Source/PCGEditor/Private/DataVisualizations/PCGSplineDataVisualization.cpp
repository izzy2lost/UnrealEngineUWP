// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataVisualizations/PCGSplineDataVisualization.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGSplineStruct.h"

namespace PCGSplineDataVisualizationConstants
{
	const FVector HalfExtents = FVector(50.0); // Set the points to fill up 1 meter of space by default.
}

FPCGTableVisualizerInfo IPCGSplineDataVisualization::GetTableVisualizerInfo(const UPCGData* Data) const
{
	FPCGTableVisualizerInfo Info = IPCGSpatialDataVisualization::GetTableVisualizerInfo(Data);

	Info.DoubleClickCallback = [](const UPCGData* Data, int Index)
	{
		if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Data))
		{
			const TArray<FInterpCurvePointVector>& Positions = SplineData->SplineStruct.SplineCurves.Position.Points;
			const TArray<FInterpCurvePointVector>& Scales = SplineData->SplineStruct.SplineCurves.Scale.Points;

			check(Positions.IsValidIndex(Index) && Scales.IsValidIndex(Index));

			const FVector& Position = SplineData->GetTransform().TransformPosition(Positions[Index].OutVal);
			const FVector HalfExtent = Scales[Index].OutVal * PCGSplineDataVisualizationConstants::HalfExtents;

			FBox BoundingBox(Position + HalfExtent, Position - HalfExtent);
			GEditor->MoveViewportCamerasToBox(BoundingBox, /*bActiveViewportOnly=*/true, /*DrawDebugBoxTimeInSeconds=*/2.5f);
		}
	};

	return Info;
}

const UPCGPointData* IPCGSplineDataVisualization::CollapseToDebugPointData(FPCGContext* Context, const UPCGData* Data) const
{
	if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Data))
	{
		UPCGPointData* PointData = NewObject<UPCGPointData>();
		PointData->InitializeFromData(SplineData);
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

		const int32 NumControlPoints = SplineData->SplineStruct.SplineCurves.Position.Points.Num();

		for (int ControlPointIndex = 0; ControlPointIndex < NumControlPoints; ++ControlPointIndex)
		{
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = SplineData->SplineStruct.GetTransformAtSplineInputKey(ControlPointIndex, ESplineCoordinateSpace::World);
			Point.SetExtents(PCGSplineDataVisualizationConstants::HalfExtents);
		}

		return PointData;
	}

	return nullptr;
}
