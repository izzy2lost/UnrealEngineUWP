// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/DeferredForcesModular.h"
#include "SimModule/SimulationModuleBase.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/Utilities.h"
#include "Chaos/ParticleHandle.h"

#if CHAOS_DEBUG_DRAW
#include "Chaos/DebugDrawQueue.h"
#endif

FCoreModularVehicleDebugParams GCoreModularVehicleDebugParams;

#if CHAOS_DEBUG_DRAW
FAutoConsoleVariableRef CVarChaosModularVehiclesShowMass(TEXT("p.ModularVehicle.ShowMass"), GCoreModularVehicleDebugParams.ShowMass, TEXT("Show mass and inertia above objects."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowForces(TEXT("p.ModularVehicle.ShowForces"), GCoreModularVehicleDebugParams.ShowForces, TEXT("Show forces on modular vehicles."));
FAutoConsoleVariableRef CVarChaosModularVehiclesDrawForceScaling(TEXT("p.ModularVehicle.DrawForceScaling"), GCoreModularVehicleDebugParams.DrawForceScaling, TEXT("Set scaling of modular vehicle debug draw forces."));
FAutoConsoleVariableRef CVarChaosModularVehiclesDisableForces(TEXT("p.ModularVehicle.DisableForces"), GCoreModularVehicleDebugParams.DisableForces, TEXT("Disable all forces."));
#endif

// #TODO: passing all these parameters in is horrible, tidy this up now we know what needs to be done
Chaos::FPBDRigidParticleHandle* FDeferredForcesModular::GetParticle(TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles
		, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles
		, int TransformIndex
		, const TManagedArray<FTransform>& Transforms
		, const TManagedArray<FTransform>& CollectionMassToLocal
		, const TManagedArray<int32>& Parent
		, FTransform& TransformOut)
{

	Chaos::FPBDRigidParticleHandle* SimParticle = Particles[TransformIndex];
	Chaos::FPBDRigidClusteredParticleHandle* SimClusterParticle = ClusterParticles[TransformIndex];

	if (SimParticle && !SimParticle->Disabled())
	{
		TransformOut = FTransform::Identity;
		return SimParticle;		// apply to broken off child
	}
	else if (SimClusterParticle && !SimClusterParticle->Disabled())
	{
		TransformOut.SetLocation(CollectionMassToLocal[Parent[TransformIndex]].InverseTransformPosition(Transforms[TransformIndex].GetLocation()));
		TransformOut.SetRotation(Transforms[TransformIndex].GetRotation());

		return SimClusterParticle; // apply to intact cluster
	}

	return nullptr;
}

Chaos::FPBDRigidParticleHandle* FDeferredForcesModular::GetParticle(const FTransform& OffsetTransform
	, TArray<Chaos::FPBDRigidParticleHandle*>& Particles
	, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles
	, int TransformIndex
	, FTransform& TransformOut)
{
	using namespace Chaos;

	ensure(ClusterParticles.Num() == 1);
	const int SingleChassisIndex = 0;
	TransformOut = FTransform::Identity;

	const FRigidTransform3 ClusterWorldTM(ClusterParticles[SingleChassisIndex]->X(), ClusterParticles[SingleChassisIndex]->R());
	//const FRigidTransform3 ClusterWorldTM(ClusterParticles[SingleChassisIndex]->GetTransformXRCom());
	FRigidTransform3 Frame = FRigidTransform3::Identity;

	//UE_LOG(LogInit, Warning, TEXT("TransformIndex %d  vs  Particles.Num() %d  vs ClusterParticles.Num() %d"), TransformIndex, Particles.Num(), ClusterParticles.Num());

	if (TransformIndex < Particles.Num())
	{
		FPBDRigidParticleHandle* Child = Particles[TransformIndex];
		if (FPBDRigidClusteredParticleHandle* ClusterChild = Child->CastToClustered(); ClusterChild && ClusterChild->IsChildToParentLocked())
		{
			Frame = ClusterChild->ChildToParent();
		}
		else
		{
			//FTransform ChildT = Child->GetTransformXRCom();
			//const FRigidTransform3 ChildWorldTM(ChildT.GetLocation(), ChildT.GetRotation());
			const FRigidTransform3 ChildWorldTM(Child->X(), Child->R());
			Frame = ChildWorldTM.GetRelativeTransform(ClusterWorldTM);

			//UE_LOG(LogChaos, Warning, TEXT("ApplyForce Data TIdx %d %s"), TransformIndex, *Frame.GetTranslation().ToString());
		}
	}

	// local transform
	FVector Offset = OffsetTransform.InverseTransformVector(Frame.GetTranslation());
	TransformOut.SetTranslation(Offset);
	return ClusterParticles[SingleChassisIndex];
}

void FDeferredForcesModular::Apply(TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles
		, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles
		, const TManagedArray<FTransform>& Transforms
		, const TManagedArray<FTransform>& CollectionMassToLocal
		, const TManagedArray<int32>& Parent)
{
	if (!GCoreModularVehicleDebugParams.DisableForces)
	{
		FTransform RelativeTransform;
		for (const FApplyForceData& Data : ApplyForceDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(Particles, ClusterParticles, Data.TransformIndex, Transforms, CollectionMassToLocal, Parent, RelativeTransform);
			if (RigidHandle)
			{
				AddForce(RigidHandle, Data, RelativeTransform);
			}
		}

		for (const FApplyForceAtPositionData& Data : ApplyForceAtPositionDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(Particles, ClusterParticles, Data.TransformIndex, Transforms, CollectionMassToLocal, Parent, RelativeTransform);
			if (RigidHandle)
			{
				AddForceAtPosition(RigidHandle, Data, RelativeTransform);
			}
		}

		for (const FAddTorqueInRadiansData& Data : ApplyTorqueDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(Particles, ClusterParticles, Data.TransformIndex, Transforms, CollectionMassToLocal, Parent, RelativeTransform);
			if (RigidHandle)
			{
				AddTorque(RigidHandle, Data, RelativeTransform);
			}
		}
	}

	ApplyForceDatas.Empty();
	ApplyForceAtPositionDatas.Empty();
	ApplyTorqueDatas.Empty();
}



void FDeferredForcesModular::Apply(const FTransform& OffsetTransform
	, TArray<Chaos::FPBDRigidParticleHandle*>& Particles
	, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles)
{
	if (!GCoreModularVehicleDebugParams.DisableForces)
	{
		FTransform RelativeTransform;
		for (const FApplyForceData& Data : ApplyForceDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(OffsetTransform, Particles, ClusterParticles, Data.TransformIndex, RelativeTransform);
			if (RigidHandle)
			{
				AddForce(RigidHandle, Data, RelativeTransform);
			}
		}

		for (const FApplyForceAtPositionData& Data : ApplyForceAtPositionDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(OffsetTransform, Particles, ClusterParticles, Data.TransformIndex, RelativeTransform);
			if (RigidHandle)
			{
				AddForceAtPosition(RigidHandle, Data, RelativeTransform);
			}
		}

		for (const FAddTorqueInRadiansData& Data : ApplyTorqueDatas)
		{
			Chaos::FPBDRigidParticleHandle* RigidHandle = GetParticle(OffsetTransform, Particles, ClusterParticles, Data.TransformIndex, RelativeTransform);
			if (RigidHandle)
			{
				AddTorque(RigidHandle, Data, RelativeTransform);
			}
		}
	}

	ApplyForceDatas.Empty();
	ApplyForceAtPositionDatas.Empty();
	ApplyTorqueDatas.Empty();
}



void AddForce_Implementation(Chaos::FPBDRigidParticleHandle* RigidHandle, const FTransform& ParticleOffsetTransform, const FVector& LocalForce, const FTransform& OffsetTransform, bool bLevelSlope, const FColor& DebugColor)
{
	const Chaos::FVec3 WorldCOM = Chaos::FParticleUtilitiesGT::GetCoMWorldPosition(RigidHandle);

	// local to world
	const FTransform WorldTM(RigidHandle->R(), RigidHandle->X());
	Chaos::FVec3 Position = WorldTM.TransformPosition(ParticleOffsetTransform.TransformVector(OffsetTransform.GetLocation()));
	Chaos::FVec3 Force = WorldTM.TransformVector(ParticleOffsetTransform.TransformVector(OffsetTransform.TransformVector(LocalForce)));


	if (bLevelSlope && WorldTM.GetUnitAxis(EAxis::Z).Z > GCoreModularVehicleDebugParams.LevelSlopeThreshold)
	{
		static float ValueToUse = 0.5f;
		Force.X = ValueToUse;
		Force.Y = ValueToUse;
	}

	Chaos::FVec3 Arm = Position - WorldCOM;

	Chaos::FVec3 WorldTorque = Chaos::FVec3::CrossProduct(Arm, Force);
	auto* RP = RigidHandle->CastToRigidParticle();
	if (RP)
	{
		RP->AddForce(Force);
		RP->AddTorque(WorldTorque);

		// #TODO: remove HACK - doing this because vehicles are falling asleep and not waking up when forces are applied
	//	RP->SetSleepType(Chaos::ESleepType::NeverSleep);

#if CHAOS_DEBUG_DRAW
		if (GCoreModularVehicleDebugParams.ShowMass)
		{
			FString OutputText = FString::Format(TEXT("{0} {1}"), { RP->M(), RP->I().ToString() });
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugString(RigidHandle->X() + FVector(0,0,200), OutputText, nullptr, FColor::White, -1, false, -1.0f);
		}
#endif
	}

#if CHAOS_DEBUG_DRAW
	if (GCoreModularVehicleDebugParams.ShowForces)
	{
		// #TODO: perhaps render different force types/sources in different colors, i.e. aerofoil, suspension, friction, etc.
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Position, Position + Force * GCoreModularVehicleDebugParams.DrawForceScaling, DebugColor, false, -1.f, 0, 5.f);
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(Position, 5, 8, FColor::White, false, -1.f, 0, 5.f);
	}
#endif

}

void FDeferredForcesModular::AddForce(Chaos::FPBDRigidParticleHandle* RigidHandle, const FApplyForceData& DataIn, const FTransform& OffsetTransform)
{
	if (ensure(RigidHandle))
	{
		AddForce_Implementation(RigidHandle, ParticleOffsetTransform, DataIn.Force, OffsetTransform, (DataIn.Flags & EForceFlags::LevelSlope) == EForceFlags::LevelSlope, DataIn.DebugColor);
	}

}

//void FDeferredForcesModular::AddForce(Chaos::FPBDRigidClusteredParticleHandle* RigidHandle, const FApplyForceData& DataIn)
//{
//	check(false);
//	if (ensure(RigidHandle))
//	{
//		Chaos::EObjectStateType ObjectState = RigidHandle->ObjectState();
//		if (CHAOS_ENSURE(ObjectState == Chaos::EObjectStateType::Dynamic || ObjectState == Chaos::EObjectStateType::Sleeping))
//		{
//			if (DataIn.Flags == EForceFlags::AccelChange)
//			{
//				const float RigidMass = RigidHandle->M();
//				const Chaos::FVec3 Acceleration = DataIn.Force * RigidMass;
//				RigidHandle->AddForce(Acceleration, false);
//			}
//			else
//			{
//				RigidHandle->AddForce(DataIn.Force, false);
//			}
//
//		}
//	}
//}

void FDeferredForcesModular::AddForceAtPosition(Chaos::FPBDRigidParticleHandle* RigidHandle, const FApplyForceAtPositionData& DataIn, const FTransform& OffsetTransform)
{
	if (ensure(RigidHandle))
	{
		FTransform ShiftedTransform = OffsetTransform;
		ShiftedTransform.SetLocation(ShiftedTransform.GetLocation() + DataIn.Position);
		AddForce_Implementation(RigidHandle, ParticleOffsetTransform, DataIn.Force, ShiftedTransform, (DataIn.Flags & EForceFlags::LevelSlope) == EForceFlags::LevelSlope, DataIn.DebugColor);
	}
}

void FDeferredForcesModular::AddTorque(Chaos::FPBDRigidParticleHandle* RigidHandle, const FAddTorqueInRadiansData& DataIn, const FTransform& OffsetTransform)
{	
	check(false); //#TODO - incorporate OffsetTransform as this is currently wrong
	//if (ensure(RigidHandle))
	//{
	//	Chaos::EObjectStateType ObjectState = RigidHandle->ObjectState();
	//	if (CHAOS_ENSURE(ObjectState == Chaos::EObjectStateType::Dynamic || ObjectState == Chaos::EObjectStateType::Sleeping))
	//	{
	//		if (DataIn.Flags == EForceFlags::AccelChange)
	//		{
	//			RigidHandle->AddTorque(Chaos::FParticleUtilitiesXR::GetWorldInertia(RigidHandle) * DataIn.Torque, false);
	//		}
	//		else
	//		{
	//			RigidHandle->AddTorque(DataIn.Torque, false);
	//		}
	//	}
	//}
}
