// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehicleDefaultAsyncInput.h"
#include "ChaosModularVehicle/ModularVehicleObject.h"
#include "Engine/Engine.h"
#include "SimModule/SimModulesInclude.h"
#include "ChaosModularVehicle/ChaosSimModuleManager.h"
#include "ChaosModularVehicle/VehicleSimBaseComponent.h"
#include "Chaos/DebugDrawQueue.h"

#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsReplication.h"

#include "VehicleUtility.h"
#include "Engine/Canvas.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Framework/Threading.h"

#if CHAOS_DEBUG_DRAW
#include "Chaos/DebugDrawQueue.h"
#endif

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

DEFINE_LOG_CATEGORY(LogModularVehicle);

class FModularVehicleSimulation;

bool IsServerNetMode(AActor* Owner)
{
	return Owner->IsNetMode(ENetMode::NM_ListenServer) || Owner->IsNetMode(ENetMode::NM_DedicatedServer);
}

Chaos::FPhysicsSolver* GetSolver(const UModularVehicleComponent& GeometryCollectionComponent)
{
	if (GeometryCollectionComponent.ChaosSolverActor)
	{
		return GeometryCollectionComponent.ChaosSolverActor->GetSolver();
	}
	else if (UWorld* CurrentWorld = GeometryCollectionComponent.GetWorld())
	{
		if (FPhysScene* Scene = CurrentWorld->GetPhysicsScene())
		{
			return Scene->GetSolver();
		}
	}
	return nullptr;
}

Chaos::FPBDRigidClusteredParticleHandle* UModularVehicleComponent::GetChassisParticle()
{
	Chaos::FPBDRigidClusteredParticleHandle* Particle = nullptr;

	if (FGeometryCollectionPhysicsProxy* Proxy = GetPhysicsProxy())
	{
		const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = Proxy->GetSolverClusterHandles();

		if (Clusters.Num() > 0 && !Clusters[0]->Disabled())
		{
			Particle = Clusters[0];
		}
	}
	return Particle;
}

UModularVehicleComponent::UModularVehicleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = false;

	bUsingNetworkPhysicsPrediction = UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction;

	if (bUsingNetworkPhysicsPrediction)
	{
		bEnableReplication = true;	// enable GC replication
	}

	SetIsReplicatedByDefault(true);

	// TODO: currently ordering of this must match EModularVehicleInputType
	InputInterpolationRates.Reset();
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Throttle")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Brake")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Clutch")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Steering")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Handbrake")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Pitch")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Roll")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Yaw")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("Gear")));
	InputInterpolationRates.Add(FModularVehicleInputRate(FString("DebugIndex")));
}

UModularVehicleComponent::~UModularVehicleComponent()
{
	
}

//-=====================================================
// Networking Replication

void UModularVehicleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UModularVehicleComponent, ModularVehicleRepData);
}

inline void UModularVehicleComponent::UpdateMVRepData()
{
	if (APawn* Owner = Cast<APawn>(GetOwner()))
	{ 
		APlayerController* OwningPC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
		if (OwningPC == nullptr)
		{
			return;
		}

		// If we have no owner or our netmode means we never require replication then early out
		if (bEnableReplication && IsServerNetMode(Owner) && GetIsReplicated())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UModularVehicleComponent, ModularVehicleRepData, this);
			++ModularVehicleRepData.Version;

			ModularVehicleRepData.ServerFrame = Owner->GetWorld()->GetPhysicsScene()->ReplicationCache.ServerFrame;

			//  Controls that were received on the server are now distributed to the clients
			Chaos::FControlInputs& ControlInputs = VehicleSimulationPT->AccessControlInputs();
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Steering] = ControlInputs.Steering;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Throttle] = ControlInputs.Throttle;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Brake] = ControlInputs.Brake;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Handbrake] = ControlInputs.Handbrake;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Roll] = ControlInputs.Roll;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Pitch] = ControlInputs.Pitch;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Yaw] = ControlInputs.Yaw;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Gear] = ControlInputs.GearNumber;
			ModularVehicleRepData.Data.Controls[EModularVehicleInputType::DebugIndex] = ControlInputs.InputDebugIndex;
		}
	}
}

inline void UModularVehicleComponent::ProcessMVRepData()
{
	if (UWorld* World = GetWorld())
	{
		if (MVRepVersionProcessed < ModularVehicleRepData.Version && GetWorld()->IsNetMode(NM_Client))
		{
			MVRepVersionProcessed = ModularVehicleRepData.Version;

			if (APawn* MyPawn = Cast<APawn>(GetOwner()))
			{
				APlayerController* OwningPC = MyPawn ? Cast<APlayerController>(MyPawn->GetController()) : nullptr;

				// only update simulated proxy with this particluar data, don't override the local autonimous proxy
				bool UpdateClient = (OwningPC == nullptr);
				if (UpdateClient)
 				{
					//  Controls from other clients received for simulates proxies
					Chaos::FControlInputs& ControlInputs = VehicleSimulationPT->AccessControlInputs();
					ControlInputs.Steering = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Steering];
					ControlInputs.Throttle = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Throttle];
					ControlInputs.Brake = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Brake];
					ControlInputs.Handbrake = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Handbrake];
					ControlInputs.Roll = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Roll];
					ControlInputs.Pitch = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Pitch];
					ControlInputs.Yaw = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Yaw];
					ControlInputs.GearNumber = ModularVehicleRepData.Data.Controls[EModularVehicleInputType::Gear];
				}
			}
		}
	}
}

