// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/NetworkPhysicsComponent.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineLogs.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PBDRigidsSolver.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Net/Core/PushModel/PushModel.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#endif // UE_WITH_IRIS

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPhysicsComponent)

namespace PhysicsReplicationCVars
{
	namespace ResimulationCVars
	{
		bool bAllowRewindToClosestState = true;
		static FAutoConsoleVariableRef CVarResimAllowRewindToClosestState(TEXT("np2.Resim.AllowRewindToClosestState"), bAllowRewindToClosestState, TEXT("When rewinding to a specific frame, if the client doens't have state data for that frame, use closest data available. Only affects the first rewind frame, when FPBDRigidsEvolution is set to Reset."));
		bool bCompareStateToTriggerRewind = false;
		static FAutoConsoleVariableRef CVarResimCompareStateToTriggerRewind(TEXT("np2.Resim.CompareStateToTriggerRewind"), bCompareStateToTriggerRewind, TEXT("When true, cache local players custom state struct in rewind history and compare the predicted state with incoming server state to trigger resimulations if they differ, comparison done through FNetworkPhysicsData::CompareData"));
		bool bCompareInputToTriggerRewind = false;
		static FAutoConsoleVariableRef CVarResimCompareInputToTriggerRewind(TEXT("np2.Resim.CompareInputToTriggerRewind"), bCompareInputToTriggerRewind, TEXT("When true, compare local players predicted inputs with incoming server inputs to trigger resimulations if they differ, comparison done through FNetworkPhysicsData::CompareData."));
		bool bEnableUnreliableFlow = true;
		static FAutoConsoleVariableRef CVarResimEnableUnreliableFlow(TEXT("np2.Resim.EnableUnreliableFlow"), bEnableUnreliableFlow, TEXT("When true, allow data to be sent unreliably. Also sends FNetworkPhysicsData not marked with FNetworkPhysicsData::bimportant unreliably over the network."));
		bool bEnableReliableFlow = false;
		static FAutoConsoleVariableRef CVarResimEnableReliableFlow(TEXT("np2.Resim.EnableReliableFlow"), bEnableReliableFlow, TEXT("EXPERIMENTAL -- When true, allow data to be sent reliably. Also send FNetworkPhysicsData marked with FNetworkPhysicsData::bimportant reliably over the network."));
		bool bApplyDataInsteadOfMergeData = false;
		static FAutoConsoleVariableRef CVarResimApplyDataInsteadOfMergeData(TEXT("np2.Resim.ApplyDataInsteadOfMergeData"), bApplyDataInsteadOfMergeData, TEXT("When true, call ApplyData for each data instead of MergeData when having to use multiple data entries in one frame."));
		bool bAllowInputExtrapolation = true;
		static FAutoConsoleVariableRef CVarResimAllowInputExtrapolation(TEXT("np2.Resim.AllowInputExtrapolation"), bAllowInputExtrapolation, TEXT("When true and not locally controlled, allow inputs to be extrapolated from last known and if there is a gap allow interpolation between two known inputs."));
	}
}

/** These CVars are deprecated from UE 5.4, physics frame offset for networked physics prediction is now handled via PlayerController with automatic time dilation
* p.net.CmdOffsetEnabled = 0 is recommended to disable the deprecated flow */
namespace InputCmdCVars
{
	static bool bCmdOffsetEnabled = true;
	static FAutoConsoleVariableRef CVarCmdOffsetEnabled(TEXT("p.net.CmdOffsetEnabled"), bCmdOffsetEnabled, TEXT("Enables deprecated (5.4) logic for legacy that handles physics frame offset. Recommended: Set this to 0 to stop the deprecated physics frame offset flow. "));

	static int32 ForceFault = 0;
	static FAutoConsoleVariableRef CVarForceFault(TEXT("p.net.ForceFault"), ForceFault, TEXT("Forces server side input fault"));
	static int32 MaxBufferedCmds = 16;
	static FAutoConsoleVariableRef CVarMaxBufferedCmds(TEXT("p.net.MaxBufferedCmds"), MaxBufferedCmds, TEXT("MaxNumber of buffered server side commands"));
	static int32 TimeDilationEnabled = 0;
	static FAutoConsoleVariableRef CVarTimeDilationEnabled(TEXT("p.net.TimeDilationEnabled"), TimeDilationEnabled, TEXT("Enable clientside TimeDilation"));
	static float MaxTargetNumBufferedCmds = 5.0;
	static FAutoConsoleVariableRef CVarMaxTargetNumBufferedCmds(TEXT("p.net.MaxTargetNumBufferedCmds"), MaxTargetNumBufferedCmds, TEXT("Maximum number of buffered inputs the server will target per client."));
	static float MaxTimeDilationMag = 0.01f;
	static FAutoConsoleVariableRef CVarMaxTimeDilationMag(TEXT("p.net.MaxTimeDilationMag"), MaxTimeDilationMag, TEXT("Maximum time dilation that client will use to slow down / catch up with server"));
	static float TimeDilationAlpha = 0.1f;
	static FAutoConsoleVariableRef CVarTimeDilationAlpha(TEXT("p.net.TimeDilationAlpha"), TimeDilationAlpha, TEXT("Lerp strength for sliding client time dilation"));
	static float TargetNumBufferedCmdsDeltaOnFault = 1.0f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmdsDeltaOnFault(TEXT("p.net.TargetNumBufferedCmdsDeltaOnFault"), TargetNumBufferedCmdsDeltaOnFault, TEXT("How much to increase TargetNumBufferedCmds when an input fault occurs"));
	static float TargetNumBufferedCmds = 1.9f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmds(TEXT("p.net.TargetNumBufferedCmds"), TargetNumBufferedCmds, TEXT("How much to increase TargetNumBufferedCmds when an input fault occurs"));
	static float TargetNumBufferedCmdsAlpha = 0.005f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmdsAlpha(TEXT("p.net.TargetNumBufferedCmdsAlpha"), TargetNumBufferedCmdsAlpha, TEXT("Lerp strength for TargetNumBufferedCmds"));
	static int32 LerpTargetNumBufferedCmdsAggresively = 0;
	static FAutoConsoleVariableRef CVarLerpTargetNumBufferedCmdsAggresively(TEXT("p.net.LerpTargetNumBufferedCmdsAggresively"), LerpTargetNumBufferedCmdsAggresively, TEXT("Aggresively lerp towards TargetNumBufferedCmds. Reduces server side buffering but can cause more artifacts."));
}
// --------------------------------------------------------------------------------------------------------------------------------------------------
//	Client InputCmd Stream stuff
// --------------------------------------------------------------------------------------------------------------------------------------------------
namespace
{
	int8 QuantizeTimeDilation(float F)
	{
		if (F == 1.f)
		{
			return 0;
		}
		float Normalized = FMath::Clamp<float>((F - 1.f) / InputCmdCVars::MaxTimeDilationMag, -1.f, 1.f);
		return (int8)(Normalized * 128.f);
	}

