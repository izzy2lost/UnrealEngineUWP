// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPathfindingElement.h"

#include "PCGContext.h"
#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "SpatialAlgo/PCGAStar.h"

#include "Components/SplineComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

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

UPCGPathfindingSettings::UPCGPathfindingSettings()
{
	// In most cases, we're not going to be interested in checking for occlusion by the landscape itself, as we'll be pathfinding on the landscape.
	PathTraceParams.SelectLandscapeHits = EPCGWorldQuerySelectLandscapeHits::Exclude;
}

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

bool FPCGPathfindingElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGPathfindingSettings* Settings = Cast<const UPCGPathfindingSettings>(InSettings);
	return !Settings || !Settings->bUsePathTraces;
}

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
			.GoalPoint = FPCGPoint(FTransform(Settings->Goal), /*InDensity=*/1.0, PCGHelpers::ComputeSeedFromPosition(Settings->Goal)),
			.SearchDistance = Settings->SearchDistance,
			.Start = Settings->Start,
			.Goal = Settings->Goal,
			.HeuristicWeight = Settings->HeuristicWeight,
			.bAcceptPartialPath = Settings->bAcceptPartialPath,
			.bCopyOriginatingPoints = Settings->bCopyOriginatingPoints
		};

		return EPCGTimeSliceInitResult::Success;
	});

	Context->InitializePerIterationStates(PointInputs.Num(), [&PointInputs, Settings, Context](IterStateType& OutSearchState, const ExecStateType& SearchSettings, const uint32 IterationIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGClusterElement::PrepareData::InitializePerIterationStates);

		OutSearchState.OriginatingPointData = Cast<UPCGPointData>(PointInputs[IterationIndex].Data);
		if (OutSearchState.OriginatingPointData->IsEmpty())
		{
			// Already confirmed we can't make it from start->goal in the execution state check.
			return EPCGTimeSliceInitResult::NoOperation;
		}

		// Build cost attribute accessor if required
		TSharedPtr<const IPCGAttributeAccessor> CostAccessor = nullptr;

		if (Settings->CostFunctionMode != EPCGPathfindingCostFunctionMode::Distance)
		{
			const FPCGAttributePropertyInputSelector Selector = Settings->CostAttribute.CopyAndFixLast(OutSearchState.OriginatingPointData);
			CostAccessor = MakeShareable(PCGAttributeAccessorHelpers::CreateConstAccessor(OutSearchState.OriginatingPointData, Selector).Release());

			FPCGAttributeAccessorKeysPoints Keys(OutSearchState.OriginatingPointData->GetPoints());

			if (!CostAccessor)
			{
				PCGLog::Metadata::LogFailToCreateAccessorError(Selector, Context);
			}
			else if(!PCG::Private::IsBroadcastableOrConstructible(CostAccessor->GetUnderlyingType(), PCG::Private::MetadataTypes<double>::Id))
			{
				PCGLog::Metadata::LogFailToGetAttributeError<double>(Selector, CostAccessor.Get(), Context);
				CostAccessor = nullptr;
			}
		}

		TFunction<bool(const FVector&, const FVector&)> LineTraceTest = [](const FVector&, const FVector&) -> bool { return true; };

		if (Settings->bUsePathTraces)
		{
			if (UWorld* World = Context->SourceComponent.Get() ? Context->SourceComponent->GetWorld() : nullptr)
			{
				FPCGWorldRaycastQueryParams PathTraceParams = Settings->PathTraceParams;
				PathTraceParams.Initialize();

				TWeakObjectPtr<UPCGComponent> OriginatingComponent = Context->SourceComponent;
				FCollisionObjectQueryParams ObjectQueryParams(PathTraceParams.CollisionChannel);
				FCollisionQueryParams Params;
				Params.bTraceComplex = PathTraceParams.bTraceComplex;

				LineTraceTest = [World, ObjectQueryParams = MoveTemp(ObjectQueryParams), Params = MoveTemp(Params), OriginatingComponent, PathTraceParams = MoveTemp(PathTraceParams)](const FVector& StartPosition, const FVector& EndPosition) -> bool
				{
					TArray<FHitResult> OutHits;
					if (World->LineTraceMultiByObjectType(OutHits, StartPosition, EndPosition, ObjectQueryParams, Params))
					{
						TOptional<FHitResult> HitResult = PCGWorldQueryHelpers::FilterRayHitResults(&PathTraceParams, OriginatingComponent, OutHits);
						return !HitResult.IsSet();
					}
					else
					{
						return true;
					}
				};
			}
		}

		if (CostAccessor)
		{
			if (Settings->CostFunctionMode == EPCGPathfindingCostFunctionMode::FitnessScore)
			{
				const double MaxFitnessPenaltyFactor = FMath::Max(Settings->MaximumFitnessPenaltyFactor, 1.0);

				OutSearchState.CostFunction = [FitnessAccessor = CostAccessor, MaxFitnessPenaltyFactor, PathTraceTest = MoveTemp(LineTraceTest)](const double PreviousNodeCost, const FPCGPoint* PreviousNodePoint, const double DistanceToCurrentSquared, const FPCGPoint* CurrentNodePoint)
				{
					if(!PathTraceTest(PreviousNodePoint->Transform.GetLocation(), CurrentNodePoint->Transform.GetLocation()))
					{
						return std::numeric_limits<double>::max();
					}

					double FitnessScore = 1.0;
					FPCGAttributeAccessorKeysPoints Key(*CurrentNodePoint);

					FitnessAccessor->Get(FitnessScore, Key, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
					FitnessScore = FMath::Clamp(FitnessScore, 0.0, 1.0);

					return PreviousNodeCost + (1.0 - FitnessScore) * MaxFitnessPenaltyFactor * FMath::Sqrt(DistanceToCurrentSquared);
				};
			}
			else if (Settings->CostFunctionMode == EPCGPathfindingCostFunctionMode::CostMultipler)
			{
				OutSearchState.CostFunction = [MultiplierAccessor = CostAccessor, PathTraceTest = MoveTemp(LineTraceTest)](const double PreviousNodeCost, const FPCGPoint* PreviousNodePoint, const double DistanceToCurrentSquared, const FPCGPoint* CurrentNodePoint)
				{
					if (!PathTraceTest(PreviousNodePoint->Transform.GetLocation(), CurrentNodePoint->Transform.GetLocation()))
					{
						return std::numeric_limits<double>::max();
					}

					double Multiplier = 1.0;
					FPCGAttributeAccessorKeysPoints Key(*CurrentNodePoint);

					MultiplierAccessor->Get(Multiplier, Key, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
					Multiplier = FMath::Max(Multiplier, 1.0);

					return PreviousNodeCost + Multiplier * FMath::Sqrt(DistanceToCurrentSquared);
				};
			}
			else
			{
				checkNoEntry();
			}
		}
		else if (Settings->bUsePathTraces)
		{
			// Use distance but with line trace
			OutSearchState.CostFunction = [PathTraceTest = MoveTemp(LineTraceTest)](const double PreviousNodeCost, const FPCGPoint* PreviousNodePoint, const double DistanceToCurrentSquared, const FPCGPoint* CurrentNodePoint)
			{
				if (!PathTraceTest(PreviousNodePoint->Transform.GetLocation(), CurrentNodePoint->Transform.GetLocation()))
				{
					return std::numeric_limits<double>::max();
				}
				else
				{
					return PCGSpatialAlgo::AStar::Cost::CalculateCost_EuclideanDistance(PreviousNodeCost, PreviousNodePoint, DistanceToCurrentSquared, CurrentNodePoint);
				}
			};
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
