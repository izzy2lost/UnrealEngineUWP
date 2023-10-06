// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/NetworkPhysicsComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EngineLogs.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PBDRigidsSolver.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsReplication.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPhysicsComponent)

bool FNetworkPhysicsRewindDataProxy::NetSerializeBase(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, TUniqueFunction<TUniquePtr<Chaos::FBaseRewindHistory>()> CreateHistoryFunction)
{
	Ar << Owner;

	bool bHasData = History.IsValid();
	Ar.SerializeBits(&bHasData, 1);

	if (bHasData)
	{
		if (Ar.IsLoading() && !History.IsValid())
		{
			if(ensureMsgf(Owner, TEXT("FNetRewindDataBase::NetSerialize: owner is null")))
			{
				History = CreateHistoryFunction();
				if (!ensureMsgf(History.IsValid(), TEXT("FNetRewindDataBase::NetSerialize: failed to create history. Owner: %s"), *GetFullNameSafe(Owner)))
				{
					Ar.SetError();
					bOutSuccess = false;
					return true;
				}
			}
			else
			{
				Ar.SetError();
				bOutSuccess = false;
				return true;
			}
		}

		History->NetSerialize(Ar, Map);
	}

	return true;
}

FNetworkPhysicsRewindDataProxy& FNetworkPhysicsRewindDataProxy::operator=(const FNetworkPhysicsRewindDataProxy& Other)
{
	if (&Other != this)
	{
		Owner = Other.Owner;
		History = Other.History ? Other.History->Clone() : nullptr;
	}

	return *this;
}

bool FNetworkPhysicsRewindDataInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedInputs.History->CreateNew(); });
}

bool FNetworkPhysicsRewindDataStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedStates.History->CreateNew(); });
}

// after presimulate internal (asyncinput internal simulation done and the output created)
void FNetworkPhysicsCallback::ApplyCallbacks_Internal(int32 PhysicsStep, const TArray<Chaos::ISimCallbackObject*>& SimCallbackObjects)
{
	QUICK_SCOPE_CYCLE_COUNTER(NetworkPhysicsComponent_ApplyCallbacks_Internal);
	UpdateNetMode();

	if ((NetMode == NM_ListenServer) || (NetMode == NM_DedicatedServer))
	{
		if (FPhysScene_Chaos* Scene = static_cast<FPhysScene_Chaos*>(World->GetPhysicsScene()))
		{
			Scene->PopulateReplicationCache(PhysicsStep);
		}
	}
}

// before presimulate internal (asyncinput internal simulation is not done yet)
void FNetworkPhysicsCallback::ProcessInputs_Internal(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbacks)
{
	PreProcessInputsInternal.Broadcast(PhysicsStep);
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		SimCallbackObject->ProcessInputs_Internal(PhysicsStep);
	}
	PostProcessInputsInternal.Broadcast(PhysicsStep);
}

int32 FNetworkPhysicsCallback::TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted)
{
	int32 ResimFrame = INDEX_NONE;
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		const int32 CallbackFrame = SimCallbackObject->TriggerRewindIfNeeded_Internal(LatestStepCompleted);
		ResimFrame = (ResimFrame == INDEX_NONE) ? CallbackFrame : FMath::Min(CallbackFrame, ResimFrame);
	}

	if (RewindData)
	{
		if (NetMode == NM_Client)
		{
			const int32 ReplicationFrame = RewindData->GetResimFrame();

#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
			UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Replication Frame = %d"), ReplicationFrame);
#endif
			ResimFrame = (ResimFrame == INDEX_NONE) ? ReplicationFrame : (ReplicationFrame == INDEX_NONE) ? ResimFrame : FMath::Min(ReplicationFrame, ResimFrame);
			RewindData->SetResimFrame(INDEX_NONE);
		}

		if (ResimFrame != INDEX_NONE)
		{
			const int32 ValidFrame = RewindData->FindValidResimFrame(ResimFrame);
#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
			UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Resim Frame = %d | Valid Frame = %d"), ResimFrame, ValidFrame);
#endif
			ResimFrame = ValidFrame;
		}
	}
	
	return ResimFrame;
}

