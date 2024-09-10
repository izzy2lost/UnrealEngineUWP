// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPathfindingElement.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Helpers/PCGHelpers.h"
#include "SpatialAlgo/PCGAStar.h"

#include "Components/SplineComponent.h"

#define LOCTEXT_NAMESPACE "PCGPathfindingElement"

namespace PCGPathfindingElement::Helpers
{
	TArray<FSplinePoint> ConvertPathToSplinePoints(const TArrayView<const FPCGPoint> Path, const EPCGPathfindingSplineMode SplineMode)
	{
		ESplinePointType::Type SplineCurveMode = ESplinePointType::Type::Constant;
		switch (SplineMode)
		{
			case EPCGPathfindingSplineMode::Curve:
				SplineCurveMode = ESplinePointType::Curve;
				break;
			case EPCGPathfindingSplineMode::Linear:
				SplineCurveMode = ESplinePointType::Linear;
			default:
				break;
		}

		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(Path.Num());

		int Index = 0;
		Algo::Transform(Path, SplinePoints, [&Index, SplineCurveMode](const FPCGPoint& Point)
		{
			return FSplinePoint(
					Index++, // Spline points must be indexed in ascending order
					Point.Transform.GetLocation(),
					FVector::ZeroVector,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					FVector::OneVector,
					SplineCurveMode);
		});

		return SplinePoints;
	}
} // namespace PCGPathfindingElement::Helpers

TArray<FPCGPinProperties> UPCGPathfindingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	// TODO: PointOrParam input for start/goal.
	return Properties;
}

TArray<FPCGPinProperties> UPCGPathfindingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	if (bOutputAsSpline)
	{
		Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);
	}
	else
	{
		Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	}

	return Properties;
}

FPCGElementPtr UPCGPathfindingSettings::CreateElement() const
{
	return MakeShared<FPCGPathfindingElement>();
}

#if WITH_EDITOR
EPCGChangeType UPCGPathfindingSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType LocalChangeType = EPCGChangeType::Cosmetic;
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGPathfindingSettings, bOutputAsSpline))
	{
		LocalChangeType |= EPCGChangeType::Structural;
	}

	return Super::GetChangeTypeForProperty(InPropertyName) | LocalChangeType;
}
#endif // WITH_EDITOR

bool FPCGPathfindingElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPathfindingElement::PrepareData);

	ContextType* Context = static_cast<ContextType*>(InContext);
	check(Context);

	const UPCGPathfindingSettings* Settings = Context->GetInputSettings<UPCGPathfindingSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> PointInputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	if (PointInputs.IsEmpty())
	{
		return true;
	}

	Context->InitializePerExecutionState([&PointInputs, Settings](ContextType*, ExecStateType& OutState)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGClusterElement::PrepareData::InitializePerExecutionState);

		if (PointInputs.IsEmpty())
		{
			return EPCGTimeSliceInitResult::NoOperation;
		}

		// TODO: The start and goal points will eventually come from PointOrParam.
		OutState = PCGSpatialAlgo::AStar::FSearchSettings
		{
			.StartPoint = FPCGPoint(FTransform(Settings->Start), /*InDensity=*/1, PCGHelpers::ComputeSeedFromPosition(Settings->Start)),
			.SearchDistance = Settings->SearchDistance,
			.Start = Settings->Start,
			.Goal = Settings->Goal,
			.HeuristicWeight = Settings->HeuristicWeight,
			.bAcceptPartialPath = Settings->bAcceptPartialPath,
			.bCopyOriginatingPoints = Settings->bCopyOriginatingPoints
		};

		return EPCGTimeSliceInitResult::Success;
	});

	Context->InitializePerIterationStates(PointInputs.Num(), [&PointInputs](IterStateType& OutSearchState, const ExecStateType& SearchSettings, const uint32 IterationIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGClusterElement::PrepareData::InitializePerIterationStates);

		OutSearchState.OriginatingPointData = Cast<UPCGPointData>(PointInputs[IterationIndex].Data);
		if (OutSearchState.OriginatingPointData->IsEmpty())
		{
			// Already confirmed we can't make it from start->goal in the execution state check.
			return EPCGTimeSliceInitResult::NoOperation;
		}

		PCGSpatialAlgo::AStar::Initialize(OutSearchState.OriginatingPointData, SearchSettings, OutSearchState);

		return EPCGTimeSliceInitResult::Success;
	});

	return true;
}

bool FPCGPathfindingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPathfindingElement::Execute);

	ContextType* TimeSlicedContext = static_cast<ContextType*>(InContext);
	check(TimeSlicedContext);

	if (!TimeSlicedContext->DataIsPreparedForExecution() || TimeSlicedContext->GetExecutionStateResult() == EPCGTimeSliceInitResult::NoOperation)
	{
		return true;
	}

	const UPCGPathfindingSettings* Settings = TimeSlicedContext->GetInputSettings<UPCGPathfindingSettings>();
	check(Settings);

	return ExecuteSlice(TimeSlicedContext, [Settings](ContextType* Context, const ExecStateType& SearchSettings, IterStateType& PathData, const uint32 IterIndex)
	{
		if (Context->GetIterationStateResult(IterIndex) == EPCGTimeSliceInitResult::NoOperation)
		{
			return true;
		}

		TArray<FPCGPoint> FinalPath;
		bool bFinished = false;
		do
		{
			bFinished = PCGSpatialAlgo::AStar::ExecuteSearchIteration(SearchSettings, PathData, FinalPath);

			if (!bFinished && Context->ShouldStop())
			{
				return false;
			}
		}
		while (!bFinished);

		// No path was found and partial paths were not enabled.
		if (FinalPath.IsEmpty())
		{
			check(!SearchSettings.bAcceptPartialPath);
			return true;
		}

		// Finally, output the path as either a spline or points
		if (Settings->bOutputAsSpline)
		{
			const TArray<FSplinePoint> SplinePoints = PCGPathfindingElement::Helpers::ConvertPathToSplinePoints(FinalPath, Settings->SplineMode);
			UPCGSplineData* SplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(Context);
			SplineData->Initialize(SplinePoints, /*bInClosedLoop=*/false, FTransform::Identity);

			FPCGTaggedData& OutputData = Context->OutputData.TaggedData.Emplace_GetRef();
			OutputData.Data = SplineData;
		}
		else
		{
			UPCGPointData* OutputPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
			if (SearchSettings.bCopyOriginatingPoints)
			{
				OutputPointData->InitializeFromData(PathData.OriginatingPointData);
			}
			OutputPointData->GetMutablePoints() = std::move(FinalPath);

			FPCGTaggedData& OutputData = Context->OutputData.TaggedData.Emplace_GetRef();
			OutputData.Data = OutputPointData;
		}

		return true;
	});
}

#undef LOCTEXT_NAMESPACE
