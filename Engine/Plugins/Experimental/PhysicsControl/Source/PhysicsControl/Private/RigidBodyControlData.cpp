// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigidBodyControlData.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"

//======================================================================================================================
FRigidBodyControlRecord::FRigidBodyControlRecord(
	const FRigidBodyControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle)
	: Control(InControl)
	, JointHandle(InJointHandle)
	, ControlData(Control.ControlData)
	, ChildBodyIndex(-1)
	, ParentBodyIndex(-1)
{
}

//======================================================================================================================
void FRigidBodyControlRecord::ResetCurrent(bool bResetTarget)
{
	ControlData = Control.ControlData;

	if (bResetTarget)
	{
		ControlTarget = FRigidBodyControlTarget();
	}
}

//======================================================================================================================
FVector FRigidBodyControlRecord::GetControlPoint(const ImmediatePhysics::FActorHandle* ChildActorHandle) const
{
	if (ControlData.bUseCustomControlPoint)
	{
		return ControlData.CustomControlPoint;
	}
	return ChildActorHandle->GetLocalCoMLocation();
}

//======================================================================================================================
FRigidBodyModifierRecord::FRigidBodyModifierRecord(
	const FRigidBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle)
	: Modifier(InModifier)
	, ActorHandle(InActorHandle)
	, ModifierData(Modifier.ModifierData)
{
}

//======================================================================================================================
void FRigidBodyModifierRecord::ResetCurrent()
{
	ModifierData = Modifier.ModifierData;
}