void FNetworkPhysicsCallback::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	InjectInputsExternal.Broadcast(PhysicsStep, NumSteps);
}

void FNetworkPhysicsCallback::ProcessInputs_External(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs)
{
	for(const Chaos::FSimCallbackInputAndObject& SimCallbackObject : SimCallbackInputs)
	{
		if (SimCallbackObject.CallbackObject && SimCallbackObject.CallbackObject->HasOption(Chaos::ESimCallbackOptions::Rewind))
		{
			SimCallbackObject.CallbackObject->ProcessInputs_External(PhysicsStep);
		}
	}
}

UNetworkPhysicsSystem::UNetworkPhysicsSystem()
{}

void UNetworkPhysicsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UWorld* World = GetWorld();
	check(World);

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UNetworkPhysicsSystem::OnWorldPostInit);
	}
}

void UNetworkPhysicsSystem::Deinitialize()
{}

void UNetworkPhysicsSystem::OnWorldPostInit(UWorld* World, const UWorld::InitializationValues)
{
	if (World != GetWorld())
	{
		return;
	}

	if(UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if(Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{ 
				if (Solver->GetRewindCallback() == nullptr)
				{
					Solver->SetRewindCallback(MakeUnique<FNetworkPhysicsCallback>(World));
				}
			}
		}
	}
}

UNetworkPhysicsComponent::UNetworkPhysicsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitPhysics();
}

UNetworkPhysicsComponent::UNetworkPhysicsComponent() : Super()
{
	InitPhysics();
}

void UNetworkPhysicsComponent::InitPhysics()
{
	bAutoActivate = true;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	SetAsyncPhysicsTickEnabled(true);

	StatesOffsets.SetNumZeroed(StatesRedundancy + 1);
	InputsOffsets.SetNumZeroed(InputsRedundancy + 1);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		FRepMovement& RepMovement = Pawn->GetReplicatedMovement_Mutable();
		RepMovement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
		RepMovement.RotationQuantizationLevel = ERotatorQuantization::ShortComponents;
		RepMovement.VelocityQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
	}
}

void UNetworkPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction)
				{
					if (Solver->GetRewindData() == nullptr)
					{
						const int32 NumFrames = FMath::Max<int32>(1, UPhysicsSettings::Get()->GetPhysicsHistoryCount());
						Solver->EnableRewindCapture(NumFrames, true);
					}

					if(FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
					{
						SolverCallback->PreProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPreProcessInputsInternal);
						SolverCallback->PostProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPostProcessInputsInternal);
					}
				}
				else
				{
					UE_LOG(LogPhysics, Warning, TEXT("A NetworkPhysicsComponent is trying to set up but 'Project Settings -> Physics -> Physics Prediction' is not enabled. The component might not work as intended."));
				}
			}
		}
	}
}

void UNetworkPhysicsComponent::InitializeComponent()
{
	Super::InitializeComponent();
	if (UWorld* World = GetWorld())
	{
		if (UNetworkPhysicsSystem* NetworkManager = World->GetSubsystem<UNetworkPhysicsSystem>())
		{
			NetworkManager->RegisterNetworkComponent(this);
		}
	}
}

void UNetworkPhysicsComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
	if (UWorld* World = GetWorld())
	{
		if (UNetworkPhysicsSystem* NetworkManager = World->GetSubsystem<UNetworkPhysicsSystem>())
		{
			NetworkManager->UnregisterNetworkComponent(this);
		}
	}
}

void UNetworkPhysicsComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNetworkPhysicsComponent, ReplicatedInputs, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNetworkPhysicsComponent, ReplicatedStates, COND_None, REPNOTIFY_Always);
}

void UNetworkPhysicsComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(NetworkPhysicsComponent_AsyncPhysicsTick);

	Super ::AsyncPhysicsTickComponent(DeltaTime, SimTime);
