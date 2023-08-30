// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuoyancySubsystem.h"
#include "BuoyancyAlgorithms.h"
#include "BuoyancyStats.h"
#include "BuoyancyRuntimeSettings.h"
#include "BuoyancyEventInterface.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsFiltering.h"
#include "Chaos/MidPhaseModification.h"
#include "Chaos/MassProperties.h"
#include "DrawDebugHelpers.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "WaterBodyActor.h"
#include "Components/SplineComponent.h"
#include "WaterSubsystem.h"
#include "WaterBodyManager.h"
#include "WaterSplineComponent.h"
#include "Chaos/PhysicsObject.h"
#include "PBDRigidsSolver.h"			// Only needed to get perparticlegravity :(
#include "Chaos/PBDRigidsEvolutionGBF.h"// Only needed to get perparticlegravity :(
#include "Chaos/PerParticleGravity.h"	// Needed in order to determine force of gravity on each particle
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/PhysicsObject.h"

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

void UBuoyancySubsystem::CreateSimCallback()
{
	// Create sim callback
	if (Chaos::FPhysicsSolver* Solver = GetSolver())
	{
		// Create the callback for keeping spline data in sync
		SplineData = Solver->CreateAndRegisterSimCallbackObject_External<FBuoyancyWaterSplineDataManager>();

		// Create the main buoyancy sim callback
		SimCallback = Solver->CreateAndRegisterSimCallbackObject_External<FBuoyancySubsystemSimCallback>();

		// Give the buoyancy sim callback a reference to the spline data callback,
		// so that it'll have access to per-particle spline data
		if (FBuoyancySubsystemSimCallbackInput* AsyncInput = SimCallback->GetProducerInputData_External())
		{
			AsyncInput->SplineData = SplineData;
		}
	}
}

void UBuoyancySubsystem::DestroySimCallback()
{
	if (SimCallback)
	{
		if (Chaos::FPhysicsSolverBase* Solver = SimCallback->GetSolver())
		{
			Solver->UnregisterAndFreeSimCallbackObject_External(SimCallback);
			SimCallback = nullptr;
		}
	}
}

void UBuoyancySubsystem::PostInitialize()
{
	Super::PostInitialize();

	// Apply initial runtime settings
	ApplyRuntimeSettings(GetDefault<UBuoyancyRuntimeSettings>(), EPropertyChangeType::ValueSet);

	// Setup callback for when waterbodies are added/removed
	if (FWaterBodyManager* WaterBodyManager = UWaterSubsystem::GetWaterBodyManager(GetWorld()))
	{
		WaterBodyManager->OnWaterBodyAdded.AddUObject(this, &UBuoyancySubsystem::OnWaterBodyAdded);
		WaterBodyManager->OnWaterBodyRemoved.AddUObject(this, &UBuoyancySubsystem::OnWaterBodyRemoved);
		bWaterObjectsChanged = true;
	}

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
	BuoyancySettings.WaterDensity = Chaos::GCm3ToKgCm3(InSettings->WaterDensity);
	BuoyancySettings.WaterDrag = InSettings->WaterDrag;
	BuoyancySettings.WaterCollisionChannel = InSettings->CollisionChannelForWaterObjects;
	BuoyancySettings.bKeepAwake = InSettings->bKeepFloatingObjectsAwake;
	BuoyancySettings.MaxNumBoundsSubdivisions = InSettings->MaxNumBoundsSubdivisions;
	BuoyancySettings.MinBoundsSubdivisionVol = InSettings->MinBoundsSubdivisionVol;
	BuoyancySettings.MinVelocityForSurfaceTouchCallback = InSettings->MinVelocityForSurfaceTouchCallback;

	// Based on server/client/editor, determine if we should generate callbacks.
	// If we're editor, always generate callbacks. If we're not editor, only
	// generate callbacks on client.	
#if WITH_EDITOR
	BuoyancySettings.SurfaceTouchCallbackFlags = InSettings->SurfaceTouchCallbackFlags;
#else
	UWorld* World = GetWorld();
	BuoyancySettings.SurfaceTouchCallbackFlags
		= (World && World->IsNetMode(NM_Client))
		? InSettings->SurfaceTouchCallbackFlags
		: EBuoyancyEventFlags::None;
#endif

	// Enable or disable
	SetEnabled(InSettings->bBuoyancyEnabled);
}

void UBuoyancySubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_Tick)

	//
	// The entire point of this tick is to send runtime data to
	// the physics thread which might have changed, and to process
	// outputs which may effect water bodies or result in
	// callbacks.
	//

	Super::Tick(DeltaTime);

	if (SimCallback == nullptr)
	{
		return;
	}

	if (SplineData == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (PhysScene == nullptr)
	{
		return;
	}

	// Update spline info for all water bodies & internal arrays of water objects
	if (bWaterObjectsChanged)
	{
		if (FWaterBodyManager* WaterBodyManager = UWaterSubsystem::GetWaterBodyManager(GetWorld()))
		{
			if (FBuoyancySubsystemSimCallbackInput* AsyncInput = SimCallback->GetProducerInputData_External())
			{
				bWaterObjectsChanged = false;
				AsyncInput->WaterObjects = TArray<Chaos::FPhysicsObjectHandle>();
				TArray<Chaos::FPhysicsObjectHandle>& WaterObjects = *AsyncInput->WaterObjects;
				WaterObjects.Reserve(WaterBodyManager->NumWaterBodies());
				WaterBodyManager->ForEachWaterBodyComponent(GetWorld(), [this, &WaterObjects](UWaterBodyComponent* WaterBodyComponent)
				{
					// Get the metadata object, if there is one
					UWaterSplineMetadata* WaterSplineMetadata = WaterBodyComponent->GetWaterSplineMetadata();

					if (UWaterSplineComponent* SplineComponent = WaterBodyComponent->GetWaterSpline())
					{
						// Copy out water spline data into a shared ptr, to be associated with all
						// child particles and marshaled to PT.
						const Chaos::FRigidTransform3 WaterTransform = WaterBodyComponent->GetComponentTransform();
						TSharedPtr<FBuoyancyWaterSplineData> WaterSplineData = MakeShared<FBuoyancyWaterSplineData>(
							WaterTransform,
							SplineComponent->SplineCurves.Position,
							WaterSplineMetadata ? WaterSplineMetadata->WaterVelocityScalar : TOptional<FInterpCurveFloat>()
						);

						// Go over each physics object in each primitive component which was generated
						// from this spline, and associate the spline with the particle.
						for (UPrimitiveComponent* WaterPrimitiveComponent : WaterBodyComponent->GetCollisionComponents(true))
						{
							for (Chaos::FPhysicsObjectHandle WaterObject : WaterPrimitiveComponent->GetAllPhysicsObjects())
							{
								SplineData->SetData_GT(WaterObject, WaterSplineData);
								WaterObjects.Add(WaterObject);
							}
						}
					}
					return true;
				});
			}
		}
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

	// Process surface-touched callbacks
	if (BuoyancySettings.SurfaceTouchCallbackFlags != 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_DispatchCallbacks)

		while (Chaos::TSimCallbackOutputHandle<FBuoyancySubsystemSimCallbackOutput> AsyncOutput = SimCallback->PopFutureOutputData_External())
		{
			for (const FBuoyancySubsystemSimCallbackOutput::FSurfaceTouch& SurfaceTouch : AsyncOutput->SurfaceTouches)
			{
				// Skip if we mask out this touch type
				if ((SurfaceTouch.Flag & BuoyancySettings.SurfaceTouchCallbackFlags) == 0)
				{
					continue;
				}

				// Extract primitive components
				UPrimitiveComponent* WaterComponent = PhysScene->GetOwningComponent<UPrimitiveComponent>(SurfaceTouch.WaterProxy);
				UPrimitiveComponent* RigidComponent = PhysScene->GetOwningComponent<UPrimitiveComponent>(SurfaceTouch.RigidProxy);

				// Get the parental water body component
				AWaterBody* WaterActor = WaterComponent->GetOwner<AWaterBody>();

				const auto DispatchEvent = [&](AActor* Actor)
				{
					// TODO: Actor relevancy check?

					// If the actor implements the event interface, call the surface touched callback
					if (Actor->Implements<UBuoyancyEventInterface>())
					{
						switch (SurfaceTouch.Flag)
						{
							case EBuoyancyEventFlags::Begin:
								IBuoyancyEventInterface::Execute_OnSurfaceTouchBegin(
									Actor, WaterActor, WaterComponent, RigidComponent,
									SurfaceTouch.Vol, SurfaceTouch.CoM, SurfaceTouch.Vel);
								break;

							case EBuoyancyEventFlags::Continue:
								IBuoyancyEventInterface::Execute_OnSurfaceTouching(
									Actor, WaterActor, WaterComponent, RigidComponent,
									SurfaceTouch.Vol, SurfaceTouch.CoM, SurfaceTouch.Vel);
								break;

							case EBuoyancyEventFlags::End:
								IBuoyancyEventInterface::Execute_OnSurfaceTouchEnd(
									Actor, WaterActor, WaterComponent, RigidComponent,
									SurfaceTouch.Vol, SurfaceTouch.CoM, SurfaceTouch.Vel);
								break;
						}
					}
				};
				DispatchEvent(WaterActor);
				DispatchEvent(RigidComponent->GetOwner());
			}
		}
	}
}

