// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPhysicsLiaison.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "GameFramework/Actor.h"
#include "MovementModeStateMachine.h"
#include "Kinematic/Settings/CommonLegacyMovementSettings.h"
#include "PBDRigidsSolver.h"
#include "Components/ShapeComponent.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"
#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/PhysicsMoverManager.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"

//////////////////////////////////////////////////////////////////////////

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverInputs

void FNetworkPhysicsMoverInputs::ApplyData(UActorComponent* NetworkComponent) const
{
	if (NetworkComponent)
	{
		if (UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->SetCurrentInputData(InputCmdContext);
		}
	}
}

void FNetworkPhysicsMoverInputs::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->GetCurrentInputData(InputCmdContext);
		}
	}
}

bool FNetworkPhysicsMoverInputs::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	if (PackageMap)
	{
		InputCmdContext.NetSerialize(FNetSerializeParams(Ar));
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}
	
	return bOutSuccess;
}

void FNetworkPhysicsMoverInputs::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverInputs& MinDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MinData);
	const FNetworkPhysicsMoverInputs& MaxDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MaxData);

	const float LerpFactor = (LocalFrame - MinDataInput.LocalFrame) / (MaxDataInput.LocalFrame - MinDataInput.LocalFrame);

	const FKinematicDefaultInputs* MinInput = MinDataInput.InputCmdContext.InputCollection.FindDataByType<FKinematicDefaultInputs>();
	const FKinematicDefaultInputs* MaxInput = MaxDataInput.InputCmdContext.InputCollection.FindDataByType<FKinematicDefaultInputs>();

	FKinematicDefaultInputs& LocalInput = InputCmdContext.InputCollection.FindOrAddMutableDataByType<FKinematicDefaultInputs>();

	if (MinInput && MaxInput)
	{
		// Note, this ignores movement base as this is not used by the physics mover
		const FKinematicDefaultInputs* ClosestInputs = LerpFactor < 0.5f ? MinInput : MaxInput;
		LocalInput.bIsJumpJustPressed = ClosestInputs->bIsJumpJustPressed;
		LocalInput.bIsJumpPressed = ClosestInputs->bIsJumpPressed;
		LocalInput.SuggestedMovementMode = ClosestInputs->SuggestedMovementMode;

		LocalInput.SetMoveInput(ClosestInputs->GetMoveInputType(), FMath::Lerp(MinInput->GetMoveInput(), MaxInput->GetMoveInput(), LerpFactor));
		LocalInput.OrientationIntent = FMath::Lerp(MinInput->OrientationIntent, MaxInput->OrientationIntent, LerpFactor);
		LocalInput.ControlRotation = FMath::Lerp(MinInput->ControlRotation, MaxInput->ControlRotation, LerpFactor);

	}
	else if (MinInput)
	{
		LocalInput = *MinInput;
	}
	else if (MaxInput)
	{
		LocalInput = *MaxInput;
	}
}

void FNetworkPhysicsMoverInputs::MergeData(const FNetworkPhysicsData& FromData)
{
	const FNetworkPhysicsMoverInputs& FromDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(FromData);

	if (const FKinematicDefaultInputs* FromInput = FromDataInput.InputCmdContext.InputCollection.FindDataByType<FKinematicDefaultInputs>())
	{
		FKinematicDefaultInputs& LocalInputs = InputCmdContext.InputCollection.FindOrAddMutableDataByType<FKinematicDefaultInputs>();

		LocalInputs.bIsJumpJustPressed |= FromInput->bIsJumpJustPressed;
		LocalInputs.bIsJumpPressed |= FromInput->bIsJumpPressed;
	}
}

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverState

void FNetworkPhysicsMoverState::ApplyData(UActorComponent* NetworkComponent) const
{
	if (NetworkComponent)
	{
		if (UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->SetCurrentStateData(SyncStateContext, AuxStateContext);
		}
	}
}

void FNetworkPhysicsMoverState::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponent* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponent>(NetworkComponent))
		{
			LiaisonComp->GetCurrentStateData(SyncStateContext, AuxStateContext);
		}
	}
}