#if DEBUG_NETWORK_PHYSICS
	if(HasServerWorld() && !HasLocalController() && InputsHistory)
	{
		TArray<int32> LocalFrames, ServerFrames, InputFrames;
		InputsHistory->DebugDatas(*ReplicatedInputs.History, LocalFrames, ServerFrames, InputFrames);

		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | AsyncPhysicsTickComponent | Receiving %d inputs from CLIENT | Component = %s"), LocalFrames.Num(), *GetFullName());
		for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
		{
			UE_LOG(LogTemp, Log, TEXT("		Debugging replicated inputs at local frame = %d | server frame = %d | Component = %s"),
				LocalFrames[FrameIndex], ServerFrames[FrameIndex], *GetFullName());
		}
	}
#endif

	// Record the received states from the server into the history for future use
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if(!PhysScene->GetSolver()->GetEvolution()->IsResimming())
			{
				// Send the inputs across the network
				SendLocalInputsDatas();

				// Send the states across the network
				SendLocalStatesDatas();

				// Advance the NetworkIndex
				InputsIndex = (InputsIndex + 1) % (InputsRedundancy + 1);
				StatesIndex = (StatesIndex + 1) % (StatesRedundancy + 1);
			}
		}
	}
}

void UNetworkPhysicsComponent::SendLocalInputsDatas()
{
	const APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (PlayerController->IsLocalController() && InputsHistory)
	{
		// We just check that the local client to server offset is valid before doing something
		if (HasServerWorld() || PlayerController->GetLocalToServerAsyncPhysicsTickOffsetAssigned())
		{
			const int32 LocalOffset = HasServerWorld() ? 0 : PlayerController->GetLocalToServerAsyncPhysicsTickOffset();
			const int32 NextIndex = (InputsIndex + 1) % (InputsRedundancy + 1);

			// if on server (Listen server) we should send the inputs onto all the clients through repnotify
			ReplicatedInputs.History = InputsHistory->CopyFramesWithOffset(InputsOffsets[NextIndex], InputsOffsets[InputsIndex], LocalOffset);
			
			if (!HasServerWorld())
			{
#if DEBUG_NETWORK_PHYSICS
				FAsyncPhysicsTimestamp Timestamp = PlayerController->GetAsyncPhysicsTimestamp();

				TArray<int32> LocalFrames, ServerFrames, InputFrames;
				InputsHistory->DebugDatas(*ReplicatedInputs.History, LocalFrames, ServerFrames, InputFrames);

				UE_LOG(LogTemp, Log, TEXT("CLIENT | GT | SendLocalInputsDatas | Sending %d inputs from CLIENT | Component = %s"), LocalFrames.Num(), *GetFullName());
				for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
				{
					UE_LOG(LogTemp, Log, TEXT("		Debugging local inputs at local frame = %d | server frame = %d | Current local frame = %d | Current server frame = %d"),
						LocalFrames[FrameIndex], ServerFrames[FrameIndex], Timestamp.LocalFrame, Timestamp.ServerFrame);
				}
#endif

				// if on the client we should first send the replicated inputs onto the server
				// the RPC will then resend them onto all the other clients (except the local ones)
				ServerReceiveInputsDatas(ReplicatedInputs);
			}
		}
	}
}