TStatId UBuoyancySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBuoyancySubsystem, STATGROUP_Tickables);
}

void UBuoyancySubsystem::OnWaterBodyAdded(UWaterBodyComponent* WaterBodyComponent)
{
	bWaterObjectsChanged = true;
}

void UBuoyancySubsystem::OnWaterBodyRemoved(UWaterBodyComponent* WaterBodyComponent)
{
	bWaterObjectsChanged = true;
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

void FBuoyancySubsystemSimCallbackOutput::Reset()
{
	SurfaceTouches.Reset();
}

void FBuoyancySubsystemSimCallback::OnPreSimulate_Internal()
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_OnPreSimulate)

	// If we were sent new buoyancy settings or data, update our local sim copy
	if (const FBuoyancySubsystemSimCallbackInput* Input = GetConsumerInput_Internal())
	{
		if (Input->WaterObjects.IsSet())
		{
			WaterObjects = *Input->WaterObjects;
		}

		if (Input->SplineData.IsSet())
		{
			SplineData = *Input->SplineData;
		}

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
}

void FBuoyancySubsystemSimCallback::OnMidPhaseModification_Internal(Chaos::FMidPhaseModifierAccessor& MidPhaseAccessor)
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_OnMidPhaseModification)

	// If we don't have a spline data manager, early out
	if (SplineData == nullptr)
	{
		return;
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
		// Why does cast-checked return a ref? That makes me think
		// it's not actually doing a check...
		FPBDRigidsSolver& PBDSolver = SolverBase->CastChecked();
		Evolution = PBDSolver.GetEvolution();
	}
	if (Evolution == nullptr)
	{
		return;
	}

	// SparseArray implements move semantics, so these swaps should amount to pointer swaps.
	// This way array memories stick around even when reset/swapped so we don't do many
	// new allocations.
	Swap(Submersions, PrevSubmersions);
	Swap(SubmersionMetaData, PrevSubmersionMetaData);

	// Clear submersions and submerged shapes array, but keep their memory allocated
	Submersions.Reset();
	SubmergedShapes.Reset();
	SubmersionMetaData.Reset();

	// Build list of "submersions"
	ProcessMidPhases(*Evolution, MidPhaseAccessor);

	// Apply buoyant forces resulting from submersions
	ApplyBuoyantForces(*Evolution);

	// Generate async outputs for callback data
	GenerateCallbackData();
}

