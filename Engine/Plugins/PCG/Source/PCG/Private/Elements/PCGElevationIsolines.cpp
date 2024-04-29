// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGElevationIsolines.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGSurfaceData.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Helpers/PCGAsync.h"
#include "SpatialAlgo/PCGMarchingSquares.h"

#define LOCTEXT_NAMESPACE "PCGElevationIsolinesElement"

#if WITH_EDITOR
FName UPCGElevationIsolinesSettings::GetDefaultNodeName() const
{
	return FName(TEXT("ElevationIsolines"));
}

FText UPCGElevationIsolinesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Elevation Isolines");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGElevationIsolinesSettings::CreateElement() const
{
	return MakeShared<FPCGElevationIsolinesElement>();
}

TArray<FPCGPinProperties> UPCGElevationIsolinesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	FPCGPinProperties& InputPin = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Surface);
	InputPin.SetRequiredPin();

	Properties.Emplace_GetRef(TEXT("BoundingShape"), EPCGDataType::Spatial);

	return Properties;
}

TArray<FPCGPinProperties> UPCGElevationIsolinesSettings::OutputPinProperties() const
{ 
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, bOutputAsSpline ? EPCGDataType::Spline : EPCGDataType::Point);
	return Properties;
}

bool FPCGElevationIsolinesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGElevationIsolinesElement::Execute);

	check(InContext);

	const UPCGElevationIsolinesSettings* Settings = InContext->GetInputSettings<UPCGElevationIsolinesSettings>();
	check(Settings);

	if (FMath::IsNearlyZero(Settings->ElevationIncrement) || Settings->ElevationEnd < Settings->ElevationStart)
	{
		return true;
	}

	bool bOutUnionDataCreated = false;
	const UPCGSpatialData* BoundingShape = InContext->InputData.GetSpatialUnionOfInputsByPin(TEXT("BoundingShape"), bOutUnionDataCreated);

	// Fallback to getting bounds from actor
	if (!BoundingShape && InContext->SourceComponent.IsValid())
	{
		check(!bOutUnionDataCreated);
		BoundingShape = Cast<UPCGSpatialData>(InContext->SourceComponent->GetActorPCGData());
	}

	if (!BoundingShape)
	{
		return true;
	}

	for (const FPCGTaggedData& InputData : InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGSurfaceData* SurfaceData = Cast<UPCGSurfaceData>(InputData.Data);
		if (!SurfaceData)
		{
			continue;
		}

		const FBox Bounds = BoundingShape->GetBounds();
		const FVector CellSize{ Settings->Resolution, Settings->Resolution, 1.0 };
		const FVector HalfCellSize = CellSize / 2.0;
		const FBox CellBounds{ -HalfCellSize, HalfCellSize };
		const FVector Origin{ Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z };
		const int32 CellMinX = FMath::CeilToInt((Bounds.Min.X) / CellSize.X);
		const int32 CellMaxX = FMath::FloorToInt((Bounds.Max.X) / CellSize.X);
		const int32 CellMinY = FMath::CeilToInt((Bounds.Min.Y) / CellSize.Y);
		const int32 CellMaxY = FMath::FloorToInt((Bounds.Max.Y) / CellSize.Y);
		const int32 CellXCount = CellMaxX - CellMinX;
		const int32 CellYCount = CellMaxY - CellMinY;
		const int32 CellCount = CellXCount * CellYCount;

		TArray<double> Heightmap;
		Heightmap.SetNumUninitialized(CellCount);

		int32 NumPointsWritten = 0;

		FPCGProjectionParams ProjectionParams{};
		ProjectionParams.bProjectPositions = true;

		// TODO: Time-slice
		FPCGAsync::AsyncProcessingOneToOneEx(&InContext->AsyncState, Heightmap.Num(), []() {}, [CellYCount, Settings, Origin, CellSize, SurfaceData, &Heightmap, &ProjectionParams](int32 ReadIndex, int32 WriteIndex)
		{
			const int32 CellX = ReadIndex / CellYCount;
			const int32 CellY = ReadIndex % CellYCount;

			FVector Cell{ Origin.X + CellSize.X * CellX, Origin.Y + CellSize.Y * CellY, Origin.Z };

			FPCGPoint Projected;
			bool bSuccess = SurfaceData->ProjectPoint(FTransform(Cell), FBox(EForceInit::ForceInit), ProjectionParams, Projected, nullptr);
			Heightmap[WriteIndex] = bSuccess ? Projected.Transform.GetLocation().Z : std::numeric_limits<double>::min();

		}, /*bEnableTimeSlicing=*/false);

		auto GetElevation = [CellYCount, &Heightmap](int32 X, int32 Y) -> double
		{
			return Heightmap[X * CellYCount + Y];
		};

		// TODO: Async + Support non-Z up cases.
		// Run the Marching Squares algorithm on each isoline we want.
		for (double Elevation = Settings->ElevationStart; Elevation < Settings->ElevationEnd; Elevation += Settings->ElevationIncrement)
		{
			for (const PCGSpatialAlgo::FPCGMarchingSquareResult& Result : PCGSpatialAlgo::MarchingSquares(CellXCount, CellYCount, Elevation, GetElevation, /*bUseLinearInterpolation=*/true))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGElevationIsolinesElement::Execute::CreateOutputData);

				auto TransformFromGridCoordinate = [&Origin, &CellSize, Elevation](const FVector2D& GridCoordinate)
				{
					return FTransform(FVector(Origin.X + CellSize.X * GridCoordinate.X, Origin.Y + CellSize.Y * GridCoordinate.Y, Elevation));
				};

				FPCGTaggedData& OutputData = InContext->OutputData.TaggedData.Emplace_GetRef(InputData);
				OutputData.Pin = PCGPinConstants::DefaultOutputLabel;

				if (Settings->bOutputAsSpline)
				{
					TArray<FSplinePoint> SplinePoints;
					SplinePoints.Reserve(Result.LinkedGridCoordinates.Num());

					for (int32 Index = 0; Index < Result.LinkedGridCoordinates.Num(); ++Index)
					{
						const FTransform Transform = TransformFromGridCoordinate(Result.LinkedGridCoordinates[Index]);

						SplinePoints.Emplace(static_cast<float>(Index),
							Transform.GetLocation(),
							FVector::ZeroVector,
							FVector::ZeroVector,
							Transform.GetRotation().Rotator(),
							Transform.GetScale3D(),
							ESplinePointType::Curve);
					}

					UPCGSplineData* OutSplineData = NewObject<UPCGSplineData>();
					OutSplineData->Initialize(SplinePoints, Result.bClosed, FTransform::Identity);
					OutputData.Data = OutSplineData;
				}
				else
				{
					UPCGPointData* OutPointData = NewObject<UPCGPointData>();
					TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
					OutPoints.Reserve(Result.LinkedGridCoordinates.Num());
					OutputData.Data = OutPointData;

					for (const FVector2D& GridCoordinate : Result.LinkedGridCoordinates)
					{
						FPCGPoint& Point = OutPoints.Emplace_GetRef();
						Point.Transform = TransformFromGridCoordinate(GridCoordinate);
						Point.SetLocalBounds(CellBounds);
					}
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
