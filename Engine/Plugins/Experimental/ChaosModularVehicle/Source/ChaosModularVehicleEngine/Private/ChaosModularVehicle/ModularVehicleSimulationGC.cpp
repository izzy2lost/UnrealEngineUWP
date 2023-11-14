// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleSimulationGC.h"
#include "ChaosModularVehicle/ModularVehicleDefaultAsyncInput.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Chaos/DebugDrawQueue.h"
#include "Engine/World.h"


void FModularVehicleSimulationGC::Initialize(TUniquePtr<Chaos::FSimModuleTree>& InSimModuleTree)
{
	SimModuleTree = MoveTemp(InSimModuleTree);
	InputInterpolation.Init(FModularVehicleInputRate(), EModularVehicleInputType::Max);

	if (bUsingNetworkPhysicsPrediction)
	{
		const int32 PhysicsHistoryLength = UPhysicsSettings::Get()->GetPhysicsHistoryCount();
		VehicleHistoryBuffer.SetNum(NumHistoryFrames);
	}

}

void FModularVehicleSimulationGC::Terminate()
{
	SimModuleTree.Reset(nullptr);
}


void FModularVehicleSimulationGC::SyncHistoryInputs(float DeltaSeconds, FGeometryCollectionPhysicsProxy* Proxy)
{
	check(bUsingNetworkPhysicsPrediction);
	check(SimModuleTree.IsValid());

	Chaos::FControlInputs& ExternalInputs = SimModuleTree->GetControlInputs();

	auto* RigidSolver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>();
	if (RigidSolver)
	{
		const int32 HistoryFrame = RigidSolver->GetCurrentFrame();
		int32 EndFrame = StartFrame + NumHistoryFrames;

		// if rewind-resim then use the values from the history buffer
		if (RigidSolver->GetEvolution()->IsResimming())
		{
			if (HistoryFrame >= StartFrame && HistoryFrame < EndFrame)
			{
				const int32 LocalFrame = HistoryFrame % NumHistoryFrames;
				if (NumHistoryFrames > LocalFrame)
				{
					// interpolation value for this frame is already included in history
					SimInputData.ControlInputs = VehicleHistoryBuffer[LocalFrame].ControlInputs;
				}

			}
		}
		else
		{
			// interpolate control on the PT for determinism
			// TODO: pass the initial InputRate values over from the GT
			Chaos::FControlInputs& InterpolatedInputs = SimInputData.ControlInputs;
			InterpolateInputs(DeltaSeconds, ExternalInputs, InterpolatedInputs);

			if (HistoryFrame >= EndFrame)
			{
				StartFrame += HistoryFrame - EndFrame + 1;
			}
			EndFrame = StartFrame + NumHistoryFrames;

			if (HistoryFrame >= StartFrame && HistoryFrame < EndFrame)
			{
				const int32 LocalFrame = HistoryFrame % NumHistoryFrames;
				if (NumHistoryFrames > LocalFrame)
				{
					// store the interpolated value - we won't interpolate again when using the history
					VehicleHistoryBuffer[LocalFrame].ControlInputs = SimInputData.ControlInputs;
				}
			}
		}
	}
}

/** This function will be called in parallel with other vehicle instances
  *  - for this reason care must be taken, i.e. can't apply forces directly they must be accumulated/deferred
  * then applied after the parallel update
  */
void FModularVehicleSimulationGC::Simulate(UWorld* InWorld, float DeltaSeconds, const FModularVehicleDefaultAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, FGeometryCollectionPhysicsProxy* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// for debugging to determine if this is the server or client
	//ENetMode NetMode = InWorld->GetNetMode();
	//UE_LOG(LogModularVehicle, Log, TEXT("FModularVehicleSimulationGC::Simulate: NetMode %d"), NetMode);

	if (Proxy && SimModuleTree.IsValid())
	{
		if (bUsingNetworkPhysicsPrediction)
		{ 
			SyncHistoryInputs(DeltaSeconds, Proxy);
		}
		else
		{
			Chaos::FControlInputs& InterpolatedInputs = SimInputData.ControlInputs;
			Chaos::FControlInputs& ExternalInputs = SimModuleTree->GetControlInputs();
			InterpolateInputs(DeltaSeconds, ExternalInputs, InterpolatedInputs);
		}

		// Processing that currently doesn't quite fit inside the sim tree simulate call
		PerformAdditionalSimWork(InWorld, InputData, Proxy, SimInputData);
		
		int SolverFrame = Proxy->GetSolver<Chaos::FPhysicsSolver>()->GetCurrentFrame();
		//UE_LOG(LogModularVehicle, Log, TEXT(" ~~~ NetMode %d Frame %d ControlsFrame %d, CheckSum %d, Steering %f")
		//	, InWorld->GetNetMode()
		//	, SolverFrame
		//	, SimInputData.ControlInputs.InputDebugIndex
		//	, SimInputData.ControlInputs.GetChecksum()
		//	, SimInputData.ControlInputs.Steering);
		
		// run the dynamics simulation, engine, suspension, wheels, aerofoils etc.
 		SimModuleTree->Simulate(DeltaSeconds, SimInputData, Proxy);
	}

}