void FBuoyancySubsystemSimCallback::ProcessMidPhases(
	Chaos::FPBDRigidsEvolution& Evolution,
	Chaos::FMidPhaseModifierAccessor& MidPhaseAccessor)
{
	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_VisitMidphases)

	// Loop over water objects
	Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
	for (Chaos::FPhysicsObjectHandle WaterObject : WaterObjects)
	{
		// Get particle handle from physics object
		Chaos::FGeometryParticleHandle* WaterParticle = Interface.GetParticle(WaterObject);
		if (WaterParticle == nullptr) { continue; }

		// Get midphases for this waterobject
		for (Chaos::FMidPhaseModifier& MidPhase : MidPhaseAccessor.GetMidPhases(WaterParticle))
		{
			ProcessMidPhase(Evolution, WaterParticle, MidPhase);
		}
	}
}

void FBuoyancySubsystemSimCallback::ProcessMidPhase(
	Chaos::FPBDRigidsEvolution& Evolution,
	Chaos::FGeometryParticleHandle* WaterParticle,
	Chaos::FMidPhaseModifier& MidPhase)
{
	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_ProcessMidphase)

	using namespace Chaos;

	// Get midphase particles
	FGeometryParticleHandle* Particle0;
	FGeometryParticleHandle* Particle1;
	MidPhase.GetParticles(&Particle0, &Particle1);
	if (Particle0 == nullptr || Particle1 == nullptr)
	{
		return;
	}

	// Get water spline data
	const TSharedPtr<FBuoyancyWaterSplineData>* SplineDataPtr = SplineData->GetData_PT(*WaterParticle);
	const FBuoyancyWaterSplineData* WaterSpline = (SplineDataPtr ? (*SplineDataPtr).Get() : nullptr);
	if (WaterSpline == nullptr)
	{
		return;
	}

	// Select & cast the rigid particle
	FPBDRigidParticleHandle* RigidParticle = (Particle0 == WaterParticle ? Particle1 : Particle0)->CastToRigidParticle();

	// Find water surface at the nearest point on the spline
	const FVector ParticlePos = RigidParticle->XCom();
	const FVector ParticleLocalPos = WaterSpline->Transform.InverseTransformPosition(ParticlePos);
	float ParticleDistance;
	const float ClosestSplineKey = WaterSpline->Position.FindNearest(ParticleLocalPos, ParticleDistance);
	const FVector ClosestSplinePoint = WaterSpline->Transform.TransformPosition(WaterSpline->Position.Eval(ClosestSplineKey));
	const float WaterZ = ClosestSplinePoint.Z;

	// Get the water velocity at this point
	const FVec3 WaterVel
		= WaterSpline->Velocity.IsSet()
		? WaterSpline->Velocity->Eval(ClosestSplineKey) * WaterSpline->Position.EvalDerivative(ClosestSplineKey).GetSafeNormal()
		: FVec3::ZeroVector;

#if ENABLE_DRAW_DEBUG
	if (bBuoyancyDebugDraw)
	{
		// Spline Color
		const FColor SplineColor = FColor::Cyan;

		// Draw projection onto the line
		const FVec3 SurfacePoint(ParticlePos.X, ParticlePos.Y, WaterZ);
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(ParticlePos, SurfacePoint, SplineColor, false, -1.f, -1, 6.f);
		Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(SurfacePoint, ClosestSplinePoint, SplineColor, false, -1.f, -1, 3.f);

		// Draw a section of the spline near the spline key
		FVec3 PrevPoint;
		bool bFirst = true;
		for (float SplineKey = ClosestSplineKey - .1f; SplineKey <= ClosestSplineKey + .1f; SplineKey += .05f)
		{
			const FVector SplinePoint = WaterSpline->Transform.TransformPosition(WaterSpline->Position.Eval(SplineKey));
			if (bFirst)
			{
				bFirst = false;
				PrevPoint = SplinePoint;
			}
			else
			{
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(PrevPoint, SplinePoint, 15.f, SplineColor, false, -1.f, -1, 3.f);
			}
		}

		Chaos::FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(SurfacePoint, SurfacePoint + WaterVel, 20.f, FColor::Yellow, false, -1.f, -1, 3.f);
	}
