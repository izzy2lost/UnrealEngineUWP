// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuoyancySubsystem.h"
#include "BuoyancyAlgorithms.h"
#include "BuoyancyStats.h"
#include "BuoyancyRuntimeSettings.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsFiltering.h"
#include "Chaos/MidPhaseModification.h"
#include "Math/UnitConversion.h"
#include "PBDRigidsSolver.h"			// Only needed to get perparticlegravity :(
#include "Chaos/PBDRigidsEvolutionGBF.h"// Only needed to get perparticlegravity :(
#include "Chaos/PerParticleGravity.h"	// Needed in order to determine force of gravity on each particle

//
// CVars
//

bool bBuoyancyDebugDraw = false;
FAutoConsoleVariableRef CVarBuoyancyDebugDraw(TEXT("p.Buoyancy.DebugDraw"), bBuoyancyDebugDraw, TEXT(""));


//
// Logging
//

DEFINE_LOG_CATEGORY(LogBuoyancySubsystem);


//
// Buoyancy Subsystem
//

bool UBuoyancySubsystem::SetEnabled(const bool bEnabled)
{
	if (bEnabled != IsEnabled())
	{
		if (bEnabled)
		{
			CreateSimCallback();
		}
		else
		{
			DestroySimCallback();
		}

		return IsEnabled() == bEnabled;
	}

	// Already had whatever setting
	return true;
}

bool UBuoyancySubsystem::IsEnabled() const
{
	return SimCallback != nullptr;
}

bool UBuoyancySubsystem::CreateSimCallback()
{
	// Create sim callback
	if (Chaos::FPhysicsSolver* Solver = GetSolver())
	{
		SimCallback = Solver->CreateAndRegisterSimCallbackObject_External<FBuoyancySubsystemSimCallback>();

		return SimCallback != nullptr;
	}

	return false;
}

bool UBuoyancySubsystem::DestroySimCallback()
{
	if (SimCallback)
	{
		if (Chaos::FPhysicsSolverBase* Solver = SimCallback->GetSolver())
		{
			Solver->UnregisterAndFreeSimCallbackObject_External(SimCallback);
			SimCallback = nullptr;
			return true;
		}
	}

	return false;
}

void UBuoyancySubsystem::PostInitialize()
{
	Super::PostInitialize();

	CreateSimCallback();

	// Apply initial runtime settings
	ApplyRuntimeSettings(GetDefault<UBuoyancyRuntimeSettings>(), EPropertyChangeType::ValueSet);

	// Set up callback for when runtime settings change in editor
#if WITH_EDITOR
	GetDefault<UBuoyancyRuntimeSettings>()->OnSettingsChange.AddUObject(this, &UBuoyancySubsystem::ApplyRuntimeSettings);
#endif //WITH_EDITOR
}

void UBuoyancySubsystem::Deinitialize()
{
	DestroySimCallback();

	Super::Deinitialize();
}

void UBuoyancySubsystem::ApplyRuntimeSettings(const UBuoyancyRuntimeSettings* InSettings, EPropertyChangeType::Type ChangeType)
{
	bBuoyancySettingsChanged = true;

	// Runtime settings presents water density in g/cm^3, but we want it in kg/cm^3
	// so introduce a factor of 10^-3 here.
	//BuoyancySettings.WaterDensity = FNumericUnit<EUnit::KilogramsPerCubicCentimeter>::ConvertTo(InSettings->WaterDensity);
	BuoyancySettings.WaterDensity = FUnitConversion::Convert(InSettings->WaterDensity,
		EUnit::GramsPerCubicCentimeter,
		EUnit::KilogramsPerCubicCentimeter);
	BuoyancySettings.WaterCollisionChannel = InSettings->CollisionChannelForWaterObjects;
	BuoyancySettings.bKeepAwake = InSettings->bKeepFloatingObjectsAwake;
	BuoyancySettings.MaxNumBoundsSubdivisions = InSettings->MaxNumBoundsSubdivisions;
	BuoyancySettings.MinBoundsSubdivisionVol = InSettings->MinBoundsSubdivisionVol;

	// Enable or disable
	SetEnabled(InSettings->bBuoyancyEnabled);
}

