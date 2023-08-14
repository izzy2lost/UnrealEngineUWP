// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlData.h"

#define SET_SPARSE_DATA(NAME) NAME = SparseData.bEnable##NAME ? SparseData.NAME : NAME

//======================================================================================================================
void FPhysicsControlData::UpdateFromSparseData(const FPhysicsControlSparseData& SparseData)
{
	SET_SPARSE_DATA(LinearStrength);
	SET_SPARSE_DATA(LinearDampingRatio);
	SET_SPARSE_DATA(LinearExtraDamping);
	SET_SPARSE_DATA(MaxForce);
	SET_SPARSE_DATA(AngularStrength);
	SET_SPARSE_DATA(AngularDampingRatio);
	SET_SPARSE_DATA(AngularExtraDamping);
	SET_SPARSE_DATA(MaxTorque);
	SET_SPARSE_DATA(LinearTargetVelocityMultiplier);
	SET_SPARSE_DATA(AngularTargetVelocityMultiplier);
	SET_SPARSE_DATA(bEnabled);
}

//======================================================================================================================
void FPhysicsControlModifierData::UpdateFromSparseData(const FPhysicsControlModifierSparseData& SparseData)
{
	SET_SPARSE_DATA(MovementType);
	SET_SPARSE_DATA(GravityMultiplier);
}

#undef SET_SPARSE_DATA