#endif

	// Compute submerged volume and CoM
	float SubmergedVol;
	FVec3 SubmergedCoM;
	float TotalVol;
	if (BuoyancyAlgorithms::ComputeSubmergedVolume(Evolution, RigidParticle, WaterParticle, WaterZ, BuoyancySettings->MaxNumBoundsSubdivisions, BuoyancySettings->MinBoundsSubdivisionVol, SubmergedShapes, SubmergedVol, SubmergedCoM, TotalVol))
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_BuildSubmersions)

		// If any volume was submerged, take the weighted average of the centers of mass to get the
		// approximate submerged center of mass, and apply the buoyancy force there.
		if (SubmergedVol > SMALL_NUMBER)
		{
			const int32 RigidParticleIndex = RigidParticle->UniqueIdx().Idx;

			// If this particle was already marked submerged, add to its existing submersion
			if (Submersions.IsValidIndex(RigidParticleIndex))
			{
				FBuoyancySubmersion& Submersion = Submersions[RigidParticleIndex];
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
				Submersions.Insert(RigidParticleIndex, { RigidParticle, SubmergedVol, SubmergedCoM, WaterVel });
			}

			// If this is a surface touch record it for callback, if 
			if (BuoyancySettings->SurfaceTouchCallbackFlags != 0 &&
				TotalVol > SubmergedVol * (1.f + UE_KINDA_SMALL_NUMBER))
			{
				SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_BuildSubmersionCallbackData)

				// Proceed only if this CoM is moving fast enough to generate events
				const FBuoyancySubmersion& Submersion = Submersions[RigidParticleIndex];
				const FVec3 CoMDiff = Submersion.CoM - RigidParticle->XCom();
				const FVec3 CoMVel = RigidParticle->V() + FVec3::CrossProduct(RigidParticle->W(), CoMDiff);
				const float CoMVelSq = FVec3::DotProduct(CoMVel, CoMVel);
				const float MinVel = BuoyancySettings->MinVelocityForSurfaceTouchCallback;
				const float MinVelSq = MinVel * MinVel;

				if (CoMVelSq > MinVelSq)
				{
					// If we don't have a metadata for this particle yet, add one
					if (!SubmersionMetaData.IsValidIndex(RigidParticleIndex))
					{
						SubmersionMetaData.Insert(RigidParticleIndex, FBuoyancySubmersionMetaData());
					}
					FBuoyancySubmersionMetaData& MetaData = SubmersionMetaData[RigidParticleIndex];

					// If we haven't already maxed out on water contacts, add one
					if (MetaData.WaterContacts.Num() < FBuoyancySubmersionMetaData::MaxNumWaterContacts)
					{
						MetaData.WaterContacts.Add({ WaterParticle, SubmergedVol, SubmergedCoM, CoMVel });
					}
				}
			}
		}
	}
}

void FBuoyancySubsystemSimCallback::ApplyBuoyantForces(Chaos::FPBDRigidsEvolution& Evolution)
{
	SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_ApplyBuoyantForces)

	using namespace Chaos;

	// How much time has the sim ticked this frame
	const FReal DeltaSeconds = GetDeltaTime_Internal();

	// Get perparticle gravity rule, for figuring out the effective gravity on buoyant objects
	const FPerParticleGravity* PerParticleGravity = &Evolution.GetGravityForces();

	// Apply all buoyant forces
	for (const FBuoyancySubmersion& Submersion : Submersions)
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
		if (BuoyancyAlgorithms::ComputeBuoyantForce(Submersion.Particle, DeltaSeconds, BuoyancySettings->WaterDensity, BuoyancySettings->WaterDrag, GravityAccel, Submersion.CoM, Submersion.Vol, Submersion.Vel, DeltaV, DeltaW))
		{
			// Clamp delta velocities
			DeltaV = DeltaV.GetClampedToSize(0.f, BuoyancySettings->MaxDeltaV);
			DeltaW = DeltaW.GetClampedToSize(0.f, BuoyancySettings->MaxDeltaW);

			// Apply the deltas
			Submersion.Particle->SetV(Submersion.Particle->V() + DeltaV);
			Submersion.Particle->SetW(Submersion.Particle->W() + DeltaW);

			// Wake up the body??
			if (BuoyancySettings->bKeepAwake)
			{
				Evolution.SetParticleObjectState(Submersion.Particle, EObjectStateType::Dynamic);
			}
		}
	}
}