void UBuoyancySubsystem::Tick(float DeltaTime)
{
	//
	// NOTE: The entire point of this tick is to send runtime data to
	// the physics thread which might have changed. At the initial
	// time of this writing, the only piece of data is the water-collision-
	// channel, which will never change once set in almost every case.
	// 
	// Therefore we can probably do away with this tick, and move the
	// GetProducerInputData_External to PostInitialize and also
	// ApplyRuntimeSettings for editor builds, and do away with the Tick
	// entirely.
	// 
	// However, I'm not confident that that would work due to timing
	// weirdness of the sim callback system, and also 
	//

	Super::Tick(DeltaTime);

	if (SimCallback == nullptr)
	{
		return;
	}

	// Only bother sending new async inputs if our buoyancy settings actually changed
	if (bBuoyancySettingsChanged)
	{
		if (FBuoyancySubsystemSimCallbackInput* AsyncInput = SimCallback->GetProducerInputData_External())
		{
			bBuoyancySettingsChanged = false;
			AsyncInput->BuoyancySettings = MakeUnique<FBuoyancySettings>(BuoyancySettings);
		}
	}
}

TStatId UBuoyancySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBuoyancySubsystem, STATGROUP_Tickables);
}

Chaos::FPhysicsSolver* UBuoyancySubsystem::GetSolver() const
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				return Solver;
			}
		}
	}

	return nullptr;
}


//
// Buoyancy Sim Callback
//

void FBuoyancySubsystemSimCallbackInput::Reset()
{
	BuoyancySettings.Reset();
}

namespace
{
	using namespace Chaos;

	// Check to see if any of this particle's shapes has a particular collision channel
	const bool ParticleHasCollision(const FGeometryParticleHandle& Particle, const ECollisionChannel CollisionChannel)
	{
		const auto& Shapes = Particle.ShapesArray();
		uint32 Word3 = 0;
		for (const auto& Shape : Shapes)
		{
			const FCollisionFilterData& ShapeQueryData = Shape->GetQueryData();
			Word3 |= ShapeQueryData.Word3;
		}
		return GetCollisionChannel(Word3) == CollisionChannel;
	};
}

void FBuoyancySubsystemSimCallback::OnPreSimulate_Internal()
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_OnPreSimulate)

	// If we were sent new buoyancy settings or data, update our local sim copy
	if (const FBuoyancySubsystemSimCallbackInput* Input = GetConsumerInput_Internal())
	{
		if (Input->BuoyancySettings.IsValid())
		{
			BuoyancySettings = MoveTemp(Input->BuoyancySettings);
		}
	}

	// If we don't have a valid buoyancy settings object, don't continue
	if (BuoyancySettings.IsValid() == false)
	{
		return;
	}

	// Get the evolution
	FPBDRigidsEvolution* Evolution = nullptr;
	if (FPhysicsSolverBase* SolverBase = GetSolver())
	{
		// Why does castchecked return a ref? That makes me think
		// it's not actually doing a check...
		FPBDRigidsSolver& PBDSolver = SolverBase->CastChecked();
		Evolution = PBDSolver.GetEvolution();
	}
	if (Evolution == nullptr)
	{
		return;
	}

	// Get perparticle gravity rule, for figuring out the effective gravity on buoyant objects
	const FPerParticleGravity* PerParticleGravity = &Evolution->GetGravityForces();
	if (PerParticleGravity == nullptr)
	{
		return;
	}

	// How much time has the sim ticked this frame
	const FReal DeltaSeconds = GetDeltaTime_Internal();

	// Apply all buoyant forces
	for (const FSubmersion& Submersion : Submersions)
	{
		// Figure out the gravity level of the particle
		const int32 GravityGroupIndex = Submersion.Particle->GravityGroupIndex();
		const FVec3 GravityAccel
			= PerParticleGravity != nullptr && GravityGroupIndex != INDEX_NONE
			? (FVec3)PerParticleGravity->GetAcceleration(GravityGroupIndex)
			: FVec3::DownVector * 980.f; // Default to "regular" gravity

		// Compute delta linear and angular velocities due to buoyancy. If they're big enough to
		// matter, apply them
		FVec3 DeltaV, DeltaW;
		if (BuoyancyAlgorithms::ComputeBuoyantForce(Submersion.Particle, DeltaSeconds, BuoyancySettings->WaterDensity, BuoyancySettings->WaterDrag, GravityAccel, Submersion.CoM, Submersion.Vol, DeltaV, DeltaW))
		{
			// Clamp delta velocities
			DeltaV = DeltaV.GetClampedToSize(0.f, BuoyancySettings->MaxDeltaV);
			DeltaW = DeltaW.GetClampedToSize(0.f, BuoyancySettings->MaxDeltaW);

			// Apply the deltas
			Submersion.Particle->SetV(Submersion.Particle->V() + DeltaV);
			Submersion.Particle->SetW(Submersion.Particle->W() + DeltaW);

			// Wake up the body??
			if (Evolution && BuoyancySettings->bKeepAwake)
			{
				Evolution->SetParticleObjectState(Submersion.Particle, EObjectStateType::Dynamic);
			}
		}

	}

	// Clear submersions, but keep memory allocated
	Submersions.Reset();
}