void FModularVehicleSimulationGC::InterpolateInputs(float DeltaSeconds, const Chaos::FControlInputs& ExternalInputIn, Chaos::FControlInputs& InterpolatedInputsInOut)
{
	InterpolatedInputsInOut.Steering = InputInterpolation[EModularVehicleInputType::Steering].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Steering, ExternalInputIn.Steering);
	InterpolatedInputsInOut.Throttle = InputInterpolation[EModularVehicleInputType::Throttle].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Throttle, ExternalInputIn.Throttle);
	InterpolatedInputsInOut.Brake = InputInterpolation[EModularVehicleInputType::Brake].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Brake, ExternalInputIn.Brake);
	InterpolatedInputsInOut.Handbrake = InputInterpolation[EModularVehicleInputType::Handbrake].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Handbrake, ExternalInputIn.Handbrake);
	InterpolatedInputsInOut.Pitch = InputInterpolation[EModularVehicleInputType::Pitch].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Pitch, ExternalInputIn.Pitch);
	InterpolatedInputsInOut.Roll = InputInterpolation[EModularVehicleInputType::Roll].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Roll, ExternalInputIn.Roll);
	InterpolatedInputsInOut.Yaw = InputInterpolation[EModularVehicleInputType::Yaw].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Yaw, ExternalInputIn.Yaw);
	InterpolatedInputsInOut.GearNumber = ExternalInputIn.GearNumber;
	InterpolatedInputsInOut.InputDebugIndex = ExternalInputIn.InputDebugIndex;
}

void FModularVehicleSimulationGC::PerformAdditionalSimWork(UWorld* InWorld, const FModularVehicleDefaultAsyncInput& InputData, FGeometryCollectionPhysicsProxy* Proxy, Chaos::FAllInputs& AllInputs)
{
	check(Proxy);
	Chaos::EnsureIsInPhysicsThreadContext();

	FGeometryDynamicCollection& GeometryCollection = Proxy->GetPhysicsCollection();
	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = Proxy->GetSolverClusterHandles();
	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles = Proxy->GetSolverParticleHandles();

	if (Clusters.Num() > 0)
	{
		const TArray<Chaos::FSimModuleTree::FSimModuleNode>& ModuleArray = SimModuleTree->GetSimulationModuleTree();

		for (const Chaos::FSimModuleTree::FSimModuleNode& Node : ModuleArray)
		{
			if (!Node.SimModule->IsEnabled() || !Particles[Node.SimModule->GetTransformIndex()]->Disabled())
			{
				Node.SimModule->SetStateFlags(Chaos::eSimModuleState::Disabled);
				continue;
			}

			check(Node.SimModule->GetTransformIndex() < Clusters.Num());
			const Chaos::FPBDRigidClusteredParticleHandle* Particle = Clusters[Node.SimModule->GetTransformIndex()];
			if (Particle)
			{
				const FTransform BodyTransform(Particle->R(), Particle->X());

				// #TODO: cheating just now to get it working - do we pass in one or one per particle, as it will be different if it has split into fragments
				AllInputs.VehicleWorldTransform = BodyTransform;

				if (Node.SimModule->IsBehaviourType(Chaos::eSimModuleTypeFlags::Raycast))
				{
					Chaos::FSpringTrace OutTrace;
					Chaos::FSuspensionSimModule* Suspension = static_cast<Chaos::FSuspensionSimModule*>(Node.SimModule);

					// would be cleaner an faster to just store radius in suspension also
					float WheelRadius = 0;
					if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
					{
						Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(ModuleArray[Suspension->GetWheelSimTreeIndex()].SimModule);
						if (Wheel)
						{
							WheelRadius = Wheel->Setup().Radius;
						}
					}

					Suspension->GetWorldRaycastLocation(BodyTransform, WheelRadius, OutTrace);

					FVector TraceStart = OutTrace.Start;
					FVector TraceEnd = OutTrace.End;

					const FCollisionQueryParams& TraceParams = InputData.TraceParams;
					FVector TraceVector(TraceStart - TraceEnd);
					FVector TraceNormal = TraceVector.GetSafeNormal();

					FHitResult HitResult = FHitResult();
					ECollisionChannel SpringCollisionChannel = ECollisionChannel::ECC_WorldDynamic;
					FCollisionResponseParams ResponseParams;
					ResponseParams.CollisionResponse = FCollisionResponseContainer::GetDefaultResponseContainer();
					ResponseParams.CollisionResponse.Vehicle = ECR_Ignore;
					if (InWorld)
					{
						InWorld->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, SpringCollisionChannel, TraceParams, ResponseParams);
					}

#if CHAOS_DEBUG_DRAW
					//if (GModularVehicleDebugParams.ShowRaycasts)
					//{
					//	Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(TraceStart, TraceEnd, FColor::Yellow, false, -1.f, 0, 2.f);
					//}
#endif

					float Offset = Suspension->Setup().MaxLength;
					if (HitResult.bBlockingHit)
					{
						FVector LocalPos = Suspension->GetClusteredTransform().GetLocation();
						FVector LocalHitPoint = BodyTransform.InverseTransformPosition(HitResult.ImpactPoint);

						//Suspension->SetLocation(LocalHitPoint - LocalPos + WheelRadius);
						Offset = HitResult.Distance - WheelRadius;

						if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
						{
							const Chaos::FSimModuleTree::FSimModuleNode& WheelNode = ModuleArray[Suspension->GetWheelSimTreeIndex()];

							Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(WheelNode.SimModule);
							if (Wheel)
							{
								Wheel->SetSurfaceFriction(HitResult.PhysMaterial->Friction);
							}
						}

#if CHAOS_DEBUG_DRAW
						//if (GModularVehicleDebugParams.ShowRaycasts)
						//{
						//	Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(HitResult.ImpactPoint, 10, 16, FColor::White, false, -1.f, 0, 10.f);
						//}
#endif
					}

					Suspension->SetSpringLength(Offset, WheelRadius);
				}

				if (Node.SimModule->GetSimType() == Chaos::eSimType::Wheel)
				{
					Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(Node.SimModule);

					int TransformIndex = Node.SimModule->GetTransformIndex();

					// animate wheel rotation and steering
					FQuat Rot = FQuat(FVector(0, 1, 0), Wheel->GetAngularPosition());
					FQuat Steer = FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(Wheel->GetSteerAngleDegrees()));
					FTransform3f Transform = GeometryCollection.GetTransform(TransformIndex);
					Transform.SetRotation(FQuat4f(Node.SimModule->GetClusteredTransform().GetRotation() * Steer * Rot));
					GeometryCollection.SetTransform(TransformIndex, Transform);

					// animate suspension/ wheel
					if (Wheel->GetSuspensionSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
					{
						const Chaos::ISimulationModuleBase* SuspModule = SimModuleTree->GetSimModule(Wheel->GetSuspensionSimTreeIndex());
						const Chaos::FSuspensionSimModule* Suspension = static_cast<const Chaos::FSuspensionSimModule*>(SuspModule);
						float CurrentSpringLength = Suspension->GetSpringLength();
						FVector Loc = FVector(GeometryCollection.GetTransform(Node.SimModule->GetTransformIndex()).GetLocation());

						FVector RestPos = Suspension->GetRestLocation();

						Loc.Z = Suspension->Setup().MaxRaise + RestPos.Z + CurrentSpringLength;
						Transform.SetLocation(FVector3f(Loc));
						GeometryCollection.SetTransform(TransformIndex, Transform);

						GeometryCollection.MakeDirty();
					}
				}
			}

		}

	}

}