	float DeQuantizeTimeDilation(int8 i)
	{
		if (i == 0)
		{
			return 1.f;
		}
		float Normalized = (float)i / 128.f;
		float Uncompressed = 1.f + (Normalized * InputCmdCVars::MaxTimeDilationMag);
		return Uncompressed;
	}
}

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

#if UE_WITH_IRIS
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataStateProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataImportantInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataImportantStateProxy);
#endif // UE_WITH_IRIS

bool FNetworkPhysicsRewindDataInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedInputs.History->CreateNew(); });
}

bool FNetworkPhysicsRewindDataStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedStates.History->CreateNew(); });
}

bool FNetworkPhysicsRewindDataImportantInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedImportantInput.History->CreateNew(); });
}

bool FNetworkPhysicsRewindDataImportantStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	return NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->ReplicatedImportantState.History->CreateNew(); });
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

void FNetworkPhysicsCallback::PreResimStep_Internal(int32 PhysicsStep, bool bFirst)
{
	if (bFirst)
	{
		for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
		{
			SimCallbackObject->FirstPreResimStep_Internal(PhysicsStep);
		}
	}
}

void FNetworkPhysicsCallback::PostResimStep_Internal(int32 PhysicsStep)
{

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
			UE_LOG(LogChaos, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Replication Frame = %d"), ReplicationFrame);
#endif
			ResimFrame = (ResimFrame == INDEX_NONE) ? ReplicationFrame : (ReplicationFrame == INDEX_NONE) ? ResimFrame : FMath::Min(ReplicationFrame, ResimFrame);
		}

		if (ResimFrame != INDEX_NONE)
		{
			const int32 ValidFrame = RewindData->FindValidResimFrame(ResimFrame);
#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
			UE_LOG(LogChaos, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Resim Frame = %d | Valid Frame = %d"), ResimFrame, ValidFrame);
#endif
			ResimFrame = ValidFrame;
		}
	}
	
	return ResimFrame;
}

/* Deprecated 5.4 */
void FNetworkPhysicsCallback::UpdateClientPlayer_External(int32 PhysicsStep)
{
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// ------------------------------------------------
		// Send RPC to server telling them what (client/local) physics step we are running
		//	* Note that SendData is empty because of the existing API, should change this
		// ------------------------------------------------	
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArray<uint8> SendData;
		PC->PushClientInput(PhysicsStep, SendData);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (InputCmdCVars::TimeDilationEnabled > 0)
		{
			UE_LOG(LogChaos, Warning, TEXT("p.net.TimeDilationEnabled is set to true, this CVar is deprecated in UE5.4 and does not affect Time Dilation. Time Dilation is automatically used via the PlayerController if Physics Prediction is enabled in Project Settings. It's also recommended to disable the legacy flow that handled physics frame offset and this time dilation by setting: p.net.CmdOffsetEnabled = 0"));
		}
	}
}