bool UModularVehicleComponent::ProcessRepData(const float DeltaTime, const float SimTime)
{
	bool bProcessed = false;
	const int ChassisIdx = 0;	// TODO: make this more formal

	// With Network Prediction the replication data is largely ignored until the error is greater than the specified tolerance
	// at which point the client transform will need to be updated to match the authoritative server transform with a resim, but blending away the error over time 
	// This benefits local control with no input/latency lag.
	Chaos::FPhysicsSolver* Solver = GetSolver(*this);
	Chaos::FRewindData* RewindData = Solver->GetRewindData();

	if (bUsingNetworkPhysicsPrediction)
	{
		if (RewindData && !RewindData->IsResim())
		{
			APawn* MyPawn = Cast<APawn>(GetOwner());
			APlayerController* OwningPC = MyPawn ? Cast<APlayerController>(MyPawn->GetController()) : nullptr;
			// no player controller so must be a SimulatedProxy, or if one off event we also need to replicate locally
			if (OwningPC)
			{
				if (UWorld* World = GetWorld())
				{
					if (!RepData.Clusters.IsEmpty())
					{
						check(World->GetNetMode() == NM_Client);

						if (APlayerController* PlayerController = World->GetFirstPlayerController())
						{
							// Same logic as PhysicsReplication.cpp to get the correct local frame in the history buffer
							int LocalFrameOffset = PlayerController->GetNetworkPhysicsTickOffset();
							const int32 ServerFrame = RepData.ServerFrame;
							const int32 LocalFrame = RepData.ServerFrame - LocalFrameOffset;
						
							if (FGeometryCollectionPhysicsProxy* Proxy = GetPhysicsProxy())
							{
								const FGeometryCollectionClusterRep& TargetState = RepData.Clusters[ChassisIdx];

								// have access to the history buffer
								check(RewindData);
								check(!RewindData->IsResim());

								const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = Proxy->GetSolverClusterHandles();
								auto* ChassisHandle = Clusters[ChassisIdx];
								const FGeometryCollectionClusterRep& RepCluster = RepData.Clusters[ChassisIdx];

								// Need the local frame in order to access correct history data
								if (ChassisHandle && RewindData && LocalFrame >= RewindData->GetEarliestFrame_Internal() && LocalFrame <= RewindData->CurrentFrame())
								{
									static constexpr Chaos::FFrameAndPhase::EParticleHistoryPhase RewindPhase = Chaos::FFrameAndPhase::EParticleHistoryPhase::PostPushData;
									Chaos::EnsureIsInPhysicsThreadContext();

									Chaos::FGeometryParticleState PastState = RewindData->GetPastStateAtFrame(*ChassisHandle, LocalFrame);
									int ThisFrame = RewindData->CurrentFrame();
									Chaos::FGeometryParticleState CurrentState = RewindData->GetPastStateAtFrame(*ChassisHandle, ThisFrame);

									const FVector PastPosition = PastState.X();
									FVector ErrorVector = (PastPosition - TargetState.Position);

									FQuat ErrorQuat = PastState.R() * TargetState.Rotation.Inverse(); // difference between quaternions

									const float ErrorPosition = ErrorVector.Size();

									const float ErrorThreshold = MyPawn->GetResimulationThreshold();
									const bool ErrorExceeded = (ErrorPosition >= ErrorThreshold) ? true : false;
									
#if CHAOS_DEBUG_DRAW
									if (Chaos::FPhysicsSolverBase::CanDebugNetworkPhysicsPrediction())
									{
										// Server historical position - will appear somewhat lagged to the client
										float BoxSize1 = 150.0f;
										float BoxThickness = 6.0f;
										Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(TargetState.Position, FVector(BoxSize1, BoxSize1, BoxSize1), TargetState.Rotation, FColor::Black, false, 0.03f, 0, BoxThickness);
									
										// client's current position
										float BoxSize2 = 145.0f;
										Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(CurrentState.X(), FVector(BoxSize2, BoxSize2, BoxSize2), CurrentState.R(), FColor::Green, false, 0.03f, 0, BoxThickness);
									}
#endif

									// want to always fall through so debug GFX works
									if (LocalFrame < RewindData->CurrentFrame())
									{											
										FAsyncPhysicsTimestamp TimeStamp;
										TimeStamp.LocalFrame = RewindData->CurrentFrame();

										float ColorLerp = (ErrorPosition >= ErrorThreshold) ? 1.0f : 0.0f;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
										if (Chaos::FPhysicsSolverBase::CanDebugNetworkPhysicsPrediction())
										{
											UE_LOG(LogModularVehicle, Log, TEXT("Particle Position Error = %f | Resim Trigger = %f | Server Frame = %d | Client Frame = %d"), ErrorPosition, ColorLerp, ServerFrame/*PhysicsTarget.ServerFrame*/, LocalFrame);
											UE_LOG(LogModularVehicle, Log, TEXT("Particle Target Position = %s | Current Position = %s"), *TargetState.Position.ToString(), *PastState.X().ToString());
											UE_LOG(LogModularVehicle, Log, TEXT("Particle Target Velocity = %s | Current Velocity = %s"), *TargetState.LinearVelocity.ToString(), *PastState.V().ToString());
											UE_LOG(LogModularVehicle, Log, TEXT("Particle Target Quaternion = %s | Current Quaternion = %s"), *TargetState.Rotation.ToString(), *PastState.R().ToString());
											UE_LOG(LogModularVehicle, Log, TEXT("Particle Target Omega = %s | Current Omega= %s"), *TargetState.AngularVelocity.ToString(), *PastState.W().ToString());

#if CHAOS_DEBUG_DRAW
											if (UWorld* OwningWorld = GetWorld())
											{
												static constexpr float BoxSize = 5.0f;
												const FColor DebugColor = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, ColorLerp).ToFColor(false);

												bool PersistLines = true;
												float DebugRenderFadeTime = 5.0f;
												Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(TargetState.Position, FVector(BoxSize, BoxSize, BoxSize), TargetState.Rotation, FColor::Orange, PersistLines, DebugRenderFadeTime, 0, 1.0f);
												Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(PastState.X(), FVector(6, 6, 6), PastState.R(), DebugColor, PersistLines, DebugRenderFadeTime, 0, 1.0f);

												Chaos::FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(PastState.X(), TargetState.Position, 5.0f, FColor::Red, PersistLines, DebugRenderFadeTime, 0, 0.5f);
											}
#endif
										}
#endif

										const bool bShouldSleep = false; // #TODO - need equivalent of (TargetState.Flags & ERigidBodyFlags::Sleeping) != 0;

										RewindData->SetTargetStateAtFrame(*GetChassisParticle(), LocalFrame, RewindPhase,
											TargetState.Position, TargetState.Rotation,
											TargetState.LinearVelocity, TargetState.AngularVelocity, bShouldSleep);

										if (ColorLerp >= 1.0f)
										{
											if (Chaos::FPBDRigidsSolver* RigidSolver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>())
											{
												RigidSolver->GetEvolution()->GetIslandManager().SetParticleResimFrame(GetChassisParticle(), LocalFrame);
											}

											int ResimFrame = RewindData->GetResimFrame();
											ResimFrame = (ResimFrame == INDEX_NONE) ? LocalFrame : FMath::Min(ResimFrame, LocalFrame);
											RewindData->SetResimFrame(ResimFrame);
											UE_LOG(LogModularVehicle, Log, TEXT("### ModularVehicleComponent: Set ResimFrame %d"), ResimFrame);
										}

									}

									VersionProcessed = RepData.Version;
									bProcessed = true;
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// Client is always corrected to the server position and so is lagged in time
		return Super::ProcessRepData(DeltaTime, SimTime);
	}

	return true;
}

inline void UModularVehicleComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(ModularVehicleComponent_AsyncPhysicsTick);

	ENetMode NetMode = GetNetMode();

	// Replication from server to client not required in stand-alone mode
	if (NetMode != ENetMode::NM_Standalone)
	{
		// Geometry Collection Replication
		Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);

		// Modular Vehicle Replication
		if (NetMode == ENetMode::NM_Client)
		{
			ProcessMVRepData();
		}
		else
		{
			UpdateMVRepData();
		}
	}
}

//-=====================================================

void UModularVehicleComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UModularVehicleComponent::ShouldCreatePhysicsState() const
{
	return true;
}

void UModularVehicleComponent::UpdateChassisUnionCollision()
{
	if (const UGeometryCollection* Rest = GetRestCollection())
	{
		TArray<UActorComponent*> Components;
		GetOwner()->GetComponents(UVehicleSimBaseComponent::StaticClass(), Components, true);

		for (int32 ii = 0, ni = Components.Num(); ii < ni; ++ii)
		{
			UVehicleSimBaseComponent* Component = Cast<UVehicleSimBaseComponent>(Components[ii]);
			if (Component)
			{
				if (Component->bRemoveFromClusterCollisionModel)
				{
					Rest->GetGeometryCollection()->SetFlags(Component->TransformIndex, FGeometryCollection::ENodeFlags::FS_IgnoreCollisionInParentCluster);
				}
				else
				{
					Rest->GetGeometryCollection()->ClearFlags(Component->TransformIndex, FGeometryCollection::ENodeFlags::FS_IgnoreCollisionInParentCluster);
				}
			}
		}
	}
}

void UModularVehicleComponent::OnCreatePhysicsState()
{
	// Selectively disable child collision from the parent cluster union collision geometry
	// which is create when Super::OnCreatePhysicsState is called
	UpdateChassisUnionCollision();

	// Geometry Collection Processing
	Super::OnCreatePhysicsState();

	// Had to move this into PostInitializeComponents since the child components are not yet avaiable/loaded at this time
	// Vehicle Sim Module Processing
	//CreateVehicleSim();

}

void UModularVehicleComponent::OnDestroyPhysicsState()
{
	// Geometry Collection Processing
	Super::OnDestroyPhysicsState();

	// Vehicle Sim Module Processing
	DestroyVehicleSim();
}

void UModularVehicleComponent::CreateVehicleSim()
{
	if (FGeometryCollectionPhysicsProxy* Proxy = GetPhysicsProxy())
	{

		VehicleSimulationPT = MakeUnique<FModularVehicleSimulationGC>(bUsingNetworkPhysicsPrediction);

		PVehicleOutput = MakeUnique<FPhysicsVehicleOutput>();	// create physics output container
	
		GenerateSimTree();

		// Register with the Sim Manager
		if (UWorld* World = GetWorld())
		{
			if (World->IsGameWorld())
			{
				FPhysScene* PhysScene = World->GetPhysicsScene();

				if (FChaosSimModuleManager* SimManager = FChaosSimModuleManager::GetManagerFromScene(PhysScene))
				{
					SimManager->AddVehicle(this);
				}
			}
		}

		// copy input rate setup across to physics thread
		TArray<FModularVehicleInputRate>& Inputs = VehicleSimulationPT->AccessInputInterpolation();
		check(InputInterpolationRates.Num() == Inputs.Num());
		for (int I = 0; I < Inputs.Num(); I++)
		{
			Inputs[I] = InputInterpolationRates[I];
		}
	}

}

void UModularVehicleComponent::DestroyVehicleSim()
{
	UWorld* World = GetWorld();
	if (World->IsGameWorld())
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();

		if (FChaosSimModuleManager* SimManager = FChaosSimModuleManager::GetManagerFromScene(PhysScene))
		{
			SimManager->RemoveVehicle(this);
		}
	}

	if (PVehicleOutput.IsValid())
	{
		PVehicleOutput.Reset(nullptr);
	}

	if (VehicleSimulationPT.IsValid())
	{
		VehicleSimulationPT->Terminate();
		VehicleSimulationPT.Reset(nullptr);
	}
}

TUniquePtr<Chaos::FSimModuleTree> UModularVehicleComponent::GenerateSimTree()
{
	TUniquePtr<Chaos::FSimModuleTree> SimModuleTree = MakeUnique<Chaos::FSimModuleTree>();	// create physics output container

	// take the UVehicleSimBaseComponents and back them into a more compact structure for simulation on physics callback thread.
	if (SimModuleTree)
	{
		int TreeIndex = -1;

		if (const UGeometryCollection* Rest = GetRestCollection())
		{
			if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GC = Rest->GetGeometryCollection())
			{
				if (const UModularVehicle* ModularVehicle = Cast<const UModularVehicle>(Rest))
				{
					TArray<UActorComponent*> Components;
					GetOwner()->GetComponents(UVehicleSimBaseComponent::StaticClass(), Components, true);

					// add a single chassis root component
					Chaos::FChassisSettings Settings;
					Chaos::ISimulationModuleBase* Chassis = new Chaos::FChassisSimModule(Settings);
					Chassis->SetTransformIndex(0);
					Chassis->SetTreeIndex(0);
					int RootIndex = SimModuleTree->AddRoot(Chassis);

					// add all other sim components - all at root for now, fixup hierarchy next step (since Component->GetAttachChildren is always empty?!)
					for (int32 ii = 0, ni = Components.Num(); ii < ni; ++ii)
					{
						UVehicleSimBaseComponent* Component = Cast<UVehicleSimBaseComponent>(Components[ii]);
						if (Component)
						{
							int TransformIndex = Component->TransformIndex;

							if (TransformIndex >= GC->Parent.Num())
							{
								break; // something is amiss - only support a single sim module per component at present
							}

							// create simulation data
							Chaos::ISimulationModuleBase* Module = Component->CreateNewCoreModule();
							Module->SetSimModuleTree(SimModuleTree.Get());

							TreeIndex = SimModuleTree->AddNodeBelow(RootIndex, Module);
							Component->TreeIndex = TreeIndex;
							Module->SetTreeIndex(TreeIndex);
							Module->SetTransformIndex(TransformIndex);

							if (GC->HasAttribute(TEXT("MassToLocal"), FTransformCollection::TransformGroup) && GC->Parent[TransformIndex] != -1)
							{
								const TManagedArray<FTransform>& CollectionMassToLocal = GC->GetAttribute<FTransform>(TEXT("MassToLocal"), FTransformCollection::TransformGroup);

								Module->SetIntactTransform(FTransform::Identity);

								Module->SetClusteredTransform(CollectionMassToLocal[GC->Parent[TransformIndex]].Inverse() * FTransform(GC->Transform[TransformIndex]));
								Module->SetClustered(true);

							//	UE_LOG(LogSimulationModule, Log, TEXT("[%d] %s GetClusteredTransformLoc %s"), TransformIndex, *Module->GetDebugName(), *Module->GetClusteredTransform().GetLocation().ToString());
							}
						}
					}

					// fix up hierearchy - having to do this as a 2 stage process because Component->GetAttachChildren() is always empty which isn't right 
					for (int32 ii = 0, ni = Components.Num(); ii < ni; ++ii)
					{
						UVehicleSimBaseComponent* Component = Cast<UVehicleSimBaseComponent>(Components[ii]);
						if (Component)
						{
							int ComponentIndex = Component->TreeIndex;
							if (Component->GetAttachParent() != nullptr)
							{
								if (UVehicleSimBaseComponent* Parent = Cast<UVehicleSimBaseComponent>(Component->GetAttachParent()))
								{
									SimModuleTree->Reparent(ComponentIndex, Parent->TreeIndex);
								}
							}
						}
					}
				}
			}
		}

		FixupTreeLinks(SimModuleTree);

		EnableAnimationForPhysicsDrivenTransforms(SimModuleTree);

		// Physics thread takes ownsership of the tree from here
		VehicleSimulationPT->Initialize(SimModuleTree);

	}

	return SimModuleTree;
}

void UModularVehicleComponent::EnableAnimationForPhysicsDrivenTransforms(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree)
{
	if (SimModuleTree)
	{
		if (FGeometryCollectionPhysicsProxy* Proxy = GetPhysicsProxy())
		{
			// AddAttribute/FindAttribute
			FGeometryDynamicCollection& GTCollection = Proxy->GetExternalCollection();

			GTCollection.AddAnimateTransformAttribute();
			TManagedArray<bool>* AnimationsActive = GTCollection.GetAnimateTransformAttribute();

			// Find the wheels and set their GC transform to animate back to the GC even when nodes are intact/disabled
			// TODO: rather than searching for wheels maybe we could have an animate flag on all tree nodes and we search for that being enabled
			const TArray<Chaos::FSimModuleTree::FSimModuleNode>& ModuleArray = SimModuleTree->GetSimulationModuleTree();
			for (const Chaos::FSimModuleTree::FSimModuleNode& Node : ModuleArray)
			{
				if (Node.SimModule->GetSimType() == Chaos::eSimType::Wheel)
				{
					Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(Node.SimModule);

					int TransformIndex = Node.SimModule->GetTransformIndex();
					if (TransformIndex != Chaos::ISimulationModuleBase::INVALID_IDX)
					{
						(*AnimationsActive)[TransformIndex] = true;
					}
				}
			}
		}
	}
}


Chaos::FWheelSimModule* UModularVehicleComponent::LocatePhysicallyClosestWheel(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree, int SuspensionTreeIndex)
{
	const Chaos::FSuspensionSimModule* Suspension = static_cast<const Chaos::FSuspensionSimModule*>(SimModuleTree->GetSimModule(SuspensionTreeIndex));

	FVector SusLoc = Suspension->GetClusteredTransform().GetLocation();
	float ClosestDistance = FLT_MAX;
	int ClosestIndex = Chaos::ISimulationModuleBase::INVALID_IDX;

	for (int I = 0; I < SimModuleTree->NumActiveNodes(); I++)
	{
		if (SimModuleTree->GetSimModule(I)->GetSimType() == Chaos::eSimType::Wheel)
		{
			const Chaos::FWheelSimModule* Wheel = static_cast<const Chaos::FWheelSimModule*>(SimModuleTree->GetSimModule(I));
			if (Wheel)
			{
				FVector WheelLoc = Wheel->GetClusteredTransform().GetLocation();
				FVector Vec = WheelLoc - SusLoc;
				float DistSqr = Vec.SquaredLength();

				if (DistSqr < ClosestDistance)
				{
					ClosestDistance = DistSqr;
					ClosestIndex = I;
				}
			}
		}
	}

	if (ClosestIndex != Chaos::ISimulationModuleBase::INVALID_IDX)
	{
		return static_cast<Chaos::FWheelSimModule*>(SimModuleTree->AccessSimModule(ClosestIndex));

	}

	return nullptr;
}

void UModularVehicleComponent::FixupTreeLinks(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree)
{
	if (const UGeometryCollection* Rest = GetRestCollection())
	{
		if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GC = Rest->GetGeometryCollection())
		{
			for (int I = 0; I < SimModuleTree->NumActiveNodes(); I++)
			{
				Chaos::ISimulationModuleBase* Module = SimModuleTree->AccessSimModule(I);
				if (Module->GetSimType() == Chaos::eSimType::Suspension)
				{
					if (Chaos::FSuspensionSimModule* Suspension = static_cast<Chaos::FSuspensionSimModule*>(Module))
					{
						if (Chaos::FWheelSimModule* Wheel = LocatePhysicallyClosestWheel(SimModuleTree, Suspension->GetTreeIndex()))
						{
							int WheelIndex = Wheel->GetTreeIndex();
							int WheelTransformIndex = Wheel->GetTransformIndex();

							Suspension->AccessSetup().RestOffset = FVector(GC->Transform[WheelTransformIndex].GetLocation());
							Suspension->SetIntactTransform(SimModuleTree->AccessSimModule(WheelIndex)->GetIntactTransform());
							Suspension->SetClusteredTransform(SimModuleTree->AccessSimModule(WheelIndex)->GetClusteredTransform());

							Wheel->SetSuspensionSimTreeIndex(Suspension->GetTreeIndex());
							Suspension->SetWheelSimTreeIndex(Wheel->GetTreeIndex());
						}
					}
				}
			}
		}
	}
}

//-=====================================================

void UModularVehicleComponent::SetThrottleInput(float Throttle)
{
	RawThrottleInput = FMath::Clamp(Throttle, -1.0f, 1.0f);
}

void UModularVehicleComponent::IncreaseThrottleInput(float ThrottleDelta)
{
	RawThrottleInput = FMath::Clamp(RawThrottleInput + ThrottleDelta, 0.f, 1.0f);
}

void UModularVehicleComponent::DecreaseThrottleInput(float ThrottleDelta)
{
	RawThrottleInput = FMath::Clamp(RawThrottleInput - ThrottleDelta, 0.f, 1.0f);
}

void UModularVehicleComponent::SetBrakeInput(float Brake)
{
	RawBrakeInput = FMath::Clamp(Brake, -1.0f, 1.0f);
}

void UModularVehicleComponent::SetSteeringInput(float Steering)
{
	RawSteeringInput = FMath::Clamp(Steering, -1.0f, 1.0f);
}

void UModularVehicleComponent::SetPitchInput(float Pitch)
{
	RawPitchInput = FMath::Clamp(Pitch, -1.0f, 1.0f);
}

void UModularVehicleComponent::SetRollInput(float Roll)
{
	RawRollInput = FMath::Clamp(Roll, -1.0f, 1.0f);
}

void UModularVehicleComponent::SetYawInput(float Yaw)
{
	RawYawInput = FMath::Clamp(Yaw, -1.0f, 1.0f);
}

void UModularVehicleComponent::SetHandbrakeInput(float Handbrake)
{
	RawHandbrakeInput = Handbrake;
}

void UModularVehicleComponent::SetGearInput(int Gear)
{
	RawGearInput = Gear;
}

//-=====================================================

void UModularVehicleComponent::PreTickGT(float DeltaTime)
{
	ProcessControls(DeltaTime);
}

TUniquePtr<FModularVehicleAsyncInput> UModularVehicleComponent::SetCurrentAsyncData(int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp)
{
	TUniquePtr<FModularVehicleDefaultAsyncInput> CurInput = MakeUnique<FModularVehicleDefaultAsyncInput>();
	SetCurrentAsyncDataInternal(CurInput.Get(), InputIdx, CurOutput, NextOutput, Alpha, VehicleManagerTimestamp);
	return CurInput;
}

/************************************************************************/
/* Setup the current async I/O data                                     */
/************************************************************************/
void UModularVehicleComponent::SetCurrentAsyncDataInternal(FModularVehicleAsyncInput* CurInput, int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp)
{
	ensure(CurAsyncInput == nullptr);	//should be reset after it was filled
	ensure(CurAsyncOutput == nullptr);	//should get reset after update is done
	FModularVehicleDefaultAsyncInput* AsyncInput = static_cast<FModularVehicleDefaultAsyncInput*>(CurInput);

	CurAsyncInput = CurInput;
	AsyncInput->SetVehicle(this);
	NextAsyncOutput = nullptr;
	OutputInterpAlpha = 0.f;

	// We need to find our vehicle in the output given
	if (CurOutput)
	{
		for (int32 PendingOutputIdx = 0; PendingOutputIdx < OutputsWaitingOn.Num(); ++PendingOutputIdx)
		{
			// Found the correct pending output, use index to get the vehicle.
			if (OutputsWaitingOn[PendingOutputIdx].Timestamp == CurOutput->Timestamp)
			{
				const int32 VehicleIdx = OutputsWaitingOn[PendingOutputIdx].Idx;
				FModularVehicleAsyncOutput* VehicleOutput = CurOutput->VehicleOutputs[VehicleIdx].Get();
				if (VehicleOutput && VehicleOutput->bValid && VehicleOutput->Type == CurAsyncType)
				{
					CurAsyncOutput = VehicleOutput;

					if (NextOutput && NextOutput->Timestamp == CurOutput->Timestamp)
					{
						// This can occur when sub-stepping - in this case, VehicleOutputs will be in the same order in NextOutput and CurOutput.
						FModularVehicleAsyncOutput* VehicleNextOutput = NextOutput->VehicleOutputs[VehicleIdx].Get();
						if (VehicleNextOutput && VehicleNextOutput->bValid && VehicleNextOutput->Type == CurAsyncType)
						{
							NextAsyncOutput = VehicleNextOutput;
							OutputInterpAlpha = Alpha;
						}
					}
				}

				// these are sorted by timestamp, we are using latest, so remove entries that came before it.
				TArray<FAsyncOutputWrapper> NewOutputsWaitingOn;
				for (int32 CopyIndex = PendingOutputIdx; CopyIndex < OutputsWaitingOn.Num(); ++CopyIndex)
				{
					NewOutputsWaitingOn.Add(OutputsWaitingOn[CopyIndex]);
				}

				OutputsWaitingOn = MoveTemp(NewOutputsWaitingOn);
				break;
			}
		}

	}

	if (NextOutput && CurOutput)
	{
		if (NextOutput->Timestamp != CurOutput->Timestamp)
		{
			// NextOutput and CurOutput occurred in different steps, so we need to search for our specific vehicle.
			for (int32 PendingOutputIdx = 0; PendingOutputIdx < OutputsWaitingOn.Num(); ++PendingOutputIdx)
			{
				// Found the correct pending output, use index to get the vehicle.
				if (OutputsWaitingOn[PendingOutputIdx].Timestamp == NextOutput->Timestamp)
				{
					FModularVehicleAsyncOutput* VehicleOutput = NextOutput->VehicleOutputs[OutputsWaitingOn[PendingOutputIdx].Idx].Get();
					if (VehicleOutput && VehicleOutput->bValid && VehicleOutput->Type == CurAsyncType)
					{
						NextAsyncOutput = VehicleOutput;
						OutputInterpAlpha = Alpha;
					}
					break;
				}
			}
		}
	}

	FAsyncOutputWrapper& NewOutput = OutputsWaitingOn.AddDefaulted_GetRef();
	NewOutput.Timestamp = VehicleManagerTimestamp;
	NewOutput.Idx = InputIdx;
}

// ---- ASYNC ----

FModularVehicleDefaultAsyncInput::FModularVehicleDefaultAsyncInput()
{

}

/************************************************************************/
/* Async simulation callback on the Physics Thread                      */
/************************************************************************/
TUniquePtr<FModularVehicleAsyncOutput> FModularVehicleDefaultAsyncInput::Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const
{
	TUniquePtr<FModularVehicleAsyncOutput> Output = MakeUnique<FModularVehicleAsyncOutput>();

	//support nullptr because it allows us to go wide on filling the async inputs
	if (Proxy == nullptr)
	{
		return Output;
	}

	// FILL OUTPUT DATA HERE THAT WILL GET PASSED BACK TO THE GAME THREAD
	FGeometryCollectionPhysicsProxy* GCProxy = static_cast<FGeometryCollectionPhysicsProxy*>(Proxy);
	if (GCProxy)
	{
		GCVehicle->VehicleSimulationPT->Simulate(World, DeltaSeconds, *this, *Output.Get(), GCProxy);
	}

	FModularVehicleAsyncOutput& OutputData = *Output.Get();
	GCVehicle->VehicleSimulationPT->FillOutputState(OutputData);

	Output->bValid = true;

	return MoveTemp(Output);
}

void FModularVehicleDefaultAsyncInput::ApplyDeferredForces() const
{
	if (Proxy)
	{
		FGeometryCollectionPhysicsProxy* GCProxy = static_cast<FGeometryCollectionPhysicsProxy*>(Proxy);
		if (GCProxy)
		{
			GCVehicle->VehicleSimulationPT->ApplyDeferredForces(GCProxy);
		}
	}
}

//void FModularVehicleDefaultAsyncInput::ProcessInputs()
//{
//}



/************************************************************************/
/* PASS ANY INPUTS TO THE PHYSICS THREAD SIMULATION IN HERE              */
/************************************************************************/
void UModularVehicleComponent::Update(float DeltaTime)
{
	if (CurAsyncInput)
	{
		CurAsyncInput->Proxy = GetPhysicsProxy();

		FModularVehicleDefaultAsyncInput* AsyncInput = static_cast<FModularVehicleDefaultAsyncInput*>(CurAsyncInput);

		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(GetOwner()); // ignore self in scene query

		FCollisionQueryParams TraceParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(), false, nullptr);
		TraceParams.bReturnPhysicalMaterial = true;	// we need this to get the surface friction coefficient
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		TraceParams.bTraceComplex = true;
		AsyncInput->TraceParams = TraceParams;
	}
}