/**
 * ApplyDeferredForces should be called after the ParallelUpdatePT to send the calculated forces to the physics thread serially
 * as this cannot be done in parallel
 */
void FModularVehicleSimulationGC::ApplyDeferredForces(FGeometryCollectionPhysicsProxy* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree && Proxy)
	{
		// TODO: shouldn't be accessing Component
		if (UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(Proxy->GetOwner()))
		{
			check(GCComponent->GetOwner());

			if (const UGeometryCollection* Rest = GCComponent->GetRestCollection())
			{
				if (Rest->GetGeometryCollection() && Rest->GetGeometryCollection()->HasAttribute(TEXT("MassToLocal"), FTransformCollection::TransformGroup))
				{
					const TManagedArray<FTransform>& CollectionMassToLocal = Rest->GetGeometryCollection()->GetAttribute<FTransform>(TEXT("MassToLocal"), FTransformCollection::TransformGroup);

					SimModuleTree->AccessDeferredForces().Apply(
						Proxy->GetSolverParticleHandles(),
						Proxy->GetSolverClusterHandles(),
						Rest->GetGeometryCollection()->Transform,
						CollectionMassToLocal,
						Rest->GetGeometryCollection()->Parent);
				}
			}
		}
	}
}

void FModularVehicleSimulationGC::FillOutputState(FModularVehicleAsyncOutput& Output)
{
	// #TODO: what output values are we going to have for SimModules
	// #Note: remember to copy/interpolate values from the physics thread output in UModularVehicleComponent::ParallelUpdate

	//if (GModularVehicleDebugParams.ShowDebug)
	//{
	//	Output.VehicleSimOutput.DebugStrings.Empty();
	//	if (Chaos::FSimModuleTree* SimTree = GetSimComponentTree().Get())
	//	{
	//		for (int I = 0; I < SimTree->GetNumNodes(); I++)
	//		{
	//			FString NewString;
	//			if (SimTree->GetSimModule(I)->GetDebugString(NewString))
	//			{
	//				Output.VehicleSimOutput.DebugStrings.Add(NewString);
	//			}
	//		}
	//	}
	//}
}

Chaos::FControlInputs& FModularVehicleSimulationGC::AccessControlInputs()
{
	Chaos::EnsureIsInPhysicsThreadContext();
	return SimModuleTree->GetControlInputs();
}
