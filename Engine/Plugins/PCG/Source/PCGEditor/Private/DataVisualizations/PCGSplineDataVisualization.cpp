// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataVisualizations/PCGSplineDataVisualization.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGSplineStruct.h"

const UPCGPointData* IPCGSplineDataVisualization::CollapseToDebugPointData(FPCGContext* Context, const UPCGData* Data) const
{
	if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Data))
	{
		UPCGPointData* PointData = NewObject<UPCGPointData>();
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

		const int32 NumControlPoints = SplineData->SplineStruct.SplineCurves.Position.Points.Num();

		for (int ControlPointIndex = 0; ControlPointIndex < NumControlPoints; ++ControlPointIndex)
		{
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = SplineData->SplineStruct.GetTransformAtSplineInputKey(ControlPointIndex, ESplineCoordinateSpace::World);
			Point.SetExtents(FVector::One() * 50.0f); // Set the points to fill up 1 meter of space by default.
		}

		return PointData;
	}

	return nullptr;
}