void FBuoyancySubsystemSimCallback::OnMidPhaseModification_Internal(Chaos::FMidPhaseModifierAccessor& MidPhaseAccessor)
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_OnMidPhaseModification)

	// If we don't have a valid buoyancy settings object, don't continue
	if (BuoyancySettings.IsValid() == false)
	{
		return;
	}

	// This 2d bit array is used to avoid double-counting buoyancy for any particular
	// shape on a body. The outer TSparseArray is indexed by the unique idx on a particle,
	// the inner bitarray is indexed by it's shape indices.
	//
	// If we have computed a buoyant force for a particular shape on an object already
	// then skip it on the second time around.
	TSparseArray< TBitArray<> > SubmergedShapes;

	// NOTE: For now we visit _every_ midphase, and check for ones which involve
	// our target collision channel. It would be nice if it were possible instead
	// to get a list of water body particles and loop over only midphases which
	// involve them.
	MidPhaseAccessor.VisitMidPhases([this, &SubmergedShapes](Chaos::FMidPhaseModifier& MidPhase)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_VisitMidphases)

		FGeometryParticleHandle* Particle0;
		FGeometryParticleHandle* Particle1;
		MidPhase.GetParticles(&Particle0, &Particle1);
		if (Particle0 == nullptr || Particle1 == nullptr)
		{
			return;
		}

		// Use collision filters to pick out which of the particles is water, if either.
		//
		// TODO: Instead of this, water particles could have additional userdata, or could
		// register themselves with the callback and we could look for only the midphases
		// generated with them.
		const bool bWater0 = ParticleHasCollision(*Particle0, BuoyancySettings->WaterCollisionChannel);
		const bool bWater1 = ParticleHasCollision(*Particle1, BuoyancySettings->WaterCollisionChannel);
		if ((bWater0 || bWater1) == false)
		{
			return;
		}

		// We want to disable all collisions with water, so disable the midphase now
		MidPhase.Disable();

		// Store the rigid particle and the water particle
		if (bWater0) { Swap(Particle0, Particle1); }
		FPBDRigidParticleHandle* RigidParticle = Particle0->CastToRigidParticle();
		FGeometryParticleHandle* WaterParticle = Particle1;
		if (RigidParticle == nullptr)
		{
			return;
		}

		// If the particle is in a cluster, get the actual root particle that we can apply forces to
		FPBDRigidClusteredParticleHandle* RigidCluster = RigidParticle->CastToClustered();
		if (RigidCluster)
		{
			while (FPBDRigidClusteredParticleHandle* ParentCluster = RigidCluster->Parent())
			{
				RigidCluster = ParentCluster;
			}
		}
		FPBDRigidParticleHandle* RigidRoot = RigidCluster ? RigidCluster->CastToRigidParticle() : RigidParticle;

		// Compute submerged volume and CoM
		float SubmergedVol;
		FVec3 SubmergedCoM;
		if (BuoyancyAlgorithms::ComputeSubmergedVolume(WaterParticle, RigidParticle, BuoyancySettings->MaxNumBoundsSubdivisions, BuoyancySettings->MinBoundsSubdivisionVol, SubmergedShapes, SubmergedVol, SubmergedCoM))
		{
			SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_ComputeBuoyantForces)

			//
			// If any volume was submerged, take the weighted average of the centers of mass to get the
			// approximate submerged center of mass, and apply the buoyancy force there.
			if (SubmergedVol > SMALL_NUMBER)
			{
				const int32 RigidParticleIndex = RigidParticle->UniqueIdx().Idx;

				// If this particle was already marked submerged, add to its existing submersion
				if (Submersions.IsValidIndex(RigidParticleIndex))
				{
					FSubmersion& Submersion = Submersions[RigidParticleIndex];
					ensureMsgf(Submersion.Particle == RigidParticle, TEXT("Something went wrong - there's a particle index mismatch in the Submersions sparse array"));

					// Sum the volumes
					Submersion.Vol = Submersion.Vol + SubmergedVol;

					// Get the weighted-average CoM
					// NOTE: The unchecked division should be safe since we already
					// know SubmergedVol > SMALL_NUMBER
					Submersion.CoM = ((Submersion.CoM * Submersion.Vol) + (SubmergedCoM * SubmergedVol)) / Submersion.Vol;
				}

				// If this particle was not yet submerged, make a new submersion for it
				else
				{
					Submersions.Insert(RigidParticleIndex, { RigidParticle, SubmergedVol, SubmergedCoM });
				}
			}
		}
	});
}