/* Deprecated 5.4 */
void FNetworkPhysicsCallback::UpdateServerPlayer_External(int32 PhysicsStep)
{
	// -----------------------------------------------
	// Server: "consume" an InputCmd from each Player Controller
	// All this means in this context is updating FServerFrameInfo:: LastProcessedInputFrame, LastLocalFrame
	// (E.g, telling each client what "Input" of theirs we were processing and our local physics frame number.
	// In cases where the buffer has a fault, we calculate a suggested time dilation to temporarily make client speed up 
	// or slow down their input cmd production.
	// -----------------------------------------------
	const bool bForceFault = InputCmdCVars::ForceFault > 0;
	InputCmdCVars::ForceFault = FMath::Max(0, InputCmdCVars::ForceFault - 1);
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (APlayerController* PC = Iterator->Get())
		{
			PC->UpdateServerTimestampToCorrect();

			APlayerController::FServerFrameInfo& FrameInfo = PC->GetServerFrameInfo();
			APlayerController::FInputCmdBuffer& InputBuffer = PC->GetInputBuffer();

			const int32 NumBufferedInputCmds = bForceFault ? 0 : (InputBuffer.HeadFrame() - FrameInfo.LastProcessedInputFrame);
			// Check Overflow
			if (NumBufferedInputCmds > InputCmdCVars::MaxBufferedCmds)
			{
				UE_LOG(LogChaos, Warning, TEXT("[Remote.Input] overflow %d %d -> %d"), InputBuffer.HeadFrame(), FrameInfo.LastProcessedInputFrame, NumBufferedInputCmds);
				FrameInfo.LastProcessedInputFrame = InputBuffer.HeadFrame() - InputCmdCVars::MaxBufferedCmds + 1;
			}
			// Check fault - we are waiting for Cmds to reach TargetNumBufferedCmds before continuing
			if (FrameInfo.bFault)
			{
				if (NumBufferedInputCmds < (int32)FrameInfo.TargetNumBufferedCmds)
				{
					// Skip this because it is in fault. We will use the prev input for this frame.
					UE_CLOG(FrameInfo.LastProcessedInputFrame != INDEX_NONE, LogPhysics, Warning, TEXT("[Remote.Input] in fault. Reusing Inputcmd. (Client) Input: %d. (Server) Local Frame: %d"), FrameInfo.LastProcessedInputFrame, FrameInfo.LastLocalFrame);
					continue;
				}
				FrameInfo.bFault = false;
			}
			else if (NumBufferedInputCmds <= 0)
			{
				// No Cmds to process, enter fault state. Increment TargetNumBufferedCmds each time this happens.
				// TODO: We should have something to bring this back down (which means skipping frames) we don't want temporary poor conditions to cause permanent high input buffering
				FrameInfo.bFault = true;
				FrameInfo.TargetNumBufferedCmds = FMath::Min(FrameInfo.TargetNumBufferedCmds + InputCmdCVars::TargetNumBufferedCmdsDeltaOnFault, InputCmdCVars::MaxTargetNumBufferedCmds);
				UE_CLOG(FrameInfo.LastProcessedInputFrame != INDEX_NONE, LogPhysics, Warning, TEXT("[Remote.Input] ENTERING fault. New Target: %.2f. (Client) Input: %d. (Server) Local Frame: %d"), FrameInfo.TargetNumBufferedCmds, FrameInfo.LastProcessedInputFrame, FrameInfo.LastLocalFrame);
				continue;
			}
			float TargetTimeDilation = 1.f;
			if (NumBufferedInputCmds < (int32)FrameInfo.TargetNumBufferedCmds)
			{
				TargetTimeDilation += InputCmdCVars::MaxTimeDilationMag; // Tell client to speed up, we are starved on cmds
			}
			FrameInfo.TargetTimeDilation = FMath::Lerp(FrameInfo.TargetTimeDilation, TargetTimeDilation, InputCmdCVars::TimeDilationAlpha);
				
			FrameInfo.QuantizedTimeDilation = QuantizeTimeDilation(TargetTimeDilation);
				
			if (InputCmdCVars::LerpTargetNumBufferedCmdsAggresively != 0)
			{
				// When aggressive, always lerp towards target
				FrameInfo.TargetNumBufferedCmds = FMath::Lerp(FrameInfo.TargetNumBufferedCmds, InputCmdCVars::TargetNumBufferedCmds, InputCmdCVars::TargetNumBufferedCmdsAlpha);
			}
			FrameInfo.LastProcessedInputFrame++;
			FrameInfo.LastLocalFrame = PhysicsStep;
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
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
	
	/* Deprecated 5.4 */
	if (InputCmdCVars::bCmdOffsetEnabled)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (NetMode == NM_Client)
		{
			UpdateClientPlayer_External(PhysicsStep);
		}
		else
		{
			UpdateServerPlayer_External(PhysicsStep);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

	if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{ 
				if (Solver->GetRewindCallback() == nullptr)
				{
					Solver->SetRewindCallback(MakeUnique<FNetworkPhysicsCallback>(World));
				}

				if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsResimulation)
				{
					// Enable RewindData from having bEnablePhysicsResimulation set
					if (Solver->GetRewindData() == nullptr)
					{
						const int32 NumFrames = UPhysicsSettings::Get()->GetPhysicsHistoryCount();
						Solver->EnableRewindCapture(NumFrames, true);
					}
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
	if (const IConsoleVariable* CVarCompareInputToTriggerRewind = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.CompareInputToTriggerRewind")))
	{
		bCompareInputToTriggerRewind = CVarCompareInputToTriggerRewind->GetBool();
	}
	if (const IConsoleVariable* CVarCompareStateToTriggerRewind = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.CompareStateToTriggerRewind")))
	{
		bCompareStateToTriggerRewind = CVarCompareStateToTriggerRewind->GetBool();
	}

	AActor* Owner = GetOwner();
	if (Owner)
	{
		FRepMovement& RepMovement = Owner->GetReplicatedMovement_Mutable();
		RepMovement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
		RepMovement.RotationQuantizationLevel = ERotatorQuantization::ShortComponents;
		RepMovement.VelocityQuantizationLevel = EVectorQuantization::RoundTwoDecimals;

		if (UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			RootPhysicsObject = RootPrimComp->GetPhysicsObjectByName(NAME_None);
		}
	}

	bAutoActivate = true;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	SetAsyncPhysicsTickEnabled(true);
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
					SetupRewindData();

					if(FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
					{
						SolverCallback->PreProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPreProcessInputsInternal);
						SolverCallback->PostProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPostProcessInputsInternal);
					}
				}
				else
				{
					UE_LOG(LogChaos, Warning, TEXT("A NetworkPhysicsComponent is trying to set up but 'Project Settings -> Physics -> Physics Prediction' is not enabled. The component might not work as intended."));
				}
			}
		}
	}
}

void UNetworkPhysicsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Cache CVar values
	bEnableUnreliableFlow = PhysicsReplicationCVars::ResimulationCVars::bEnableUnreliableFlow;
	bEnableReliableFlow = PhysicsReplicationCVars::ResimulationCVars::bEnableReliableFlow;
	bApplyDataInsteadOfMergeData = PhysicsReplicationCVars::ResimulationCVars::bApplyDataInsteadOfMergeData;
	bAllowInputExtrapolation = PhysicsReplicationCVars::ResimulationCVars::bAllowInputExtrapolation;

	if (AActor* Owner = GetOwner())
	{
		// Get settings from NetworkPhysicsSettingsComponent, if there is one
		if (UNetworkPhysicsSettingsComponent* PhysicsSettings = Owner->FindComponentByClass<UNetworkPhysicsSettingsComponent>())
		{
			InputRedundancy = PhysicsSettings->NetworkPhysicsComponentSettings.GetRedundantInputs(InputRedundancy);
			StateRedundancy = PhysicsSettings->NetworkPhysicsComponentSettings.GetRedundantStates(StateRedundancy);
			bCompareInputToTriggerRewind = PhysicsSettings->NetworkPhysicsComponentSettings.GetCompareInputToTriggerRewind(bCompareInputToTriggerRewind);
			bCompareStateToTriggerRewind = PhysicsSettings->NetworkPhysicsComponentSettings.GetCompareStateToTriggerRewind(bCompareStateToTriggerRewind);
			bEnableUnreliableFlow = PhysicsSettings->NetworkPhysicsComponentSettings.GetEnableUnreliableFlow();
			bEnableReliableFlow = PhysicsSettings->NetworkPhysicsComponentSettings.GetEnableReliableFlow();
			bApplyDataInsteadOfMergeData = PhysicsSettings->NetworkPhysicsComponentSettings.GetApplyDataInsteadOfMergeData();
			bAllowInputExtrapolation = PhysicsSettings->NetworkPhysicsComponentSettings.GetAllowInputExtrapolation();

			if (ReplicatedInputs.History)
			{
				ReplicatedInputs.History->ResizeDataHistory(InputRedundancy);
			}
			if (ReplicatedStates.History)
			{
				ReplicatedStates.History->ResizeDataHistory(StateRedundancy);
			}
		}
	}

	// TEMP: Stops ensure that happens if first ever network broadcast for the actor is a multicast from PT (with GT frozen), this instead makes the first multicast happen in GT.
	// ToDo, remove this when NetworkPhysicComponent has a proper Async ISimCallback flow.
	if (bEnableReliableFlow && ReplicatedImportantInput.Owner != nullptr)
	{
		MulticastReceiveImportantInputData(ReplicatedImportantInput);
	}

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

	FDoRepLifetimeParams ReplicatedParams;
	ReplicatedParams.Condition = COND_None;
	ReplicatedParams.RepNotifyCondition = REPNOTIFY_Always; //REPNOTIFY_OnChanged;
	ReplicatedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedInputs, ReplicatedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedStates, ReplicatedParams);
}

void UNetworkPhysicsComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(NetworkPhysicsComponent_AsyncPhysicsTick);

	Super ::AsyncPhysicsTickComponent(DeltaTime, SimTime);

	// Record the received states from the server into the history for future use
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if(!PhysScene->GetSolver()->GetEvolution()->IsResimming())
			{
				// Replicate inputs across the network
				SendInputData();

				// Replicate states across the network
				SendStateData();
			}
		}
	}
}

void UNetworkPhysicsComponent::SendInputData()
{
	const bool bIsServer = HasServerWorld();
	// Inputs are sent from the server or locally controlled actors/pawns
	if (InputHistory && (IsLocallyControlled() || bIsServer))
	{
		const APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}

		// Check that client and server have synced the physics tick offset
		if (PlayerController && PlayerController->GetNetworkPhysicsTickOffsetAssigned())
		{
			// Send latest N frames from history
			const int32 ToFrame = InputHistory->GetLatestFrame();

			// -- Default / Unreliable Flow --
			if (bEnableUnreliableFlow)
			{
				const int32 FromFrame = ToFrame - InputRedundancy;
				if (InputHistory->CopyData(*ReplicatedInputs.History, FromFrame, ToFrame, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false))
				{
					if (bIsServer)
					{
						// Server sends inputs through property replication
						MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedInputs, this);
					}
					else
					{
						// Clients send inputs through an RPC to the server
						ServerReceiveInputData(ReplicatedInputs);
					}

#if DEBUG_NETWORK_PHYSICS
					{
						FAsyncPhysicsTimestamp Timestamp = const_cast<APlayerController*>(PlayerController)->GetPhysicsTimestamp();
						FString NetRole = HasServerWorld() ? FString("SERVER    ") : (IsLocallyControlled() ? FString("AUTONOMOUS") : FString("PROXY     "));
						ReplicatedInputs.History->DebugData(FString::Printf(TEXT("%s | PT | SendInputData | CurrentLocalFrame = %d | CurrentServerFrame = %d | Component = %s"), *NetRole, Timestamp.LocalFrame, Timestamp.ServerFrame, *GetFullName()));
					}
#endif
				}
			}

			// -- Important / Reliable flow --
			if (bEnableReliableFlow)
			{
				/* Get the latest valid frame that can hold new important data:
				* 1. Frame after last time we called SendInputData
				* 2. Frame after last entry in replicated data
				* 3. Earliest possible frame in history */
				const int32 FromFrame = FMath::Max(FMath::Max(LastInputSendFrame + 1, ReplicatedImportantInput.History->GetLatestFrame() + 1), ToFrame - InputHistory->GetHistorySize());

				for (int32 CopyFrame = FromFrame; CopyFrame <= ToFrame; CopyFrame++)
				{
					if (InputHistory->CopyData(*ReplicatedImportantInput.History, CopyFrame, CopyFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
					{
						if (bIsServer)
						{
							MulticastReceiveImportantInputData(ReplicatedImportantInput);
						}
						else
						{
							ServerReceiveImportantInputData(ReplicatedImportantInput);
						}
#if DEBUG_NETWORK_PHYSICS
						{
							FAsyncPhysicsTimestamp Timestamp = const_cast<APlayerController*>(PlayerController)->GetPhysicsTimestamp();
							FString NetRole = HasServerWorld() ? FString("SERVER    ") : (IsLocallyControlled() ? FString("AUTONOMOUS") : FString("PROXY     "));
							ReplicatedImportantInput.History->DebugData(FString::Printf(TEXT("%s | PT | SendInputData | RELIABLE | CurrentLocalFrame = %d | CurrentServerFrame = %d | Component = %s"), *NetRole, Timestamp.LocalFrame, Timestamp.ServerFrame, *GetFullName()));
						}
#endif
					}
				}
			}
			LastInputSendFrame = InputHistory->GetLatestFrame();
		}
	}

	if (bStopRelayingLocalInputsDeferred)
	{
		bIsRelayingLocalInputs = false;
		bStopRelayingLocalInputsDeferred = false;
	}
}

void UNetworkPhysicsComponent::SendStateData()
{
	if (HasServerWorld() && StateHistory)
	{
		// Send latest N frames from history
		const int32 ToFrame = StateHistory->GetLatestFrame();

		// -- Default / Unreliable Flow --
		if (bEnableUnreliableFlow)
		{
			const int32 FromFrame = ToFrame - StateRedundancy;
			if (StateHistory->CopyData(*ReplicatedStates.History, FromFrame, ToFrame, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false))
			{
				// If on server we should send the states onto all the clients through repnotify
				MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedStates, this);

#if DEBUG_NETWORK_PHYSICS
				ReplicatedStates.History->DebugData(FString::Printf(TEXT("SERVER | PT | SendStateData | Component = %s"), *GetFullName()));
#endif
			}
		}
		
		// -- Important / Reliable flow --
		if (bEnableReliableFlow)
		{
			/* Get the latest valid frame that can hold new important data: 
			* 1. Frame after last time we called SendStateData
			* 2. Frame after last entry in replicated data
			* 3. Earliest possible frame in history */
			const int32 FromFrame = FMath::Max(FMath::Max(LastStateSendFrame + 1, ReplicatedImportantState.History->GetLatestFrame() + 1), ToFrame - StateHistory->GetHistorySize());

			for (int32 CopyFrame = FromFrame; CopyFrame <= ToFrame; CopyFrame++)
			{
				if (StateHistory->CopyData(*ReplicatedImportantState.History, CopyFrame, CopyFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
				{
					MulticastReceiveImportantStateData(ReplicatedImportantState);
#if DEBUG_NETWORK_PHYSICS
					ReplicatedImportantState.History->DebugData(FString::Printf(TEXT("SERVER | PT | SendStateData | RELIABLE | Component = %s"), *GetFullName()));
#endif
				}
			}
		}
		LastStateSendFrame = StateHistory->GetLatestFrame();
	}
}

/* Deprecated 5.4 */
void UNetworkPhysicsComponent::CorrectServerToLocalOffset(const int32 LocalToServerOffset)
{
	if (IsLocallyControlled() && !HasServerWorld() && StateHistory)
	{
		TArray<int32> LocalFrames, ServerFrames, InputFrames;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to DebugData() in UE 5.6 and remove deprecation pragma
		StateHistory->DebugDatas(*ReplicatedStates.History, LocalFrames, ServerFrames, InputFrames);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		int32 ServerToLocalOffset = LocalToServerOffset;
		for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
		{
#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
			UE_LOG(LogChaos, Log, TEXT("CLIENT | GT | CorrectServerToLocalOffset | Server frame = %d | Client Frame = %d | [NOTE: DEPRECATED logic, deactivate by setting CVar: p.net.CmdOffsetEnabled 0]"), ServerFrames[FrameIndex], InputFrames[FrameIndex]);
#endif
			ServerToLocalOffset = FMath::Min(ServerToLocalOffset, ServerFrames[FrameIndex] - InputFrames[FrameIndex]);
		}
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GetPlayerController()->SetServerToLocalAsyncPhysicsTickOffset(ServerToLocalOffset);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
		UE_LOG(LogChaos, Log, TEXT("CLIENT | GT | CorrectServerToLocalOffset | Server to local offset = %d | Local to server offset = %d | [NOTE: DEPRECATED logic, deactivate by setting CVar: p.net.CmdOffsetEnabled 0]"), ServerToLocalOffset, LocalToServerOffset);
#endif
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedStates()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}

	// The replicated states should only be used on the client since the server already have authoritative local ones
	if (PlayerController && !HasServerWorld() && StateHistory)
	{
		const int32 LocalOffset = PlayerController->GetNetworkPhysicsTickOffset();

		/* Deprecated 5.4 */
		if (InputCmdCVars::bCmdOffsetEnabled)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const int32 LocalToServerOffset = PlayerController->GetLocalToServerAsyncPhysicsTickOffset();
			CorrectServerToLocalOffset(LocalToServerOffset);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		// Record the received states from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedStates = MakeShareable(ReplicatedStates.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedStates, LocalOffset]()
				{
					if (bCompareStateToTriggerRewind && IsLocallyControlled())
					{
						// Record received data into history
						const int32 ResimFrame = StateHistory->ReceiveNewData(*ReceivedStates, LocalOffset, /*CompareDataForRewind*/ bCompareStateToTriggerRewind);
					
						// Trigger resimulation if ReceiveNewData returned a valid frame to rewind to
						TriggerResimulation(ResimFrame);
					}
					else 
					{
						// Record received data into history
						PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ReceiveNewData() in UE 5.6 and remove deprecation pragma
						StateHistory->ReceiveNewDatas(*ReceivedStates, LocalOffset);
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
					}

#if DEBUG_NETWORK_PHYSICS
					{
						const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
						ReceivedStates->DebugData(FString::Printf(TEXT("CLIENT | PT | OnRep_SetReplicatedStates | CurrentLocalFrame = %d | LocalOffset = %d | Component = %s"), CurrentFrame, LocalOffset, *GetFullName()));
					}
#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedInputs()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}

	// Put replicated inputs into input history, also done for the local player since the server can alter inputs if invalid and the correct server-authoritative input should be used during a resimulation
	if (PlayerController && !HasServerWorld() && InputHistory)
	{
		const int32 LocalOffset = PlayerController->GetNetworkPhysicsTickOffset();

		// Record the received inputs from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ReplicatedInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedInputs, LocalOffset]()
				{
					if (bCompareInputToTriggerRewind && IsLocallyControlled())
					{
						// Record received data into history
						const int32 ResimFrame = InputHistory->ReceiveNewData(*ReceivedInputs, LocalOffset, /*CompareDataForRewind*/ bCompareInputToTriggerRewind, /*Important*/false);
						
						// Trigger resimulation if ReceiveNewData returned a valid frame to rewind to
						TriggerResimulation(ResimFrame);
					}
					else
					{
						// Record received data into history
						PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ReceiveNewData() in UE 5.6 and remove deprecation pragma
						InputHistory->ReceiveNewDatas(*ReceivedInputs, LocalOffset);
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
					}

#if DEBUG_NETWORK_PHYSICS
					{
						const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
						ReceivedInputs->DebugData(FString::Printf(TEXT("CLIENT | PT | OnRep_SetReplicatedInputs | CurrentLocalFrame = %d | LocalOffset = %d | Component = %s"), CurrentFrame, LocalOffset, *GetFullName()));
					}
#endif
				}, false);
			}
		}
	}
}

/** DEPRECATED UE 5.4*/
void UNetworkPhysicsComponent::ServerReceiveInputsDatas_Implementation(const FNetworkPhysicsRewindDataInputProxy& ClientInputs)
{
	ServerReceiveInputData_Implementation(ClientInputs);
}

void UNetworkPhysicsComponent::ServerReceiveInputData_Implementation(const FNetworkPhysicsRewindDataInputProxy& ClientInputs)
{
	if (InputHistory)
	{ 
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				// Make another copy of the client inputs for the physics thread to consume
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ClientInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedInputs, PhysScene]()
				{
					// Validate data in the received inputs
					ReceivedInputs->ValidateDataInHistory(ActorComponent);

					// Record received data into history
					PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ReceiveNewData() in UE 5.6 and remove deprecation pragma
					InputHistory->ReceiveNewDatas(*ReceivedInputs, 0);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if DEBUG_NETWORK_PHYSICS
					{
						const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
						ReceivedInputs->DebugData(FString::Printf(TEXT("SERVER | PT | ServerReceiveInputData | CurrentLocalFrame = %d | Component = %s"), CurrentFrame, *GetFullName()));
					}
#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::ServerReceiveImportantInputData_Implementation(const FNetworkPhysicsRewindDataImportantInputProxy& ClientInputs)
{
	if (InputHistory)
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				// Make another copy of the client inputs for the physics thread to consume
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ClientInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedInputs, PhysScene]()
					{
						// Validate data in the received inputs
						ReceivedInputs->ValidateDataInHistory(ActorComponent);

						// Record received data into history
						InputHistory->ReceiveNewData(*ReceivedInputs, 0, /*CompareDataForRewind*/false, /*Important*/true);

						// Cache earliest important data frame since last ApplyData was called
						NewImportantInputFrame = FMath::Min(NewImportantInputFrame, ReceivedInputs->GetEarliestFrame());

#if DEBUG_NETWORK_PHYSICS
						{
							const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
							ReceivedInputs->DebugData(FString::Printf(TEXT("SERVER | PT | ServerReceiveImportantInputData | RELIABLE | CurrentLocalFrame = %d | NewImportantInputFrame = %d | Component = %s"), CurrentFrame, NewImportantInputFrame, *GetFullName()));
						}
#endif
					}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::MulticastReceiveImportantInputData_Implementation(const FNetworkPhysicsRewindDataImportantInputProxy& ServerInputs)
{
	// Multicast RPC, ignore on server
	if (HasServerWorld())
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}

	if (PlayerController && InputHistory)
	{
		const int32 LocalOffset = PlayerController->GetNetworkPhysicsTickOffset();
		
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				// Make another copy of the client inputs for the physics thread to consume
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedInputs = MakeShareable(ServerInputs.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedInputs, PhysScene, LocalOffset]()
					{
						// Record received data into history
						const bool bCompareDataForRewind = (bCompareInputToTriggerRewind && IsLocallyControlled());
						const int32 ResimFrame = InputHistory->ReceiveNewData(*ReceivedInputs, LocalOffset, /*CompareDataForRewind*/bCompareDataForRewind, /*Important*/true);
						
						// Trigger resimulation if ReceiveNewData returned a valid frame to rewind to
						TriggerResimulation(ResimFrame);

						// Cache earliest important data frame since last ApplyData was called
						NewImportantInputFrame = FMath::Min(NewImportantInputFrame, ReceivedInputs->GetEarliestFrame());

#if DEBUG_NETWORK_PHYSICS
						{
							const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
							FString NetRole = HasServerWorld() ? FString("SERVER    ") : (IsLocallyControlled() ? FString("AUTONOMOUS") : FString("PROXY     "));
							ReceivedInputs->DebugData(FString::Printf(TEXT("%s | PT | MulticastReceiveImportantInputData | RELIABLE | CurrentLocalFrame = %d | NewImportantInputFrame = %d | Component = %s"), *NetRole, CurrentFrame, NewImportantInputFrame, *GetFullName()));
						}
#endif
					}, false);
			}
		}

	}
}

void UNetworkPhysicsComponent::MulticastReceiveImportantStateData_Implementation(const FNetworkPhysicsRewindDataImportantStateProxy& ServerStates)
{
	// Multicast RPC, ignore on server
	if (HasServerWorld())
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}

	if (PlayerController && StateHistory)
	{
		const int32 LocalOffset = PlayerController->GetNetworkPhysicsTickOffset();

		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				// Make another copy of the client inputs for the physics thread to consume
				TSharedPtr<Chaos::FBaseRewindHistory> ReceivedStates = MakeShareable(ServerStates.History->Clone().Release());
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedStates, PhysScene, LocalOffset]()
					{
						const bool bCompareDataForRewind = (bCompareStateToTriggerRewind && IsLocallyControlled());
						const int32 ResimFrame = StateHistory->ReceiveNewData(*ReceivedStates, LocalOffset, /*CompareDataForRewind*/bCompareDataForRewind, /*bImportant*/true);
						TriggerResimulation(ResimFrame);

#if DEBUG_NETWORK_PHYSICS
						{
							const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();
							FString NetRole = HasServerWorld() ? FString("SERVER    ") : (IsLocallyControlled() ? FString("AUTONOMOUS") : FString("PROXY     "));
							ReceivedStates->DebugData(FString::Printf(TEXT("%s | PT | MulticastReceiveImportantStateData | RELIABLE | CurrentLocalFrame = %d | Component = %s"), *NetRole, CurrentFrame, *GetFullName()));
						}
#endif
					}, false);
			}
		}

	}
}