// This is called after the pull from physics state
void UModularVehicleComponent::PostUpdate()
{
	return;

// TODO - code for visual error blending
//	// want this to run after pull from physics state but before the UpdateGlobalMatricesWithExplodedVectors for render update?
//	const int ChassisIdx = 0;	// TODO: make this more formal
//
//	const float ErrorThreshold = UPhysicsSettings::Get()->PhysicsPrediction.ResimulationErrorThreshold;
//
//	NetReplicationError *= 0.98f;
//	if (NextRepError.Size() > SMALL_NUMBER)
//	{
//		NetReplicationError = NextRepError;
//		NextRepError = FVector::ZeroVector;
//	}
//
//	if (bUsingNetworkPhysicsPrediction)
//	{
//		if (Chaos::FPhysicsSolver* Solver = GetSolver(*this))
//		{
//			if (FGeometryCollectionPhysicsProxy* Proxy = GetPhysicsProxy())
//			{
//				Chaos::FRewindData* RewindData = Solver->GetRewindData();
//
//				// [BH] just for debug remove
//				const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClustersTmp = Proxy->GetSolverClusterHandles();
//				auto* ChassisHandleTmp = ClustersTmp[ChassisIdx];
//				if (ChassisHandleTmp)
//				{
//					UE_LOG(LogModularVehicle, Log, TEXT("***** Pos %s"), *ChassisHandleTmp->X().ToString());
//
//					if (!RewindData->IsResim())
//					{
//						FVector Delta = PrevPosOther - ChassisHandleTmp->X();
//						if (Delta.SizeSquared() > (ErrorThreshold * ErrorThreshold))
//						{
//							// Warp
//							UE_LOG(LogModularVehicle, Log, TEXT("***** WARPING %s"), *Delta.ToString());
//
//							/*NetReplicationError*/NextRepError = Delta; // delay this by one frame
//
//						}
//
//						PrevPosOther = ChassisHandleTmp->X();
//					}
//				}
//
//
////				Chaos::FRewindData* RewindData = Solver->GetRewindData();
////				if (RewindData && !RewindData->IsResim())
////				{			
////					// [BH - TODO setup ResimFinishFrame any good!!]
////					if (Warping && ResimFinishFrame == RewindData->CurrentFrame())
////					{
////						const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = Proxy->GetSolverClusterHandles();
////						auto* ChassisHandle = Clusters[ChassisIdx];
////						if (ChassisHandle)
////						{
////#ifdef ONWAYORANOTHER
////							FTransform Loc = GameThreadCollection.Transform[ChassisTransformGroupIndex];
////							FVector Delta = Loc.GetLocation() - PreRewindPos.GetLocation();
////#else
////							// is this thread safe accessing ChassisHandle->X()
////							FVector Loc = ChassisHandle->X();
////							FVector Delta = Loc - PreRewindPos;
////
////							UE_LOG(LogModularVehicle, Log, TEXT("***** RESIM Final LOC %s"), *PreRewindPos.ToString());
////							UE_LOG(LogModularVehicle, Log, TEXT("***** Delta %s"), *Delta.ToString());
////#endif
////							Warping = false;
////							NetReplicationError = -Delta;
////						}
////					}
////				}		
//			}
//		}
//	}
}

