// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigidBodyControlData.h"

#define INTERPOLATE_PARAM(NAME) Output.NAME = FMath::Lerp(A.NAME, B.NAME, Weight)
#define SET_ENABLED_PARAM(NAME) Output.bEnable##NAME = A.bEnable##NAME && B.bEnable##NAME

//======================================================================================================================
FPhysicsControlData Interpolate(
	const FPhysicsControlData& A, const FPhysicsControlData& B, const float Weight)
{
	FPhysicsControlData Output;
	INTERPOLATE_PARAM(LinearStrength);
	INTERPOLATE_PARAM(LinearDampingRatio);
	INTERPOLATE_PARAM(LinearExtraDamping);
	INTERPOLATE_PARAM(AngularStrength);
	INTERPOLATE_PARAM(AngularDampingRatio);
	INTERPOLATE_PARAM(AngularExtraDamping);
	INTERPOLATE_PARAM(LinearTargetVelocityMultiplier);
	INTERPOLATE_PARAM(AngularTargetVelocityMultiplier);
	Output.bEnabled = (Weight < 0.5f) ? A.bEnabled : B.bEnabled;
	return Output;
}

//======================================================================================================================
FPhysicsControlSparseData Interpolate(
	const FPhysicsControlSparseData& A, const FPhysicsControlSparseData& B, const float Weight)
{
	FPhysicsControlSparseData Output;
	INTERPOLATE_PARAM(LinearStrength);
	INTERPOLATE_PARAM(LinearDampingRatio);
	INTERPOLATE_PARAM(LinearExtraDamping);
	INTERPOLATE_PARAM(AngularStrength);
	INTERPOLATE_PARAM(AngularDampingRatio);
	INTERPOLATE_PARAM(AngularExtraDamping);
	INTERPOLATE_PARAM(LinearTargetVelocityMultiplier);
	INTERPOLATE_PARAM(AngularTargetVelocityMultiplier);
	Output.bEnabled = (Weight < 0.5f) ? A.bEnabled : B.bEnabled;

	SET_ENABLED_PARAM(LinearStrength);
	SET_ENABLED_PARAM(LinearDampingRatio);
	SET_ENABLED_PARAM(LinearExtraDamping);
	SET_ENABLED_PARAM(AngularStrength);
	SET_ENABLED_PARAM(AngularDampingRatio);
	SET_ENABLED_PARAM(AngularExtraDamping);
	SET_ENABLED_PARAM(LinearTargetVelocityMultiplier);
	SET_ENABLED_PARAM(AngularTargetVelocityMultiplier);
	SET_ENABLED_PARAM(bEnabled);

	return Output;
}

//======================================================================================================================
FPhysicsControlModifierData Interpolate(
	const FPhysicsControlModifierData& A, const FPhysicsControlModifierData& B, const float Weight)
{
	FPhysicsControlModifierData Output;
	Output.MovementType = (Weight < 0.5f) ? A.MovementType : B.MovementType;
	INTERPOLATE_PARAM(GravityMultiplier);
	return Output;
}

//======================================================================================================================
FPhysicsControlModifierSparseData Interpolate(
	const FPhysicsControlModifierSparseData& A, const FPhysicsControlModifierSparseData& B, const float Weight)
{
	FPhysicsControlModifierSparseData Output;
	Output.MovementType = (Weight < 0.5f) ? A.MovementType : B.MovementType;
	INTERPOLATE_PARAM(GravityMultiplier);

	SET_ENABLED_PARAM(MovementType);
	SET_ENABLED_PARAM(GravityMultiplier);
	return Output;
}

#undef INTERPOLATE_PARAM
#undef SET_ENABLED_PARAM

//======================================================================================================================
FRigidBodyControlRecord::FRigidBodyControlRecord(const FRigidBodyControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle)
	: Control(InControl)
	, JointHandle(InJointHandle)
	, CurrentData(Control.ControlData)
	, ChildBodyIndex(-1)
	, ParentBodyIndex(-1)
{
}

//======================================================================================================================
void FRigidBodyControlRecord::ResetCurrent(bool bResetTarget)
{
	CurrentData = Control.ControlData;

	if (bResetTarget)
	{
		ControlTarget = FRigidBodyControlTarget();
	}
}

//======================================================================================================================
FRigidBodyModifierRecord::FRigidBodyModifierRecord(const FRigidBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle)
	: Modifier(InModifier)
	, ActorHandle(InActorHandle)
	, CurrentData(Modifier.ModifierData)
{
}

//======================================================================================================================
void FRigidBodyModifierRecord::ResetCurrent()
{
	CurrentData = Modifier.ModifierData;
}

