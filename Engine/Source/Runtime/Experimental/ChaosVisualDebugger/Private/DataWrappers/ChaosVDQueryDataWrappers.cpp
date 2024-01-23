// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDQueryDataWrappers.h"

bool FChaosVDCollisionResponseParams::Serialize(FArchive& Ar)
{
	Ar << FlagsPerChannel;
	Ar << bHasValidData;

	return !Ar.IsError();
}

bool FChaosVDCollisionObjectQueryParams::Serialize(FArchive& Ar)
{
	Ar << ObjectTypesToQuery;
	Ar << IgnoreMask;

	Ar << bHasValidData;

	return !Ar.IsError();
}

bool FChaosVDCollisionQueryParams::Serialize(FArchive& Ar)
{
	Ar << TraceTag;
	Ar << OwnerTag;
	Ar << bTraceComplex;
	Ar << bFindInitialOverlaps;
	Ar << bReturnFaceIndex;
	Ar << bReturnPhysicalMaterial;
	Ar << bIgnoreBlocks;
	Ar << bIgnoreTouches;
	Ar << bSkipNarrowPhase;
	Ar << bTraceIntoSubComponents;
	Ar << bReplaceHitWithSubComponents;
	Ar << IgnoreMask;
	Ar << IgnoredComponentsIDs;
	Ar << IgnoredActorsIDs;
	Ar << IgnoredComponentsNames;
	Ar << IgnoredActorsNames;

	Ar << bHasValidData;
	
	return !Ar.IsError();
}

bool FChaosVDQueryFastData::Serialize(FArchive& Ar)
{
	Ar << Dir;
	Ar << InvDir;
	Ar << CurrentLength;
	Ar << InvDir;

	//TODO : Pack these flags
	Ar << bParallel0;
	Ar << bParallel1;
	Ar << bParallel2;

	Ar << bHasValidData;

	return !Ar.IsError();
}

bool FChaosVDQueryHitData::Serialize(FArchive& Ar)
{
	Ar << Distance;
	Ar << FaceIdx;
	Ar << Flags;
	Ar << WorldPosition;
	Ar << WorldNormal;
	Ar << FaceNormal;
	Ar << bHasValidData;

	return !Ar.IsError();
}

bool FChaosVDQueryVisitStep::Serialize(FArchive& Ar)
{
	Ar << OwningQueryID;
	Ar << Type;
	Ar << ShapeIndex;
	Ar << ParticleIndex;
	Ar << ParticleTransform;
	Ar << HitType;
	Ar << HitData;

	Ar << QueryFastData;

	Ar << bHasValidData;

	return !Ar.IsError();
}

bool FChaosVDQueryDataWrapper::Serialize(FArchive& Ar)
{
	Ar << ID;
	Ar << ParentQueryID;
	Ar << WorldSolverID;
	Ar << bIsRetryQuery;
	Ar << InputGeometryKey;
	Ar << GeometryOrientation;
	Ar << Type;
	Ar << Mode;
	Ar << StartLocation;
	Ar << EndLocation;
	Ar << CollisionChannel;
	Ar << CollisionQueryParams;
	Ar << CollisionResponseParams;
	Ar << CollisionObjectQueryParams;

	// Hits and steps are intentionally not serialized as they are recorded as separated events, and reconstructed during trace analysis

	return !Ar.IsError();
}