void UModularVehicleComponent::FinalizeSimCallbackData(FChaosSimModuleManagerAsyncInput& Input)
{
	bool bIsPhysicsStateCreated = true;
	CurAsyncInput = nullptr;
	CurAsyncOutput = nullptr;
}


/***************************************************************************/
/* READ OUTPUT DATA - Access the async output data from the Physics Thread */
/***************************************************************************/
void UModularVehicleComponent::ParallelUpdate(float DeltaSeconds)
{
	if (FModularVehicleAsyncOutput* CurrentOutput = static_cast<FModularVehicleAsyncOutput*>(CurAsyncOutput))
	{
		if (CurrentOutput->bValid && PVehicleOutput)
		{
			// #TODO: lerp output values here
			// i.e. PVehicleOutput->Value = FMath::Lerp(CurAsyncOutput->VehicleSimOutput.Value, NextAsyncOutput->VehicleSimOutput.Value, OutputInterpAlpha);
			//for (int I=0; I<CurAsyncOutput->VehicleSimOutput.ModuleTransform.Num(); I++)
			//{
			//	PVehicleOutput->ModuleTransform[I] = CurAsyncOutput->VehicleSimOutput.ModuleTransform[I];
			//}
		}
	}
}

void UModularVehicleComponent::ClearRawInput()
{
	RawThrottleInput = 0.0f;
	RawBrakeInput = 0.0f;
	RawSteeringInput = 0.0f;
	RawClutchInput = 0.0f;	// 0 is engaged/locked, 1 is depressed/open
	RawPitchInput = 0.0f;
	RawRollInput = 0.0f;
	RawYawInput = 0.0f;
	RawHandbrakeInput = false;
}


