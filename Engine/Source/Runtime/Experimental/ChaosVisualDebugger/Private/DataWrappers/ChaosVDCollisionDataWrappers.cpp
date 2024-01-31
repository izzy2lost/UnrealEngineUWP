// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

#ifndef CVD_SERIALIZE_STATIC_ARRAY
	#define CVD_SERIALIZE_STATIC_ARRAY(Archive, Array) \
		{ \
			constexpr int32 Size = UE_ARRAY_COUNT(Array) ; \
			for (int32 Index = 0; Index < Size; Index++)\
			{\
				Archive << Array[Index]; \
			}\
		}
#endif

bool FChaosVDContactPoint::Serialize(FArchive& Ar)
{
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeContactPoints);
	Ar << ShapeContactNormal;
	Ar << Phi;
	Ar << FaceIndex;
	Ar << ContactType;

	return true;
}

bool FChaosVDManifoldPoint::Serialize(FArchive& Ar)
{
	FArchive_Serialize_BitfieldBool(Ar, bDisabled);
	FArchive_Serialize_BitfieldBool(Ar, bWasRestored);
	FArchive_Serialize_BitfieldBool(Ar, bWasReplaced);

	FArchive_Serialize_BitfieldBool(Ar, bHasStaticFrictionAnchor);
	FArchive_Serialize_BitfieldBool(Ar, bIsValid);
	FArchive_Serialize_BitfieldBool(Ar, bInsideStaticFrictionCone);

	Ar << NetPushOut;
	Ar << NetImpulse;

	Ar << TargetPhi;
	Ar << InitialPhi;

	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeAnchorPoints);
	CVD_SERIALIZE_STATIC_ARRAY(Ar, InitialShapeContactPoints);

	Ar << ContactPoint;
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeContactPoints);

	return true;
}

bool FChaosVDCollisionMaterial::Serialize(FArchive& Ar)
{
	Ar << FaceIndex;
	Ar << MaterialDynamicFriction;
	Ar << MaterialStaticFriction;
	Ar << MaterialRestitution;
	Ar << DynamicFriction;
	Ar << StaticFriction;
	Ar << Restitution;
	Ar << RestitutionThreshold;
	Ar << InvMassScale0;
	Ar << InvMassScale1;
	Ar << InvInertiaScale0;
	Ar << InvInertiaScale1;

	return true;
}

bool FChaosVDConstraint::Serialize(FArchive& Ar)
{
	FArchive_Serialize_BitfieldBool(Ar, bIsCurrent);
	FArchive_Serialize_BitfieldBool(Ar, bDisabled);
	FArchive_Serialize_BitfieldBool(Ar, bUseManifold);
	FArchive_Serialize_BitfieldBool(Ar, bUseIncrementalManifold);
	FArchive_Serialize_BitfieldBool(Ar, bCanRestoreManifold);
	FArchive_Serialize_BitfieldBool(Ar, bWasManifoldRestored);
	FArchive_Serialize_BitfieldBool(Ar, bIsQuadratic0);
	FArchive_Serialize_BitfieldBool(Ar, bIsQuadratic1);
	FArchive_Serialize_BitfieldBool(Ar, bIsProbe);
	FArchive_Serialize_BitfieldBool(Ar, bCCDEnabled);
	FArchive_Serialize_BitfieldBool(Ar, bCCDSweepEnabled);
	FArchive_Serialize_BitfieldBool(Ar, bModifierApplied);
	FArchive_Serialize_BitfieldBool(Ar, bMaterialSet);

	Ar << Material;
	Ar << AccumulatedImpulse;
	Ar << ShapesType;

	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeWorldTransforms);
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ImplicitTransforms);

	Ar << CullDistance;
	Ar << CollisionMargins;	
	Ar << CollisionTolerance;	
	Ar << ClosestManifoldPointIndex;	
	Ar << ExpectedNumManifoldPoints;	
	Ar << LastShapeWorldPositionDelta;
	Ar << LastShapeWorldRotationDelta;
	Ar << Stiffness;
	Ar << MinInitialPhi;
	Ar << InitialOverlapDepenetrationVelocity;
	Ar << CCDTimeOfImpact;
	Ar << CCDEnablePenetration;
	Ar << CCDTargetPenetration;
	Ar << ManifoldPoints;
	Ar << SolverID;
	Ar << Particle0Index;
	Ar << Particle1Index;

	return true;
}


bool FChaosVDParticlePairMidPhase::Serialize(FArchive& Ar)
{
	Ar << SolverID;
	
	FArchive_Serialize_BitfieldBool(Ar, bIsActive);
	FArchive_Serialize_BitfieldBool(Ar, bIsCCD);
	FArchive_Serialize_BitfieldBool(Ar, bIsCCDActive);
	FArchive_Serialize_BitfieldBool(Ar, bIsSleeping);
	FArchive_Serialize_BitfieldBool(Ar, bIsModified);
		
	Ar << LastUsedEpoch;

	Ar << Particle0Idx;
	Ar << Particle1Idx;

	Ar << Constraints;

	return true;
}

bool FChaosVDCollisionFilterData::Serialize(FArchive& Ar)
{
	Ar << Word0;
	Ar << Word1;
	Ar << Word2;
	Ar << Word3;

	return !Ar.IsError();
}

bool FChaosVDShapeCollisionData::Serialize(FArchive& Ar)
{
	Ar << CollisionTraceType;
	
	FArchive_Serialize_BitfieldBool(Ar, bSimCollision);
	FArchive_Serialize_BitfieldBool(Ar, bQueryCollision);
	FArchive_Serialize_BitfieldBool(Ar, bIsProbe);

	return true;
}

bool FChaosVDShapeCollisionData::operator==(const FChaosVDShapeCollisionData& Other) const
{
	return CollisionTraceType == Other.CollisionTraceType
			&& bSimCollision == Other.bSimCollision
			&& bQueryCollision == Other.bQueryCollision
			&& bIsProbe == Other.bIsProbe;
}
