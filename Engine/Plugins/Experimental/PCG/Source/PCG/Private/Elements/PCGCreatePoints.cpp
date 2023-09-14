// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreatePoints.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreatePointsElement"

UPCGCreatePointsSettings::UPCGCreatePointsSettings()
{
	// Add one default point in the array
	PointsToCreate.Add(FPCGPoint());
}

TArray<FPCGPinProperties> UPCGCreatePointsSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

FPCGElementPtr UPCGCreatePointsSettings::CreateElement() const
{
	return MakeShared<FPCGCreatePointsElement>();
}

bool FPCGCreatePointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreatePointsElement::Execute);
	
	check(Context);

	const UPCGCreatePointsSettings* Settings = Context->GetInputSettings<UPCGCreatePointsSettings>();
	check(Settings);

	UPCGComponent* OriginalComponentContext = Context->SourceComponent.Get();
	check(OriginalComponentContext);

	const FTransform OriginalComponentTransform = OriginalComponentContext->GetOwner()->GetActorTransform();
	const FTransform ComponentTransformScaleOne = FTransform(OriginalComponentTransform.Rotator(), OriginalComponentTransform.GetLocation(), FVector::One());

	TArray<FPCGPoint> PointsToLoopOn = Settings->PointsToCreate;
	
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGPointData* PtData = NewObject<UPCGPointData>();
	check(PtData); 

	TArray<FPCGPoint>& OutputPoints = PtData->GetMutablePoints();
	Output.Data = PtData;
	

	if (!Settings->bLocal && !Settings->bCullPointsOutsideVolume)
	{
		for (auto& Points : PointsToLoopOn)
		{
			if (Points.Seed == 0)
			{
				// If the seed is the default value, generate a new seed based on the its transform
				Points.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Points.Transform.GetLocation());
			}
		}

		PtData->SetPoints(PointsToLoopOn);
	}
	else
	{
		const UPCGSpatialData* Target = Settings->bCullPointsOutsideVolume ? Cast<UPCGSpatialData>(OriginalComponentContext->GetActorPCGData()) : nullptr;

		FPCGAsync::AsyncPointProcessing(Context, PointsToLoopOn.Num(), OutputPoints, [&PointsToLoopOn, Settings, &ComponentTransformScaleOne, Target](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& InPoint = PointsToLoopOn[Index];
			OutPoint = InPoint;

			if (Settings->bLocal)
			{
				OutPoint.Transform *= ComponentTransformScaleOne;
			}

			OutPoint.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(OutPoint.Transform.GetLocation());

			// Discards all points that are outside the volume
			return !Target || (Target->GetDensityAtPosition(OutPoint.Transform.GetLocation()) > 0.0f);
		});
	}

	return true;
}

bool FPCGCreatePointsElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreatePointsSettings* Settings = Cast<const UPCGCreatePointsSettings>(InSettings);

	return Settings && !Settings->bLocal;
}

#undef LOCTEXT_NAMESPACE