void UModularVehicleComponent::ResetVehicleState()
{
	ClearRawInput();

	OnDestroyPhysicsState();
	OnCreatePhysicsState();
}


void UModularVehicleComponent::ShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	UFont* RenderFont = GEngine->GetMediumFont();

	// draw input values
	Canvas->SetDrawColor(FColor::White);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Throttle Raw  (%f) %f"), RawThrottleInput, ThrottleInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Brake Raw     (%f) %f"), RawBrakeInput, BrakeInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Steering Raw  (%f) %f"), RawSteeringInput, SteeringInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Handbrake Raw (%f) %f"), RawHandbrakeInput, HandbrakeInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Roll Raw      (%f) %f"), RawRollInput, RollInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Pitch Raw     (%f) %f"), RawPitchInput, PitchInput), 4, YPos);
	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Yaw Raw       (%f) %f"), RawYawInput, YawInput), 4, YPos);

	YPos += 10;

	// draw general vehicle data
	{
		Canvas->SetDrawColor(FColor::White);
		YPos += 16;

		//for (const FString& StringOut : PVehicleOutput->DebugStrings)
		//{
		//	YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("%s"), *StringOut), 4, YPos);
		//}

		//PVehicleOutput->DebugStrings.Empty();
	}

	YPos += 10;

}


