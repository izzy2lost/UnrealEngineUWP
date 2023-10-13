// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreatePointsGrid.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreatePointsGridElement"

TArray<FPCGPinProperties> UPCGCreatePointsGridSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

FPCGElementPtr UPCGCreatePointsGridSettings::CreateElement() const
{
	return MakeShared<FPCGCreatePointsGridElement>();
}

bool FPCGCreatePointsGridElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreatePointsGridElement::Execute);
	
	check(Context);

	const UPCGCreatePointsGridSettings* Settings = Context->GetInputSettings<UPCGCreatePointsGridSettings>();
	check(Settings);

	UPCGComponent* PCGComponent = nullptr;
	check(Context->SourceComponent.Get());

	if (Settings->GridPivot == EPCGGridPivot::OriginalComponent)
	{
		PCGComponent = Context->SourceComponent->GetOriginalComponent();
	}
	else if (Settings->GridPivot == EPCGGridPivot::LocalComponent)
	{
		PCGComponent = Context->SourceComponent.Get();
	}

	FTransform OriginalComponentTransform = FTransform();
	FTransform ComponentTransformScaleOne = FTransform();
	const UPCGSpatialData* Target = nullptr;

	if (PCGComponent)
	{
		check(PCGComponent->GetOwner());

		OriginalComponentTransform = PCGComponent->GetOwner()->GetActorTransform();
		ComponentTransformScaleOne = FTransform(OriginalComponentTransform.Rotator(), OriginalComponentTransform.GetLocation(), FVector::One());
		Target = Settings->bCullPointsOutsideVolume ? Cast<UPCGSpatialData>(PCGComponent->GetActorPCGData()) : nullptr;
	}

	if (Settings->CellSize.X <= 0.0 || Settings->CellSize.Y <= 0.0 || Settings->CellSize.Z <= 0.0)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidCellDataInput", "CellSize must not be less than 0"));
		return true;
	}

	if (Settings->GridExtents.X < 0.0 || Settings->GridExtents.Y < 0.0 || Settings->GridExtents.Z < 0.0)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidGridDataInput", "GridExtents must not be less than 0"));
		return true;
	}

	FVector GridExtents = Settings->GridExtents;
	const FVector& CellSize = Settings->CellSize;

	int32 PointCountX = FMath::TruncToInt((2 * GridExtents.X) / CellSize.X);
	int32 PointCountY = FMath::TruncToInt((2 * GridExtents.Y) / CellSize.Y);
	int32 PointCountZ = FMath::TruncToInt((2 * GridExtents.Z) / CellSize.Z);

	if (Settings->PointPosition == EPCGPointPosition::CellCorners)
	{ 
		PointCountX++;
		PointCountY++;
		PointCountZ++;

		if (GridExtents.X < (CellSize.X / 2))
		{
			GridExtents.X = 0.0;
		}

		if (GridExtents.Y < (CellSize.Y / 2))
		{
			GridExtents.Y = 0.0;
		}

		if (GridExtents.Z < (CellSize.Z / 2))
		{
			GridExtents.Z = 0.0;
		}
	}

	// If the GridExtent would produce an off center result, snap it to the center of the grid
	if (Settings->PointPosition == EPCGPointPosition::CellCenter)
	{
		if (GridExtents.X < (CellSize.X / 2))
		{
			PointCountX++;
		}

		if (GridExtents.Y < (CellSize.Y / 2))
		{
			PointCountY++;
		}

		if (GridExtents.Z < (CellSize.Z / 2))
		{
			PointCountZ++;
		}

		GridExtents.X = GridExtents.X - FMath::Fmod(GridExtents.X, CellSize.X / 2);
		GridExtents.Y = GridExtents.Y - FMath::Fmod(GridExtents.Y, CellSize.Y / 2);
		GridExtents.Z = GridExtents.Z - FMath::Fmod(GridExtents.Z, CellSize.Z / 2);
	}

	const int64 NumIterations64 = static_cast<int64>(PointCountX) * static_cast<int64>(PointCountY) * static_cast<int64>(PointCountZ);

	if (NumIterations64 <= 0)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidNumberOfIterations", "The number of iterations produced cannot be less than or equal to 0."));
		return true;
	}

	if (NumIterations64 >= MAX_int32)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("Overflow_int32", "The number of iterations produced is larger than what a 32-bit integer can hold."));
		return true;
	}

	if (PCGFeatureSwitches::CVarCheckSamplerMemory.GetValueOnAnyThread() && FPlatformMemory::GetStats().AvailablePhysical < sizeof(FPCGPoint) * NumIterations64)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MemoryOverflow", "The number of iterations produced is larger than available memory."));
		return true;
	}

	int32 NumIterations = static_cast<int32>(NumIterations64);
	
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGPointData* PtData = NewObject<UPCGPointData>();
	check(PtData);

	TArray<FPCGPoint>& OutputPoints = PtData->GetMutablePoints();
	Output.Data = PtData;

	FPCGAsync::AsyncPointProcessing(Context, NumIterations, OutputPoints, [Settings, Target, &PointCountX, &PointCountY, &CellSize, &GridExtents, &ComponentTransformScaleOne](int32 Index, FPCGPoint& OutPoint)
	{
		OutPoint = FPCGPoint();

		double XCoordinate = Index % PointCountX;
		double YCoordinate = (Index / PointCountX) % PointCountY;
		double ZCoordinate = Index / (PointCountX * PointCountY);

		if (Settings->PointPosition == EPCGPointPosition::CellCenter)
		{
			// If extents are smaller than a point in that dimension, place in center, otherwise offset 
			XCoordinate += 0.5;
			YCoordinate += 0.5;
			ZCoordinate += 0.5;
		}

		// If the extends are smaller than the point, set point to origin
		if (GridExtents.X < CellSize.X / 2.0)
		{
			XCoordinate = 0.0;
		}
		if (GridExtents.Y < CellSize.Y / 2.0)
		{
			YCoordinate = 0.0;
		}
		if (GridExtents.Z < CellSize.Z / 2.0)
		{
			ZCoordinate = 0.0;
		}

		const FVector GridTransformLocation((CellSize.X * XCoordinate) - GridExtents.X, (CellSize.Y * YCoordinate) - GridExtents.Y, (CellSize.Z * ZCoordinate) - GridExtents.Z);

		if (Settings->GridPivot == EPCGGridPivot::LocalComponent || Settings->GridPivot == EPCGGridPivot::OriginalComponent)
		{
			OutPoint.Transform = FTransform(FRotator::ZeroRotator, GridTransformLocation, FVector::One()) * ComponentTransformScaleOne;
		}
		else
		{
			OutPoint.Transform.SetLocation(GridTransformLocation);
		}

		if (Settings->bSetPointsBounds)
		{
			OutPoint.SetExtents(CellSize * 0.5);
		}

		OutPoint.Seed = PCGHelpers::ComputeSeed(OutPoint.Transform.GetLocation().X, OutPoint.Transform.GetLocation().Y, OutPoint.Transform.GetLocation().Z);

		// Discards points outside of the volume
		return !Target || (Target->GetDensityAtPosition(OutPoint.Transform.GetLocation()) > 0.0f);
	});

	return true;
}

void FPCGCreatePointsGridElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);
	
	if (const UPCGCreatePointsGridSettings* Settings = Cast<UPCGCreatePointsGridSettings>(InSettings))
	{
		int GridPivot = static_cast<int>(EPCGGridPivot::Global);
		bool bCullPointsOutsideVolume = false;
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings, GET_MEMBER_NAME_CHECKED(UPCGCreatePointsGridSettings, GridPivot), static_cast<int>(Settings->GridPivot), GridPivot);
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings, GET_MEMBER_NAME_CHECKED(UPCGCreatePointsGridSettings, bCullPointsOutsideVolume), Settings->bCullPointsOutsideVolume, bCullPointsOutsideVolume);
		
		// We're using the bounds of the pcg volume, so we extract the actor data here
		EPCGGridPivot EnumGridPivot = static_cast<EPCGGridPivot>(GridPivot);

		if ((EnumGridPivot == EPCGGridPivot::OriginalComponent || EnumGridPivot == EPCGGridPivot::LocalComponent || bCullPointsOutsideVolume) && InComponent)
		{
			if (const UPCGData* Data = InComponent->GetActorPCGData())
			{
				Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
			}
		}
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