void UNetworkPhysicsComponent::SendLocalStatesDatas()
{
	if (HasServerWorld() && StatesHistory)
	{
		const int32 NextIndex = (StatesIndex + 1) % (StatesRedundancy + 1);

		// if on server we should send the states onto all the clients through repnotify
		ReplicatedStates.History = StatesHistory->CopyFramesWithOffset(StatesOffsets[NextIndex], StatesOffsets[StatesIndex], 0);
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedStates()
{
	// The replicated states should only be used on the client since the server already have authoritative local ones
	if (!HasServerWorld() && StatesHistory)
	{
		APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}
		const int32 LocalOffset = PlayerController->GetLocalToServerAsyncPhysicsTickOffset();

		// Record the received states from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedStates = MakeShareable(ReplicatedStates.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedStates, LocalOffset]()
				{
					StatesHistory->ReceiveNewDatas(*ReceivedStates, LocalOffset);
#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						StatesHistory->DebugDatas(*ReceivedStates, LocalFrames, ServerFrames, InputFrames);

						UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnRep_SetReplicatedStates | Receiving %d states from SERVER | Local offset = %d | Component = %s "), LocalFrames.Num(), LocalOffset, *GetFullName());
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated states at local frame = %d | server frame = %d | life time = %d | Component = %s"), ServerFrames[FrameIndex] - LocalOffset, ServerFrames[FrameIndex], InputFrames[FrameIndex], *GetFullName());

							if (HasLocalController() && (InputFrames[FrameIndex] != (ServerFrames[FrameIndex] - LocalOffset)))
							{
								UE_LOG(LogTemp, Log, TEXT("		Bad local frame compared to input frame!!!"));
							}
						}
					}
#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedInputs()
{
	// For local controller we should already have correct replicated inputs
	if (!HasLocalController() && !HasServerWorld() && InputsHistory)
	{
		APlayerController* PlayerController = GetPlayerController();
		if(!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}
		const int32 LocalOffset = PlayerController->GetLocalToServerAsyncPhysicsTickOffset();

		// Record the received inputs from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ReplicatedInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedInputs, LocalOffset]()
				{
					InputsHistory->ReceiveNewDatas(*ReceivedInputs, LocalOffset);

#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						InputsHistory->DebugDatas(*ReceivedInputs, LocalFrames, ServerFrames, InputFrames);

						UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnRep_SetReplicatedInputs | Receiving %d inputs from SERVER | Local offset = %d | Component = %s"), LocalFrames.Num(), LocalOffset, *GetFullName());
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated inputs at local frame = %d | server frame = %d | Component = %s"), ServerFrames[FrameIndex] - LocalOffset, ServerFrames[FrameIndex], *GetFullName());
						}
					}
#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::ServerReceiveInputsDatas_Implementation(const FNetworkPhysicsRewindDataInputProxy& ClientInputs)
{
	if(InputsHistory)
	{ 
		// We could probably skip that test since the server RPC is on server
		ensure(HasServerWorld());

		// We could probably skip that test since the server RPC is on server
		ensure(!HasLocalController());

		// Record the received inputs from the client into the history for future use
		ReplicatedInputs.History = ClientInputs.History->Clone();

		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				// Make another copy of the client inputs for the physics thread to consume
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ClientInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedInputs, PhysScene]()
				{
					InputsHistory->ReceiveNewDatas(*ReceivedInputs, 0);

	#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						InputsHistory->DebugDatas(*ReceivedInputs, LocalFrames, ServerFrames, InputFrames);

						const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();

						const int32 EvalOffset = CurrentFrame - InputFrames[InputFrames.Num()-1] + 4;
						UE_LOG(LogTemp, Log, TEXT("SERVER | PT | ServerReceiveInputsDatas | Receiving %d inputs from CLIENT | Inputs frame = %d | Server frame = %d | Eval Offset = %d | Component = %s"), LocalFrames.Num(), InputFrames[InputFrames.Num() - 1], CurrentFrame, EvalOffset, *GetFullName());
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated inputs at local frame = %d | server frame = %d | Solver offset = %d | Component = %s"), 
								LocalFrames[FrameIndex], ServerFrames[FrameIndex], ServerFrames[FrameIndex] - LocalFrames[FrameIndex], *GetFullName());
						}
					}
	#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnPreProcessInputsInternal(const int32 PhysicsStep)
{
#if DEBUG_NETWORK_PHYSICS
	if (HasServerWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if(InputsHistory && StatesHistory && ActorComponent)
	{ 
		bool bIsSolverReset = false;
		bool bIsSolverResim = false;

		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{ 
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bIsSolverResim = Solver->GetEvolution()->IsResimming();
				bIsSolverReset = Solver->GetEvolution()->IsResetting();
			}
		}

		// for the inputs client local ones are ground truth otherwise use the replicated ones coming from the server
		if (!HasLocalController() || bIsSolverResim)
		{
			FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Extracting history inputs at frame %d | Component = %s"), PhysicsStep, *GetFullName());
	#endif
			if (InputsHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsDatas))
			{ 
				PhysicsDatas->ApplyDatas(ActorComponent);
			}
		}

		if (!HasServerWorld() && bIsSolverResim)
		{
			FNetworkPhysicsDatas* PhysicsDatas = StatesDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Extracting history states at frame %d | Component = %s"), PhysicsStep, *GetFullName());
	#endif
			if (StatesHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsDatas, true))
			{
				PhysicsDatas->ApplyDatas(ActorComponent);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnPostProcessInputsInternal(const int32 PhysicsStep)
{
	bool bIsSolverReset = false;
	bool bIsSolverResim = false;
#if DEBUG_NETWORK_PHYSICS
	if (HasServerWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if(InputsHistory && StatesHistory && ActorComponent)
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bIsSolverResim = Solver->GetEvolution()->IsResimming();
				bIsSolverReset = Solver->GetEvolution()->IsResetting();
			}
		}

		// for the inputs client local ones are ground truth otherwise use the replicated ones coming from the server
		if (HasLocalController() && !bIsSolverResim && (InputsDatas != nullptr))
		{
			FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
			PhysicsDatas->ServerFrame = HasServerWorld() ? PhysicsStep : PhysicsStep + GetPlayerController()->GetLocalToServerAsyncPhysicsTickOffset();
			PhysicsDatas->InputFrame = PhysicsStep;

			PhysicsDatas->BuildDatas(ActorComponent);

			InputsOffsets[InputsIndex] = FMath::Max(InputsOffsets[InputsIndex], PhysicsStep + 1);
			InputsHistory->RecordDatas(PhysicsStep, PhysicsDatas);

#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Recording local inputs at frame %d | Component = %s"), PhysicsDatas->LocalFrame, *GetFullName());
#endif
		}

		if (HasServerWorld())
		{
			// Compute of the local frame coming from the client that was used to generate this state
			int32 InputFrame = INDEX_NONE;
			{
				FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
				if(InputsHistory->ExtractDatas(PhysicsStep, false, PhysicsDatas, true))
				{
					InputFrame = PhysicsDatas->InputFrame;
				}
			}

			FNetworkPhysicsDatas* PhysicsDatas = StatesDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
			PhysicsDatas->ServerFrame = PhysicsStep;
			PhysicsDatas->InputFrame = InputFrame;

			PhysicsDatas->BuildDatas(ActorComponent);

			StatesOffsets[StatesIndex] = FMath::Max(StatesOffsets[StatesIndex], PhysicsStep + 1);
			StatesHistory->RecordDatas(PhysicsStep, PhysicsDatas);

#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Recording local states at frame %d | from input frame = %d | Component = %s"), PhysicsDatas->LocalFrame, PhysicsDatas->InputFrame, *GetFullName());
#endif
		}
	}
}

bool UNetworkPhysicsComponent::HasServerWorld() const
{
	return GetWorld()->IsNetMode(NM_DedicatedServer) || GetWorld()->IsNetMode(NM_ListenServer);
}

bool UNetworkPhysicsComponent::HasLocalController() const
{
	if (APlayerController* PlayerController = GetPlayerController())
	{
		return PlayerController->IsLocalController();
	}
	return false;
}

APlayerController* UNetworkPhysicsComponent::GetPlayerController() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController * PC = Pawn->GetController<APlayerController>())
		{
			return PC;
		}

		// In this case the APlayerController can be found as the owner of the pawn
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetOwner()))
		{
			return PC;
		}

	}

	return nullptr;
}

void UNetworkPhysicsComponent::RemoveDatasHistory()
{
	if (GetWorld())
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
				{
					RewindData->RemoveInputsHistory(InputsHistory);
					RewindData->RemoveStatesHistory(StatesHistory);
				}
			}
		}
	}
}
void UNetworkPhysicsComponent::AddDatasHistory()
{
	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
			{
				RewindData->AddInputsHistory(InputsHistory);
				RewindData->AddStatesHistory(StatesHistory);
			}
		}
	}
}