// networking

void UModularVehicleComponent::ProcessControls(float DeltaTime)
{
	float PhysicsDeltaTime = DeltaTime;
	if (UPhysicsSettings::Get())
	{
		PhysicsDeltaTime = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
	}

	APawn* MyPawn = Cast<APawn>(GetOwner());
	if (MyPawn)
	{
		APlayerController* OwningPC = MyPawn ? Cast<APlayerController>(MyPawn->GetController()) : nullptr;

		if (OwningPC != nullptr && GetOuter() != nullptr && !OwningPC->IsNetMode(NM_DedicatedServer))
		{
			const FAsyncPhysicsTimestamp ActualTimestamp = OwningPC->GetPhysicsTimestamp();

			// Apply controls locally
			if ((OwningPC->GetLocalRole() >= ENetRole::ROLE_AutonomousProxy)&&(OwningPC->GetRemoteRole() != ENetRole::ROLE_AutonomousProxy))
			{
				//UE_LOG(LogModularVehicle, Log, TEXT("ApplyLocally [NetMode %d,  LocalRole %d,  GetRemoteRole %d] %f %f"), OwningPC->GetNetMode(), OwningPC->GetLocalRole(), OwningPC->GetRemoteRole(), RawThrottleInput, RawSteeringInput);
				ApplyControls_Imp(RawSteeringInput, RawThrottleInput, RawBrakeInput, HandbrakeInput, RawGearInput, RawRollInput, RawPitchInput, RawYawInput, ActualTimestamp);

				if (OwningPC->IsNetMode(NM_Client))
				{
					// and send to server
					//UE_LOG(LogModularVehicle, Log, TEXT("SendServerControls [NetMode %d,  LocalRole %d,  GetRemoteRole %d] %f %f"), OwningPC->GetNetMode(), OwningPC->GetLocalRole(), OwningPC->GetRemoteRole(), RawThrottleInput, RawSteeringInput);
					ServerApplyControls(RawSteeringInput, RawThrottleInput, RawBrakeInput, HandbrakeInput, RawGearInput, RawRollInput, RawPitchInput, RawYawInput, ActualTimestamp);
				}
			}
		}
	}
}