bool FNetworkPhysicsMoverState::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	FNetworkPhysicsData::SerializeFrames(Ar);

	if (PackageMap)
	{
		FNetSerializeParams Params(Ar);
		SyncStateContext.NetSerialize(Params);
		AuxStateContext.NetSerialize(Params);
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}

	return bOutSuccess;
}

void FNetworkPhysicsMoverState::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverState& MinState = static_cast<const FNetworkPhysicsMoverState&>(MinData);
	const FNetworkPhysicsMoverState& MaxState = static_cast<const FNetworkPhysicsMoverState&>(MaxData);

	const float LerpFactor = (LocalFrame - MinState.LocalFrame) / (MaxState.LocalFrame - MinState.LocalFrame);
	SyncStateContext.Interpolate(&MinState.SyncStateContext, &MaxState.SyncStateContext, LerpFactor);
	AuxStateContext.Interpolate(&MinState.AuxStateContext, &MaxState.AuxStateContext, LerpFactor);
}

//////////////////////////////////////////////////////////////////////////

void UMoverNetworkPhysicsLiaisonComponent::GetCurrentInputData(OUT FMoverInputCmdContext& InputCmd) const
{
	InputCmd = NetInputCmd;
}

void UMoverNetworkPhysicsLiaisonComponent::GetCurrentStateData(OUT FMoverSyncState& SyncState, OUT FMoverAuxStateContext& AuxState) const
{
	SyncState = NetSyncState;
	AuxState = NetAuxState;
}

void UMoverNetworkPhysicsLiaisonComponent::SetCurrentInputData(const FMoverInputCmdContext& InputCmd)
{
	NetInputCmd = InputCmd;
}

void UMoverNetworkPhysicsLiaisonComponent::SetCurrentStateData(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	NetSyncState = SyncState;
	NetAuxState = AuxState;
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent

UMoverNetworkPhysicsLiaisonComponent::UMoverNetworkPhysicsLiaisonComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);

	// Network physics relies on movement being replicated
	if (AActor* MyActor = GetOwner())
	{
		MyActor->SetReplicatingMovement(true);
		MyActor->SetReplicateMovement(true);
	}

	static const FName NetworkPhysicsComponentName(TEXT("PC_NetworkPhysicsComponent"));

	NetworkPhysicsComponent = CreateDefaultSubobject<UNetworkPhysicsComponent, UNetworkPhysicsComponent>(NetworkPhysicsComponentName);
	NetworkPhysicsComponent->SetNetAddressable(); // Make DSO components net addressable
	NetworkPhysicsComponent->SetIsReplicated(true);
}

//////////////////////////////////////////////////////////////////////////
//  UMoverNetworkPhysicsLiaisonComponent IMoverBackendLiaisonInterface

float UMoverNetworkPhysicsLiaisonComponent::GetCurrentSimTimeMs()
{
	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			return Solver->GetAsyncDeltaTime() * GetCurrentSimFrame() * 1000.0f;
		}
	}

	return 0.0f;
}

