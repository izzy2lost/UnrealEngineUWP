// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPredictionLiaison.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"

// ----------------------------------------------------------------------------------------------------------
//	FMoverActorModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FMoverActorModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = UMoverNetworkPredictionLiaisonComponent;
	using StateTypes = KinematicMoverStateTypes;
	using Driver = UMoverNetworkPredictionLiaisonComponent;

	static const TCHAR* GetName() { return TEXT("MoverActor"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers; }
};

NP_MODEL_REGISTER(FMoverActorModelDef);



UMoverNetworkPredictionLiaisonComponent::UMoverNetworkPredictionLiaisonComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);
}

void UMoverNetworkPredictionLiaisonComponent::ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd)
{
	check(MoverComp);
	MoverComp->ProduceInput(DeltaTimeMS, Cmd);
}

void UMoverNetworkPredictionLiaisonComponent::RestoreFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	check(MoverComp);
	MoverComp->RestoreFrame(SyncState, AuxState);
}

void UMoverNetworkPredictionLiaisonComponent::FinalizeFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	check(MoverComp);
	MoverComp->FinalizeFrame(SyncState, AuxState);
}

void UMoverNetworkPredictionLiaisonComponent::InitializeSimulationState(FMoverSyncState* OutSync, FMoverAuxStateContext* OutAux)
{
	check(MoverComp);
	StartingOutSync = OutSync;
	StartingOutAux = OutAux;
}

void UMoverNetworkPredictionLiaisonComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<KinematicMoverStateTypes>& SimInput, const TNetSimOutput<KinematicMoverStateTypes>& SimOutput)
{
	check(MoverComp);

	FMoverTickStartData StartData;
	FMoverTickEndData EndData;

	StartData.InputCmd  = *SimInput.Cmd;
	StartData.SyncState = *SimInput.Sync;
	StartData.AuxState  = *SimInput.Aux;

	MoverComp->SimulationTick(FMoverTimeStep(TimeStep), StartData, OUT EndData);

	*SimOutput.Sync = EndData.SyncState;
    *SimOutput.Aux.Get() = EndData.AuxState;
}


float UMoverNetworkPredictionLiaisonComponent::GetCurrentSimTimeMs()
{
	return NetworkPredictionProxy.GetTotalSimTimeMS();
}

int32 UMoverNetworkPredictionLiaisonComponent::GetCurrentSimFrame()
{
	return NetworkPredictionProxy.GetPendingFrame();
}


bool UMoverNetworkPredictionLiaisonComponent::ReadPendingSyncState(OUT FMoverSyncState& OutSyncState)
{
	if (const FMoverSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FMoverSyncState>())
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}

bool UMoverNetworkPredictionLiaisonComponent::WritePendingSyncState(const FMoverSyncState& SyncStateToWrite)
{
	NetworkPredictionProxy.WriteSyncState<FMoverSyncState>([&SyncStateToWrite](FMoverSyncState& PendingSyncStateRef)
		{
			PendingSyncStateRef = SyncStateToWrite;
		});

	return true;
}

void UMoverNetworkPredictionLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();
	if (StartingOutSync && StartingOutAux)
	{
		MoverComp->InitializeSimulationState(StartingOutSync, StartingOutAux);
		StartingOutSync = nullptr;
		StartingOutAux = nullptr;
	}
}


void UMoverNetworkPredictionLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UMoverNetworkPredictionLiaisonComponent::OnRegister()
{
	Super::OnRegister();
}


void UMoverNetworkPredictionLiaisonComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
}

void UMoverNetworkPredictionLiaisonComponent::InitializeNetworkPredictionProxy()
{
	MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>();


	if (ensureAlwaysMsgf(MoverComp, TEXT("UMoverNetworkPredictionLiaisonComponent on actor %s failed to find associated Mover component. This actor's movement will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		MoverComp->InitMoverSimulation();

		NetworkPredictionProxy.Init<FMoverActorModelDef>(GetWorld(), GetReplicationProxies(), this, this);
	}
}