void UModularVehicleComponent::ServerApplyControls_Implementation(float InSteeringInput, float InThrottleInput, float InBrakeInput
	, float InHandbrakeInput, int32 InGearChange, float InRollInput, float InPitchInput, float InYawInput, const FAsyncPhysicsTimestamp& AsyncPhysicsTimestamp)
{
	//UE_LOG(LogModularVehicle, Log, TEXT("ApplyFromRPCCallToServer [NetMode %d] %f %f"), GetWorld()->GetNetMode(), RawThrottleInput, RawSteeringInput);
	ApplyControls_Imp(InSteeringInput, InThrottleInput, InBrakeInput, InHandbrakeInput, InGearChange, InRollInput, InPitchInput, InYawInput, AsyncPhysicsTimestamp);
}


void UModularVehicleComponent::ApplyControls_Imp(float InSteeringInput, float InThrottleInput, float InBrakeInput
	, float InHandbrakeInput, int32 InGearChange, float InRollInput, float InPitchInput, float InYawInput, const FAsyncPhysicsTimestamp& AsyncPhysicsTimestamp)
{
	UWorld* World = GetWorld();

	APawn* MyPawn = Cast<APawn>(GetOwner());
	APlayerController* OwningPC = MyPawn ? Cast<APlayerController>(MyPawn->GetController()) : nullptr;

	FAsyncPhysicsTimestamp AsyncPhysicsTimestampShift = AsyncPhysicsTimestamp;

	float ControlInputWakeTolerance = 0.02f;

	const bool bControlInputPressed = ((InThrottleInput >= /*GVehicleDebugParams.*/ControlInputWakeTolerance)
		|| (InBrakeInput >= /*GVehicleDebugParams.*/ControlInputWakeTolerance)
		|| (InRollInput >= /*GVehicleDebugParams.*/ControlInputWakeTolerance)
		|| (InPitchInput >= /*GVehicleDebugParams.*/ControlInputWakeTolerance)
		|| (InYawInput >= /*GVehicleDebugParams.*/ControlInputWakeTolerance)
		|| (FMath::Abs(InSteeringInput - PrevSteeringInput) >= /*GVehicleDebugParams.*/ControlInputWakeTolerance));

	if (bControlInputPressed)
	{
		SetSleeping(false);
	}

	//UE_LOG(LogModularVehicle, Log, TEXT("Apply : Net Mode %s [ T %f  S %f ]"), *NetModeToString(World->GetNetMode()), InThrottleInput, InSteeringInput);

	OwningPC->ExecuteAsyncPhysicsCommand(AsyncPhysicsTimestampShift, this, [this, InSteeringInput, InThrottleInput, InBrakeInput, InHandbrakeInput, InGearChange, InRollInput, InPitchInput, InYawInput, World, AsyncPhysicsTimestampShift]
		{
			Chaos::FControlInputs& ControlInputs = VehicleSimulationPT->AccessControlInputs();
			ControlInputs.Steering = InSteeringInput;
			ControlInputs.Throttle = InThrottleInput;
			ControlInputs.Brake = InBrakeInput;
			ControlInputs.Handbrake = InHandbrakeInput;
			ControlInputs.Roll = InRollInput;
			ControlInputs.Pitch = InPitchInput;
			ControlInputs.Yaw = InYawInput;
			ControlInputs.GearNumber = InGearChange;
			ControlInputs.InputDebugIndex = AsyncPhysicsTimestampShift.LocalFrame;

			// [BH - debug only - try to match controls input on same frame on client and server]
			//{
			//	int32 LocalFrameOffset = -1;		// LocalFrame = ServerFrame + LocalFrameOffset;

			//	{
			//		if (World->GetNetMode() == NM_Client)
			//		{
			//			if (APlayerController* PlayerController = World->GetFirstPlayerController())
			//			{
			//				//if (FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
			//				{
			//					LocalFrameOffset = PlayerController->GetNetworkPhysicsTickOffset();
			//				}
			//			}
			//		}
			//	}

			//	if (LocalFrameOffset >= 0) // client
			//	{
			//		UE_LOG(LogModularVehicle, Log, TEXT("Apply Controls IMP CLIENT Timestamp %d, LocalFrameOffset %d"), AsyncPhysicsTimestampShift.LocalFrame, LocalFrameOffset);
			//	}
			//	else // server
			//	{
			//		UE_LOG(LogModularVehicle, Log, TEXT("Apply Controls IMP SERVER Timestamp %d"), AsyncPhysicsTimestampShift.LocalFrame);
			//	}
			//}
			// TODO: need to figure out how manual gear changing is going to work, need to find correct node to change state of, i.e. multiple gears ?!?
		});
}

void UModularVehicleComponent::SetSleeping(bool bEnableSleep)
{
	// TODO
}

#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif
