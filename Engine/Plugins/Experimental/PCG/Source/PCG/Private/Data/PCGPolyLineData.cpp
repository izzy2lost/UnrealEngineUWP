// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPolyLineData)

FBox UPCGPolyLineData::GetBounds() const
{
	FBox Bounds(EForceInit::ForceInit);

	const int NumSegments = GetNumSegments();

	for (int SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		Bounds += GetLocationAtDistance(SegmentIndex, 0);
		Bounds += GetLocationAtDistance(SegmentIndex, GetSegmentLength(SegmentIndex));
	}
	
	return Bounds;
}

void UPCGPolyLineData::GetTangentsAtSegmentStart(int SegmentIndex, FVector& OutArriveTangent, FVector& OutLeaveTangent) const
{
	OutArriveTangent = FVector::Zero();
	OutLeaveTangent = FVector::Zero();
}