int32 UMoverNetworkPhysicsLiaisonComponent::GetCurrentSimFrame()
{
	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			int32 Offset = 0;
			if (NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
			{
				if (APlayerController* PC = NetworkPhysicsComponent->GetPlayerController())
				{
					Offset = PC->GetNetworkPhysicsTickOffset();
				}
			}

			return Solver->GetCurrentFrame() + Offset;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent UObject interface

void UMoverNetworkPhysicsLiaisonComponent::OnRegister()
{
	Super::OnRegister();

	if (!NetworkPhysicsComponent->IsRegistered())
	{
		NetworkPhysicsComponent->RegisterComponent();
	}

	// Need to set this here as physics creation requires access to MoverComp
	
	if ( (MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>()) != nullptr )
	{
	
		CommonMovementSettings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		check(CommonMovementSettings);

		if (MoverComp->UpdatedCompAsPrimitive)
		{
			MoverComp->UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &ThisClass::OnComponentPhysicsStateChanged);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnUnregister()
{
	if (MoverComp)
	{
		if (MoverComp->UpdatedCompAsPrimitive)
		{
			MoverComp->UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.RemoveDynamic(this, &ThisClass::OnComponentPhysicsStateChanged);
		}
	}

	Super::OnUnregister();
}

void UMoverNetworkPhysicsLiaisonComponent::SetupConstraint()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
				{
					if (FBodyInstance* BI = MoverComp->UpdatedCompAsPrimitive->GetBodyInstance())
					{
						if (Chaos::FSingleParticlePhysicsProxy* CharacterProxy = BI->ActorHandle)
						{
							// Create and register the constraint
							Constraint = MakeUnique<Chaos::FCharacterGroundConstraint>();
							Constraint->Init(CharacterProxy);
							Solver->RegisterObject(Constraint.Get());

							// Set the common settings from the initial aux data
							// The rest get set every frame depending on the current movement mode
							Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);
							Constraint->SetVerticalAxis(MoverComp->GetUpDirection());

							// Enable Physics Simulation
							MoverComp->UpdatedCompAsPrimitive->SetSimulatePhysics(true);
							
							// Turn off sleeping
							Chaos::FRigidBodyHandle_External& PhysicsBody = CharacterProxy->GetGameThreadAPI();
							PhysicsBody.SetSleepType(Chaos::ESleepType::NeverSleep);
						}
					}
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::DestroyConstraint()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && HasValidPhysicsState())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				// Note: Proxy gets destroyed when the constraint is deregistered and that deletes the constraint
				Solver->UnregisterObject(Constraint.Release());
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (StateChange == EComponentPhysicsStateChange::Destroyed)
	{
		DestroyConstraint();
	}
	else if (StateChange == EComponentPhysicsStateChange::Created)
	{
		SetupConstraint();

		if (MoverComp && MoverComp->ModeFSM)
		{
			MoverComp->ModeFSM->SetModeImmediately(MoverComp->StartingMovementMode);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();

	NetworkPhysicsComponent->InitializeComponent();

	if (ensureAlwaysMsgf(MoverComp, TEXT("UMoverNetworkPhysicsLiaisonComponent on actor %s failed to find associated Mover component. This actor's movement will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		MoverComp->InitMoverSimulation();
		MoverComp->ModeFSM->SetModeImmediately(MoverComp->StartingMovementMode);
	}
}

void UMoverNetworkPhysicsLiaisonComponent::UninitializeComponent()
{
	NetworkPhysicsComponent->UninitializeComponent();
	Super::UninitializeComponent();
}

bool UMoverNetworkPhysicsLiaisonComponent::ShouldCreatePhysicsState() const
{
	if (!IsRegistered() || IsBeingDestroyed())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World->IsGameWorld())
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();

		if (PhysScene && CanCreatePhysics())
		{
			return true;
		}
	}

	return false;
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidPhysicsState() const
{
	return Constraint.IsValid() && Constraint->IsValid();
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidState() const
{
	return HasValidPhysicsState() && MoverComp && MoverComp->UpdatedCompAsPrimitive && MoverComp->UpdatedComponent
		&& MoverComp->ModeFSM->IsValidLowLevel() && MoverComp->SimBlackboard->IsValidLowLevel();
}

void UMoverNetworkPhysicsLiaisonComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	SetupConstraint();

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
				{
					if (FBodyInstance* BI = MoverComp->UpdatedCompAsPrimitive->GetBodyInstance())
					{
						if (Chaos::FSingleParticlePhysicsProxy* CharacterProxy = BI->ActorHandle)
						{
							// Register network data for recording and rewind/resim
							if (NetworkPhysicsComponent)
							{
								NetworkPhysicsComponent->CreateDataHistory<FNetworkPhysicsMoverTraits>(this);
							}

							// Register with the physics mover manager
							if (UPhysicsMoverManager* Manager = World->GetSubsystem<UPhysicsMoverManager>())
							{
								Manager->RegisterPhysicsMoverComponent(this);
							}
						}
					}
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnDestroyPhysicsState()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && HasValidPhysicsState())
	{
		if (NetworkPhysicsComponent)
		{
			NetworkPhysicsComponent->RemoveDataHistory();
		}

		DestroyConstraint();

		if (UPhysicsMoverManager* Manager = World->GetSubsystem<UPhysicsMoverManager>())
		{
			Manager->UnregisterPhysicsMoverComponent(this);
		}
	}
	
	Super::OnDestroyPhysicsState();
}

bool UMoverNetworkPhysicsLiaisonComponent::CanCreatePhysics() const
{
	check(GetOwner());
	FString ActorName = GetOwner()->GetName();

	if (!IsValid(MoverComp->UpdatedComponent))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not set."), *ActorName, *GetPathName());
		return false;
	}

	if (!IsValid(MoverComp->UpdatedCompAsPrimitive))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not a PrimitiveComponent."), *ActorName, *GetPathName());
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

Chaos::FUniqueIdx UMoverNetworkPhysicsLiaisonComponent::GetUniqueIdx() const
{
	if (MoverComp && MoverComp->UpdatedCompAsPrimitive)
	{
		if (FBodyInstance* BI = MoverComp->UpdatedCompAsPrimitive->GetBodyInstance())
		{
			if (FPhysicsActorHandle ActorHandle = BI->ActorHandle)
			{
				return ActorHandle->GetGameThreadAPI().UniqueIdx();
			}
		}
	}

	return Chaos::FUniqueIdx();
}

void UMoverNetworkPhysicsLiaisonComponent::UpdateConstraintSettings(const FMoverAuxStateContext& AuxState)
{
	if (HasValidState())
	{
		Constraint->SetVerticalAxis(MoverComp->GetUpDirection());
		Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);

		const UBaseMovementMode* CurrentMode = MoverComp->ModeFSM->GetCurrentMode();
		if (CurrentMode && CurrentMode->Implements<UPhysicsCharacterMovementModeInterface>())
		{
			const IPhysicsCharacterMovementModeInterface* PhysicsMode = CastChecked<IPhysicsCharacterMovementModeInterface>(CurrentMode);
			PhysicsMode->UpdateConstraintSettings(*Constraint);
		}
	}
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponent::GetCurrentMoverTimeStep() const
{
	FMoverTimeStep TimeStep;

	if (FPhysScene* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			int32 Offset = 0;
			if (NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
			{
				if (APlayerController* PC = NetworkPhysicsComponent->GetPlayerController())
				{
					Offset = PC->GetNetworkPhysicsTickOffset();
				}
			}

			TimeStep.ServerFrame = Solver->GetCurrentFrame() + Offset;
			TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
			TimeStep.BaseSimTimeMs = TimeStep.ServerFrame * TimeStep.StepMs;
			TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
		}
	}

	return TimeStep;
}

void UMoverNetworkPhysicsLiaisonComponent::GameThread_ProduceInput(float DeltaSeconds, OUT FPhysicsMoverAsyncInput& Input)
{
	if (HasValidState())
	{
		Input.MoverIdx = GetUniqueIdx();
		Input.MoverSimulation = this;
		
		// Produce input
		if (!MoverComp || !NetworkPhysicsComponent)
		{
			return;
		}

		if (!NetworkPhysicsComponent->HasServerWorld())
		{
			APlayerController* PlayerController = NetworkPhysicsComponent->GetPlayerController();
			if (PlayerController && PlayerController->IsLocalController())
			{
				if (!bCachedInputIsValid)
				{
					const int DeltaTimeMS = FMath::TruncToInt(DeltaSeconds * 1000.0f);

					MoverComp->ProduceInput(DeltaTimeMS, &Input.InputCmd);

					// We only want to consume one input per physics frame
					// so if there is already a valid cached input we use that.
					// Input is set invalid when the async output is consumed
					bCachedInputIsValid = true;
				}
				else
				{
					Input.InputCmd = MoverComp->CachedLastProducedInputCmd;
				}
			}
		}

		Input.SyncState.SyncStateCollection.Empty();
		if (MoverComp->bHasValidCachedState)
		{
			Input.SyncState = MoverComp->CachedLastSyncState;
			Input.AuxState = MoverComp->CachedLastAuxState;
		}
		else
		{
			FMoverDefaultSyncState& Default = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
			Default.MovementMode = KinematicModeNames::Falling;
			Input.AuxState = FMoverAuxStateContext();
		}

		// This is required so that the physics thread can have a copy of the data to access
		NetInputCmd = Input.InputCmd;
		NetSyncState = Input.SyncState;
		NetAuxState = Input.AuxState;

		if (MoverComp->bHasValidLastProducedInput)
		{
			MoverComp->OnPreSimulationTick.Broadcast(GetCurrentMoverTimeStep(), MoverComp->CachedLastProducedInputCmd);
		}
		else
		{
			MoverComp->OnPreSimulationTick.Broadcast(GetCurrentMoverTimeStep(), FMoverInputCmdContext());
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::GameThread_ConsumeOutput(const FPhysicsMoverAsyncOutput& Output)
{
	if (Output.bIsValid && MoverComp)
	{
		if (const FMoverDefaultSyncState* OutputSyncState = Output.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			// If landed, broadcast OnLanded
			if (MoverComp->bHasValidCachedState)
			{
				if (const FMoverDefaultSyncState* CachedSyncState = MoverComp->CachedLastSyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
				{
					if ((CachedSyncState->MovementMode == KinematicModeNames::Falling) && (OutputSyncState->MovementMode == KinematicModeNames::Walking))
					{
						if (UFallingMode* FallingMode = MoverComp->FindMode_Mutable<UFallingMode>())
						{
							FHitResult HitResult;
							MoverComp->TryGetFloorCheckHitResult(HitResult);
							FallingMode->OnLanded.Broadcast(OutputSyncState->MovementMode, HitResult);
						}
					}
				}
			}

			// TODO: Consider moving to a util function
			MoverComp->CachedLastSyncState = Output.SyncState;
			MoverComp->CachedLastAuxState = Output.AuxState;
			MoverComp->CachedLastSimTickTimeStep.BaseSimTimeMs = GetCurrentSimTimeMs();
			MoverComp->CachedLastSimTickTimeStep.ServerFrame = GetCurrentSimFrame();
			MoverComp->bHasValidCachedState = true;
		}

		FMoverTimeStep TimeStep;
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
				TimeStep.BaseSimTimeMs = Solver->GetSolverTime();
			}
		}
		MoverComp->OnPostSimulationTick.Broadcast(TimeStep);

		bCachedInputIsValid = false;

		UpdateConstraintSettings(Output.AuxState);
	}
}

void UMoverNetworkPhysicsLiaisonComponent::AsyncPhysics_ProcessInputs(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	// Override input data unless player is local client
	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (NetworkPhysicsComponent)
		{
			if (NetworkPhysicsComponent->HasServerWorld())
			{
				// Server remote player
				GetCurrentInputData(Input.InputCmd);
			}
			else
			{
				GetCurrentStateData(Input.SyncState, Input.AuxState);

				bool bIsSolverResim = false;
				if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
				{
					bIsSolverResim = Solver->GetEvolution()->IsResimming();
				}

				bool bLocalPlayer = false;
				APlayerController* PlayerController = NetworkPhysicsComponent->GetPlayerController();
				if (PlayerController && PlayerController->IsLocalController())
				{
					bLocalPlayer = true;
				}

				if (!bLocalPlayer || bIsSolverResim)
				{
					GetCurrentInputData(Input.InputCmd);
				}
			}
		}
		
	}
}

void UMoverNetworkPhysicsLiaisonComponent::AsyncPhysics_OnPreSimulate(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& Output) const
{
	// Sync/aux state should carry over to the next sim frame by default unless something modifies it
	Output.SyncState = Input.SyncState;
	Output.AuxState = Input.AuxState;

	if (!HasValidState() || !Input.InputCmd.InputCollection.FindDataByType<FKinematicDefaultInputs>())
	{
		return;
	}

	// Exit if Physics is not enabled on the CollisionShape
	const UShapeComponent* CollisionShape = Cast<const UShapeComponent>(MoverComp->UpdatedComponent);
	if (!CollisionShape || !CollisionShape->IsSimulatingPhysics())
	{
		return;
	}
	
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
	if (!ConstraintHandle || !ConstraintHandle->IsEnabled() || !ConstraintHandle->GetCharacterParticle())
	{
		return;
	}

	// Update sync state from physics
	Chaos::FPBDRigidParticleHandle* CharacterParticle = ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	if (!CharacterParticle || CharacterParticle->Disabled())
	{
		return;
	}

	FMoverDefaultSyncState& SyncState = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const IPhysicsCharacterMovementModeInterface* PhysicsMode = Cast<const IPhysicsCharacterMovementModeInterface>(MoverComp->ModeFSM->FindMovementMode(SyncState.MovementMode));
	if (!PhysicsMode)
	{
		UE_LOG(LogMover, Verbose, TEXT("Attempting to run non-physics movement mode %s in physics mover update."), *SyncState.MovementMode.ToString());
		return;
	}

	// Make the sync state velocity relative to the ground if walking
	FVector LocalGroundVelocity = FVector::ZeroVector;
	if (SyncState.MovementMode == KinematicModeNames::Walking)
	{
		if (const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard())
		{
			FFloorCheckResult LastFloorResult;
			if (Blackboard->TryGet(KinematicBlackboard::LastFloorResult, LastFloorResult))
			{
				LocalGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(CharacterParticle->X(), LastFloorResult.HitResult, TickParams.DeltaTimeSeconds);
				LocalGroundVelocity -= LocalGroundVelocity.ProjectOnToNormal(LastFloorResult.HitResult.ImpactNormal);
			}
		}
	}
	SyncState.SetTransforms_WorldSpace(CharacterParticle->X(), FRotator(CharacterParticle->R()), CharacterParticle->V() - LocalGroundVelocity);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Update the simulation
	//Input.MoverSimulation->SimulationTick(TickParams, Input.SimInput, SimOutput);

	FMoverTickStartData TickStartData(Input.InputCmd, Input.SyncState, Input.AuxState);
	FMoverTickEndData TickEndData;
	FFloorCheckResult FloorResult;

	FMoverTimeStep TimeStep = GetCurrentMoverTimeStep();

	bool bHasRolledBack = false; // TODO

	//-------------------------------------------------------------------------------------------
	// Copied from KinematicMoverComponent::SimulationTick

	if (bHasRolledBack)
	{
		MoverComp->ProcessFirstSimTickAfterRollback(TimeStep);
	}

	// Sync/aux state should carry over to the next sim frame by default unless something modifies it
	TickEndData.SyncState = TickStartData.SyncState;
	TickEndData.AuxState = TickStartData.AuxState;

	if (MoverComp->ModeFSM->IsValidLowLevel() && MoverComp->SimBlackboard->IsValidLowLevel())
	{
		const FMoverDefaultSyncState* StartingSyncState = TickStartData.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		check(StartingSyncState);

		FKinematicDefaultInputs* InputCmd = TickStartData.InputCmd.InputCollection.FindMutableDataByType<FKinematicDefaultInputs>();
		FMoverDefaultSyncState& OutputSyncState = TickEndData.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		if (InputCmd && !InputCmd->SuggestedMovementMode.IsNone())
		{
			MoverComp->QueueNextMode(InputCmd->SuggestedMovementMode);
		}

		// Tick the actual simulation. This is where the proposed moves are queried and executed, affecting change to the moving actor's gameplay state and captured in the output sim state
		MoverComp->ModeFSM->OnSimulationTick(MoverComp->UpdatedComponent, MoverComp->UpdatedCompAsPrimitive, MoverComp->SimBlackboard.Get(), TickStartData, TimeStep, TickEndData);

		const FName MovementModeAfterTick = MoverComp->ModeFSM->GetCurrentModeName();
		OutputSyncState.MovementMode = MovementModeAfterTick;
	}

	//-------------------------------------------------------------------------------------------

	MoverComp->SimBlackboard->TryGet(KinematicBlackboard::LastFloorResult, Output.FloorResult);

	Output.SyncState = TickEndData.SyncState;
	Output.AuxState = TickEndData.AuxState;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Update physics constraint from output sync state and aux state
	const FMoverDefaultSyncState* OutputSyncState = Output.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!ensure(OutputSyncState))
	{
		return;
	}

	FVector TargetDeltaPos = OutputSyncState->GetLocation_WorldSpace() - CharacterParticle->X();

	if (TargetDeltaPos.SizeSquared2D() > GPhysicsDrivenMotionDebugParams.TeleportThreshold * GPhysicsDrivenMotionDebugParams.TeleportThreshold)
	{
		TeleportParticle(CharacterParticle, OutputSyncState->GetLocation_WorldSpace(), OutputSyncState->GetOrientation_WorldSpace().Quaternion());
	}

	// Add back the ground velocity that was subtracted to but the movement velocity in local space
	FVector TargetVelocity = OutputSyncState->GetVelocity_WorldSpace() + LocalGroundVelocity;

	// Landed so add the new ground velocity
	if ((OutputSyncState->MovementMode == KinematicModeNames::Walking) && (SyncState.MovementMode != KinematicModeNames::Walking))
	{
		if (const UPhysicsDrivenWalkingMode* WalkingMode = Cast<UPhysicsDrivenWalkingMode>(MoverComp->FindMovementMode(UPhysicsDrivenWalkingMode::StaticClass())))
		{
			LocalGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(CharacterParticle->X(), Output.FloorResult.HitResult, TickParams.DeltaTimeSeconds);
			LocalGroundVelocity -= LocalGroundVelocity.ProjectOnToNormal(Output.FloorResult.HitResult.ImpactNormal);
			TargetVelocity += WalkingMode->FractionalVelocityToTarget * LocalGroundVelocity;
		}
	}

	CharacterParticle->SetV(TargetVelocity);

	// Note: Output sync state does not have a target angular velocity so
	// use the target orientation
	FRotator DeltaRotation = OutputSyncState->GetOrientation_WorldSpace() - FRotator(CharacterParticle->R());
	FRotator Winding, Remainder;
	DeltaRotation.GetWindingAndRemainder(Winding, Remainder);
	float TargetDeltaFacing = FMath::DegreesToRadians(Remainder.Yaw);
	if (TickParams.DeltaTimeSeconds > UE_SMALL_NUMBER)
	{
		CharacterParticle->SetW((TargetDeltaFacing / TickParams.DeltaTimeSeconds) * Chaos::FVec3::ZAxisVector);
	}

	// Update the constraint data based on the floor result
	if (Output.FloorResult.bBlockingHit)
	{
		// Set the ground particle on the constraint
		Chaos::FGeometryParticleHandle* GroundParticle = nullptr;

		if (IPhysicsComponent* PhysicsComp = Cast<IPhysicsComponent>(Output.FloorResult.HitResult.Component))
		{
			if (Chaos::FPhysicsObjectHandle PhysicsObject = PhysicsComp->GetPhysicsObjectById(Output.FloorResult.HitResult.Item))
			{
				Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
				GroundParticle = Interface.GetParticle(PhysicsObject);

				// Wake the ground particle if it is sleeping
				WakeParticleIfSleeping(GroundParticle);
			}
		}
		ConstraintHandle->SetGroundParticle(GroundParticle);

		// Set the max walkable slope angle using any override from the hit component
		float WalkableSlopeCosine = ConstraintHandle->GetSettings().CosMaxWalkableSlopeAngle;
		if (Output.FloorResult.HitResult.Component != nullptr)
		{
			const FWalkableSlopeOverride& SlopeOverride = Output.FloorResult.HitResult.Component->GetWalkableSlopeOverride();
			WalkableSlopeCosine = SlopeOverride.ModifyWalkableFloorZ(WalkableSlopeCosine);
		}

		if (!Output.FloorResult.bWalkableFloor)
		{
			WalkableSlopeCosine = 2.0f;
		}

		ConstraintHandle->SetData({
			Output.FloorResult.HitResult.ImpactNormal,
			TargetDeltaPos,
			TargetDeltaFacing,
			Output.FloorResult.FloorDist,
			WalkableSlopeCosine
			});
	}
	else
	{
		ConstraintHandle->SetData({
			Chaos::FVec3::ZAxisVector,
			Chaos::FVec3::ZeroVector,
			0.0,
			1.0e10,
			0.5f
			});
	}

	Output.bIsValid = true;
}

void UMoverNetworkPhysicsLiaisonComponent::TeleportParticle(Chaos::FGeometryParticleHandle* Particle, const FVector& Position, const FQuat& Rotation) const
{
	if (!Particle)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				if (Solver->GetEvolution())
				{
					Solver->GetEvolution()->SetParticleTransform(Particle, Position, Rotation, true);
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::WakeParticleIfSleeping(Chaos::FGeometryParticleHandle* Particle) const
{
	if (Particle)
	{
		Chaos::FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle();
		if (Rigid && (Rigid->ObjectState() == Chaos::EObjectStateType::Sleeping))
		{
			if (UWorld* World = GetWorld())
			{
				if (FPhysScene* Scene = World->GetPhysicsScene())
				{
					if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
					{
						if (Solver->GetEvolution())
						{
							Solver->GetEvolution()->SetParticleObjectState(Rigid, Chaos::EObjectStateType::Dynamic);
						}
					}
				}
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::ASyncPhysics_OnContactModification(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const
{
	if (!HasValidState())
	{
		return;
	}

	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
	if (!ConstraintHandle || !ConstraintHandle->IsEnabled() || !ConstraintHandle->GetCharacterParticle())
	{
		return;
	}

	Chaos::FPBDRigidParticleHandle* CharacterParticle = ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	if (!CharacterParticle || CharacterParticle->Disabled())
	{
		return;
	}

	const Chaos::FGeometryParticleHandle* GroundParticle = ConstraintHandle->GetGroundParticle();
	if (!GroundParticle)
	{
		return;
	}

	if (const FMoverDefaultSyncState* SyncState = Input.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		float PawnHalfHeight;
		float PawnRadius;
		MoverComp->UpdatedComponent->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

		const float CharacterHeight = CharacterParticle->X().Z;
		const float EndCapHeight = CharacterHeight - PawnHalfHeight + PawnRadius;

		const float CosThetaMax = 0.97f;

		float MinContactHeightStepUps = CharacterHeight + 1.0e10f;
		if (SyncState->MovementMode == KinematicModeNames::Walking)
		{
			if (const UPhysicsDrivenWalkingMode* WalkingMode = Cast<UPhysicsDrivenWalkingMode>(MoverComp->FindMode_Mutable<UPhysicsDrivenWalkingMode>()))
			{
				// Contacts on the character capsule below the MaxStepHeight tend to snag the character when stepping
				// up or down, so disable them

				if (const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>())
				{
					const float StepDistance = FMath::Abs(WalkingMode->TargetHeight - ConstraintHandle->GetData().GroundDistance);
					if (StepDistance >= GPhysicsDrivenMotionDebugParams.MinStepUpDistance)
					{
						MinContactHeightStepUps = CharacterHeight - WalkingMode->TargetHeight + Settings->MaxStepHeight;
					}
				}
			}
		}

		for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(CharacterParticle))
		{
			const int32 CharacterIdx = CharacterParticle == PairModifier.GetParticlePair()[0] ? 0 : 1;
			const int32 OtherIdx = CharacterIdx == 0 ? 1 : 0;

			if (GroundParticle == PairModifier.GetParticlePair()[OtherIdx])
			{
				for (int32 Idx = 0; Idx < PairModifier.GetNumContacts(); ++Idx)
				{
					Chaos::FVec3 Point0, Point1;
					PairModifier.GetWorldContactLocations(Idx, Point0, Point1);
					Chaos::FVec3 CharacterPoint = CharacterIdx == 0 ? Point0 : Point1;

					Chaos::FVec3 ContactNormal = PairModifier.GetWorldNormal(Idx);
					if ((ContactNormal.Z > CosThetaMax) && CharacterPoint.Z < EndCapHeight)
					{
						// Disable any nearly vertical contact with the end cap of the capsule
						// This will be handled by the character ground constraint
						PairModifier.SetContactPointDisabled(Idx);
					}
					else if (CharacterPoint.Z < MinContactHeightStepUps)
					{
						// In the case of steps ups disable all contacts below the max step height
						PairModifier.SetContactPointDisabled(Idx);
					}
				}
			}
		}
	}
}