void FBuoyancySubsystemSimCallback::GenerateCallbackData()
{
	// Generate callback data if we're into that sort of thing
	const uint8 CallbackFlags = BuoyancySettings->SurfaceTouchCallbackFlags;
	if (CallbackFlags != 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancySubsystem_ProduceSurfaceTouches)

		// Get the async output struct to write to
		FBuoyancySubsystemSimCallbackOutput& Output = GetProducerOutputData_Internal();

		// Process every surface touch and queue up some of them to return
		// to game thread for callback dispatch
		Output.SurfaceTouches.Reserve(SubmersionMetaData.Num() * FBuoyancySubmersionMetaData::MaxNumWaterContacts);
		for (auto Iter = SubmersionMetaData.CreateIterator(); Iter; ++Iter)
		{
			const FBuoyancySubmersionMetaData& MetaData = *Iter;
			const int32 ObjectIndex = Iter.GetIndex();
			const FBuoyancySubmersion& Submersion = Submersions[ObjectIndex];

			// Mark this as a new or continuing contact based on whether or
			// not we have a bit from the previous-submersions array.
			const bool bPrevSubmerged =
				PrevSubmersionMetaData.IsValidIndex(ObjectIndex) &&
				PrevSubmersionMetaData.IsAllocated(ObjectIndex);
			const EBuoyancyEventFlags TouchFlag
				= bPrevSubmerged
				? EBuoyancyEventFlags::Continue
				: EBuoyancyEventFlags::Begin;

			// Clear out the "prev" entry for this one's metadata so that
			// we can loop over the prev metadata for lost-contacts. Only
			// bother doing this work if we're tracking removals
			if (bPrevSubmerged && (CallbackFlags & EBuoyancyEventFlags::End) != 0)
			{
				PrevSubmersionMetaData.RemoveAt(ObjectIndex);
			}

			// Only continue if we're tracking this touch type
			if ((TouchFlag & CallbackFlags) == 0)
			{
				continue;
			}

			// Build up output of new and continuing surface touches
			for (const FBuoyancySubmersionMetaData::FWaterContact& WaterContact : MetaData.WaterContacts)
			{
				Output.SurfaceTouches.Add({
					TouchFlag,
					Submersion.Particle->PhysicsProxy(),
					WaterContact.Water->PhysicsProxy(),
					WaterContact.Vol,
					WaterContact.CoM,
					WaterContact.Vel
				});
			}
		}

		// The remaining previous submersion metadata will correspond with lost contacts
		//
		// NOTE:
		// At the moment, lost contact callbacks will only occur when an entire object
		// loses contact, not just when one part of it loses contact.
		if ((CallbackFlags & EBuoyancyEventFlags::End) != 0)
		{
			for (auto Iter = PrevSubmersionMetaData.CreateIterator(); Iter; ++Iter)
			{
				const FBuoyancySubmersionMetaData& MetaData = *Iter;
				const int32 ObjectIndex = Iter.GetIndex();
				const FBuoyancySubmersion& Submersion = PrevSubmersions[ObjectIndex];
				for (const FBuoyancySubmersionMetaData::FWaterContact& WaterContact : MetaData.WaterContacts)
				{
					Output.SurfaceTouches.Add({
						EBuoyancyEventFlags::End,
						Submersion.Particle->PhysicsProxy(),
						WaterContact.Water->PhysicsProxy(),
						WaterContact.Vol,
						WaterContact.CoM,
						WaterContact.Vel
					});
				}
			}
		}
	}
}