void UNetworkPhysicsComponent::OnPreProcessInputsInternal(const int32 PhysicsStep)
{
	const bool bIsServer = HasServerWorld();

#if DEBUG_NETWORK_PHYSICS
	if (bIsServer)
	{
		UE_LOG(LogChaos, Log, TEXT("SERVER | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogChaos, Log, TEXT("CLIENT | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if (ActorComponent)
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

		// Apply replicated state on clients if we are resimulating
		if (bIsSolverResim && StateHistory)
		{
			FNetworkPhysicsData* PhysicsData = StateData.Get();
			PhysicsData->LocalFrame = PhysicsStep;
			const bool bExactFrame = PhysicsReplicationCVars::ResimulationCVars::bAllowRewindToClosestState ? !bIsSolverReset : true;
			PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ExtractData() in UE 5.6 and remove deprecation pragma
			if (StateHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsData, bExactFrame) && PhysicsData->bReceivedData)
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			{
				PhysicsData->ApplyData(ActorComponent);
#if DEBUG_NETWORK_PHYSICS
				UE_LOG(LogChaos, Log, TEXT("		Applying extracted state from history | bExactFrame = %d | LocalFrame = %d | ServerFrame = %d | InputFrame = %d | Data: %s")
					, bExactFrame, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->InputFrame, *PhysicsData->DebugData());
#endif
			}
#if DEBUG_NETWORK_PHYSICS
			else
			{
				UE_LOG(LogChaos, Log, TEXT("		FAILED to extract and apply state from history | bExactFrame = %d | -- Printing history --"), bExactFrame);
				StateHistory->DebugData(FString::Printf(TEXT("StateHistory | Component = %s"), *GetFullName()));
			}
#endif
		}

		// Apply replicated inputs on server and simulated proxies (and on local player if we are resimulating)
		if (InputHistory && (!IsLocallyControlled() || bIsSolverResim))
		{
			FNetworkPhysicsData* PhysicsData = InputData.Get();
			int32 NextExpectedLocalFrame = PhysicsData->LocalFrame + 1;
			PhysicsData->LocalFrame = PhysicsStep;

			// There are important inputs earlier than upcoming input to apply
			if (NewImportantInputFrame < NextExpectedLocalFrame && !bIsSolverResim)
			{
				if (bApplyDataInsteadOfMergeData)
				{
#if DEBUG_NETWORK_PHYSICS
					UE_LOG(LogChaos, Log, TEXT("	Reapplying multiple data due to receiving an important data that was previously missed. FromFrame: %d | ToFrame: %d | IsLocallyControlled = %d"), NewImportantInputFrame, (NextExpectedLocalFrame - 1), IsLocallyControlled());
#endif
					// Apply all inputs in range
					InputHistory->ApplyDataRange(NewImportantInputFrame, NextExpectedLocalFrame - 1, ActorComponent, /*bOnlyImportant*/false);
				}
				else
				{
					// Merge all inputs from earliest new important
					NextExpectedLocalFrame = NewImportantInputFrame;
#if DEBUG_NETWORK_PHYSICS
					UE_LOG(LogChaos, Log, TEXT("	Prepare to reapply multiple data through MergeData due to receiving an important data that was previously missed. FromFrame: %d | ToFrame: %d | IsLocallyControlled = %d"), NewImportantInputFrame, (NextExpectedLocalFrame - 1), IsLocallyControlled());
#endif
				}
			}

			PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ExtractData() in UE 5.6 and remove deprecation pragma
			if (InputHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsData, /*bExactFrame*/(bAllowInputExtrapolation == false)))
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			{
				// Calculate input decay if we are resimulating and we don't have up to date inputs
				if (bIsSolverResim)
				{
					if (PhysicsData->LocalFrame < PhysicsStep)
					{
						const float InputDecay = GetCurrentInputDecay(PhysicsData);
						PhysicsData->DecayData(InputDecay);
					}
				}
				// Apply input if data is unimportant or if data is important and unaltered (extrapolated, interpolated)
				else if (!PhysicsData->bImportant || PhysicsData->InputFrame != INDEX_NONE)
				{
					// Check if we have a gap between last used input and current input
					if (PhysicsData->LocalFrame > NextExpectedLocalFrame)
					{
						if (bApplyDataInsteadOfMergeData)
						{
#if DEBUG_NETWORK_PHYSICS
							UE_LOG(LogChaos, Log, TEXT("	Applying multiple data instead of merging, from LocalFrame %d into LocalFrame %d | IsLocallyControlled = %d"), NextExpectedLocalFrame, PhysicsData->LocalFrame, IsLocallyControlled());
#endif
							// Iterate over each input and call ApplyData, except on the last, it will get handled by the normal ApplyData call further down
							const int32 LastFrame = PhysicsData->LocalFrame;
							for (; NextExpectedLocalFrame <= LastFrame; NextExpectedLocalFrame++)
							{
								if (InputHistory->ExtractData(NextExpectedLocalFrame, bIsSolverReset, PhysicsData, true) && NextExpectedLocalFrame < LastFrame)
								{
									PhysicsData->ApplyData(ActorComponent);
								}
							}
						}
						else
						{
#if DEBUG_NETWORK_PHYSICS
							UE_LOG(LogChaos, Log, TEXT("	Merging inputs from LocalFrame %d into LocalFrame %d | IsLocallyControlled = %d"), NextExpectedLocalFrame, PhysicsData->LocalFrame, IsLocallyControlled());
#endif
							// Merge all inputs since last used input
							InputHistory->MergeData(NextExpectedLocalFrame, PhysicsData);
						}
					}

					// If the extracted input data was altered (extrapolated, interpolated) on the server, record it into the history for it to get replicated to clients
					if (bIsServer && !bIsSolverResim && PhysicsData->InputFrame == INDEX_NONE)
					{
						PhysicsData->bReceivedData = true; // Mark the input data as received so that it doesn't get overwritten by incoming client inputs
						PhysicsData->bImportant = false;
						PhysicsData->LocalFrame = PhysicsStep;
						InputHistory->RecordData(PhysicsStep, PhysicsData);
					}
				}

				PhysicsData->ApplyData(ActorComponent);

#if DEBUG_NETWORK_PHYSICS
				UE_LOG(LogChaos, Log, TEXT("		Applying extracted input from history | LocalFrame = %d | ServerFrame = %d | InputFrame = %d | IsResim = %d | IsLocallyControlled = %d | InputDecay = %f | Data: %s")
					, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->InputFrame, bIsSolverResim, IsLocallyControlled(), GetCurrentInputDecay(PhysicsData), *PhysicsData->DebugData());
#endif
			}
#if DEBUG_NETWORK_PHYSICS
			else
			{
				UE_LOG(LogChaos, Log, TEXT("		FAILED to extract and apply input from history | IsResim = %d | IsLocallyControlled = %d | -- Printing history --"), bIsSolverResim, IsLocallyControlled());
				InputHistory->DebugData(FString::Printf(TEXT("InputHistory | Component = %s"), *GetFullName()));
			}
#endif
		}
	}
	NewImportantInputFrame = INT_MAX;
}

