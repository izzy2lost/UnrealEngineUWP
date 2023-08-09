// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMutateSeed.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGMutateSeedSettings"

UPCGMutateSeedSettings::UPCGMutateSeedSettings() 
{
	bUseSeed = true;
}

FPCGElementPtr UPCGMutateSeedSettings::CreateElement() const
{
	return MakeShared<FPCGMutateSeedElement>();
}

bool FPCGMutateSeedElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMutateSeedElement::Execute);
	
	check(Context);

	const UPCGMutateSeedSettings* Settings = Context->GetInputSettings<UPCGMutateSeedSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const int Seed = Context->GetSeed();

	ProcessPoints(Context, Inputs, Outputs, [Seed](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;
		OutPoint.Seed = PCGHelpers::ComputeSeed(UPCGBlueprintHelpers::ComputeSeedFromPosition(OutPoint.Transform.GetLocation()), Seed, OutPoint.Seed);
		return true;
	});
	
	return true;
}

#undef LOCTEXT_NAMESPACE
