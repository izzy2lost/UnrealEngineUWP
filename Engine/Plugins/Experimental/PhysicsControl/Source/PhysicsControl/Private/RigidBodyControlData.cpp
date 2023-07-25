// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigidBodyControlData.h"

//======================================================================================================================
FName GetControlTypeName(const ERigidBodyMovementType MovementType)
{
	switch (MovementType)
	{
	case ERigidBodyMovementType::Kinematic:
		return "Kinematic";
	case ERigidBodyMovementType::Simulated:
		return "Simulated";
	}
	return "None";
}

//======================================================================================================================
FName GetControlTypeName(const ERigidBodyControlType ControlType)
{
	switch (ControlType)
	{
	case ERigidBodyControlType::ParentSpace:
		return "ParentSpace";
	case ERigidBodyControlType::WorldSpace:
		return "WorldSpace";
	}
	return "None";
}

#define INTERPOLATE_PARAM(NAME) Output.NAME = FMath::Lerp(A.NAME, B.NAME, Weight)
#define SET_ENABLED_PARAM(NAME) Output.bEnable##NAME = A.bEnable##NAME && B.bEnable##NAME

//======================================================================================================================
FRigidBodyControlData Interpolate(
	const FRigidBodyControlData& A, const FRigidBodyControlData& B, const float Weight)
{
	FRigidBodyControlData Output;
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
FRigidBodyControlSparseData Interpolate(
	const FRigidBodyControlSparseData& A, const FRigidBodyControlSparseData& B, const float Weight)
{
	FRigidBodyControlSparseData Output;
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
FRigidBodyModifierData Interpolate(
	const FRigidBodyModifierData& A, const FRigidBodyModifierData& B, const float Weight)
{
	FRigidBodyModifierData Output;
	Output.MovementType = (Weight < 0.5f) ? A.MovementType : B.MovementType;
	INTERPOLATE_PARAM(GravityMultiplier);
	return Output;
}

//======================================================================================================================
FRigidBodyModifierSparseData Interpolate(
	const FRigidBodyModifierSparseData& A, const FRigidBodyModifierSparseData& B, const float Weight)
{
	FRigidBodyModifierSparseData Output;
	Output.MovementType = (Weight < 0.5f) ? A.MovementType : B.MovementType;
	INTERPOLATE_PARAM(GravityMultiplier);

	SET_ENABLED_PARAM(MovementType);
	SET_ENABLED_PARAM(GravityMultiplier);
	return Output;
}

#undef INTERPOLATE_PARAM
#undef SET_ENABLED_PARAM

//======================================================================================================================
FControlRecord::FControlRecord(const FRigidBodyControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle)
	: Control(InControl)
	, JointHandle(InJointHandle)
	, CurrentData(Control.ControlData)
{
}

//======================================================================================================================
void FControlRecord::ResetCurrent(bool bResetTarget)
{
	CurrentData = Control.ControlData;

	if (bResetTarget)
	{
		ControlTarget = FRigidBodyControlTarget();
	}
}

//======================================================================================================================
FBodyModifierRecord::FBodyModifierRecord(const FRigidBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle)
	: Modifier(InModifier)
	, ActorHandle(InActorHandle)
	, CurrentData(Modifier.ModifierData)
{
}

//======================================================================================================================
void FBodyModifierRecord::ResetCurrent()
{
	CurrentData = Modifier.ModifierData;
}

#define SET_SPARSE_DATA(NAME) NAME = SparseData.bEnable##NAME ? SparseData.NAME : NAME

//======================================================================================================================
void FRigidBodyControlData::UpdateFromSparseData(const FRigidBodyControlSparseData& SparseData)
{
	SET_SPARSE_DATA(LinearStrength);
	SET_SPARSE_DATA(LinearDampingRatio);
	SET_SPARSE_DATA(LinearExtraDamping);
	SET_SPARSE_DATA(AngularStrength);
	SET_SPARSE_DATA(AngularDampingRatio);
	SET_SPARSE_DATA(AngularExtraDamping);
	SET_SPARSE_DATA(LinearTargetVelocityMultiplier);
	SET_SPARSE_DATA(AngularTargetVelocityMultiplier);
	SET_SPARSE_DATA(bEnabled);
}

//======================================================================================================================
void FRigidBodyModifierData::UpdateFromSparseData(const FRigidBodyModifierSparseData& SparseData)
{
	SET_SPARSE_DATA(MovementType);
	SET_SPARSE_DATA(GravityMultiplier);
}

#undef SET_SPARSE_DATA