void UNetworkPhysicsComponent::OnPostProcessInputsInternal(const int32 PhysicsStep)
{
	const bool bIsServer = HasServerWorld();

	bool bIsSolverReset = false;
	bool bIsSolverResim = false;
#if DEBUG_NETWORK_PHYSICS
	if (bIsServer)
	{
		UE_LOG(LogChaos, Log, TEXT("SERVER | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogChaos, Log, TEXT("CLIENT | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if (ActorComponent)
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bIsSolverResim = Solver->GetEvolution()->IsResimming();
				bIsSolverReset = Solver->GetEvolution()->IsResetting();
			}
		}

		APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}

		const bool bShouldCacheInputHistory = IsLocallyControlled() && !bIsSolverResim;
		// for the inputs client local ones are ground truth otherwise use the replicated ones coming from the server
		if (PlayerController && bShouldCacheInputHistory && (InputData != nullptr))
		{
			FNetworkPhysicsData* PhysicsData = InputData.Get();
			PhysicsData->PrepareFrame(PhysicsStep, bIsServer, PlayerController->GetNetworkPhysicsTickOffset());

			PhysicsData->BuildData(ActorComponent);

			PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to RecordData() in UE 5.6 and remove deprecation pragma
			InputHistory->RecordDatas(PhysicsStep, PhysicsData);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if DEBUG_NETWORK_PHYSICS
			{
				UE_LOG(LogChaos, Log, TEXT("		Recording input into history | LocalFrame = %d | ServerFrame = %d | InputFrame = %d | Input: %s ")
					, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->InputFrame, *PhysicsData->DebugData());
			}
#endif
		}

		const bool bShouldCacheStateHistory = bIsServer || (bCompareStateToTriggerRewind && bShouldCacheInputHistory);
		if (PlayerController && StateHistory && bShouldCacheStateHistory)
		{
			// Compute of the local frame coming from the client that was used to generate this state
			int32 InputFrame = INDEX_NONE;
			{
				FNetworkPhysicsData* PhysicsData = InputData.Get();
				PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to ExtractData() in UE 5.6 and remove deprecation pragma
				if (InputHistory && InputHistory->ExtractDatas(PhysicsStep, false, PhysicsData, true))
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					InputFrame = PhysicsData->InputFrame;
				}
			}

			FNetworkPhysicsData* PhysicsData = StateData.Get();
			PhysicsData->PrepareFrame(PhysicsStep, bIsServer, PlayerController->GetNetworkPhysicsTickOffset());
			PhysicsData->InputFrame = InputFrame;

			PhysicsData->BuildData(ActorComponent);

			PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to RecordData() in UE 5.6 and remove deprecation pragma
			StateHistory->RecordDatas(PhysicsStep, PhysicsData);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if DEBUG_NETWORK_PHYSICS
			{
				UE_LOG(LogChaos, Log, TEXT("		Recording state into history | LocalFrame = %d | ServerFrame = %d | InputFrame = %d | State: %s ")
					, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->InputFrame, *PhysicsData->DebugData());
			}
#endif
		}
	}
}

const float UNetworkPhysicsComponent::GetCurrentInputDecay(FNetworkPhysicsData* PhysicsData)
{
	if (!PhysicsData)
	{
		return 0.0f;
	}

	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	if (!PhysScene)
	{
		return 0.0f;
	}

	Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver();
	if (!Solver)
	{
		return 0.0f;
	}

	Chaos::FRewindData* RewindData = Solver->GetRewindData();
	if (!RewindData)
	{
		return 0.0f;
	}
	
	const float NumPredictedInputs = RewindData->CurrentFrame() - PhysicsData->LocalFrame; // Number of frames we have used the same PhysicsData for during resim
	const float MaxPredictedInputs = RewindData->GetLatestFrame() - 1 - PhysicsData->LocalFrame; // Max number of frames PhysicsData registered frame until end of resim

	// Linear decay
	const float PredictionAlpha = MaxPredictedInputs > 0 ? (NumPredictedInputs / MaxPredictedInputs) : 0.0f;

	return PredictionAlpha;
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

bool UNetworkPhysicsComponent::IsLocallyControlled() const
{
	if (bIsRelayingLocalInputs)
	{
		return true;
	}

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

void UNetworkPhysicsComponent::RemoveDataHistory()
{
	if (GetWorld())
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
				{
					RewindData->RemoveInputHistory(InputHistory);
					RewindData->RemoveStateHistory(StateHistory);
				}
			}
		}
	}
}
void UNetworkPhysicsComponent::AddDataHistory()
{
	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
			{
				RewindData->AddInputHistory(InputHistory);
				if (StateHistory)
				{
					RewindData->AddStateHistory(StateHistory);
				}
			}
		}
	}
}

int32 UNetworkPhysicsComponent::SetupRewindData()
{
	int32 NumFrames = UPhysicsSettings::Get()->GetPhysicsHistoryCount();

	AActor* Owner = GetOwner();
	// Don't let this actor initialize RewindData if the actor is not using resimulation
	if (Owner && Owner->GetPhysicsReplicationMode() != EPhysicsReplicationMode::Resimulation)
	{
		return NumFrames;
	}

	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction && Solver->GetRewindData() == nullptr)
			{
				Solver->EnableRewindCapture(NumFrames, true);
			}

			if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
			{
				NumFrames = RewindData->Capacity();
			}
		}
	}

	return NumFrames;
}

void UNetworkPhysicsComponent::TriggerResimulation(int32 ResimFrame)
{
	if (ResimFrame != INDEX_NONE)
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
				{
					// Mark particle/island as resim
					Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
					if (Chaos::FPBDRigidParticleHandle* POHandle = Interface.GetRigidParticle(RootPhysicsObject))
					{
						Solver->GetEvolution()->GetIslandManager().SetParticleResimFrame(POHandle, ResimFrame);
					}

					// Set resim frame in rewind data
					ResimFrame = (RewindData->GetResimFrame() == INDEX_NONE) ? ResimFrame : FMath::Min(ResimFrame, RewindData->GetResimFrame());
					RewindData->SetResimFrame(ResimFrame);
				}
			}
		}
	}
}






