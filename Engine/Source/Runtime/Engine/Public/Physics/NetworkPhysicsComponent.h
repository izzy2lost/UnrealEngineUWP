// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "RewindData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include "NetworkPhysicsComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInjectInputsExternal, const int32 /* PhysicsStep */, const int32 /* NumSteps */);

/** Templated datas history holding a datas buffer */
template<typename DatasType>
struct TNetRewindHistory : public Chaos::TDatasRewindHistory<DatasType>
{
	using Super = Chaos::TDatasRewindHistory<DatasType>;

	FORCEINLINE TNetRewindHistory(const int32 FrameCount, const bool bIsHistoryLocal) :
		Super(FrameCount, bIsHistoryLocal)
	{
	}

	FORCEINLINE virtual ~TNetRewindHistory() {}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> CreateNew() const
	{
		TUniquePtr<TNetRewindHistory> Copy = MakeUnique<TNetRewindHistory>(0, Super::bIsLocalHistory);

		return Copy;
	}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> Clone() const
	{
		return MakeUnique<TNetRewindHistory>(*this);
	}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> CopyFramesWithOffset(const uint32 StartFrame, const uint32 EndFrame, const int32 FrameOffset) override
	{
		uint32 FramesCount = (uint32)Super::NumValidDatas(StartFrame, EndFrame);
			
		TUniquePtr<TNetRewindHistory> Copy = MakeUnique<TNetRewindHistory>(FramesCount, Super::bIsLocalHistory);

		DatasType FrameDatas;
		for (uint32 FrameIndex = StartFrame; FrameIndex < EndFrame; ++FrameIndex)
		{
			const int32 LocalFrame = FrameIndex % Super::NumFrames;
			if (FrameIndex == Super::DatasArray[LocalFrame].LocalFrame)
			{
				FrameDatas = Super::DatasArray[LocalFrame];
				FrameDatas.ServerFrame = FrameDatas.LocalFrame + FrameOffset;
				Copy->RecordDatas(LocalFrame, &FrameDatas);
			}
		}

		return Copy;
	}

	virtual void ReceiveNewDatas(Chaos::FBaseRewindHistory& NewDatas, const int32 FrameOffset) override
	{
		TNetRewindHistory& NetNewDatas = static_cast<TNetRewindHistory&>(NewDatas);

		if (NetNewDatas.NumFrames > 0)
		{
			for (int32 FrameIndex = 0; FrameIndex < NetNewDatas.NumFrames; ++FrameIndex)
			{
				DatasType& FrameDatas = NetNewDatas.DatasArray[FrameIndex];

				FrameDatas.LocalFrame = FrameDatas.ServerFrame - FrameOffset;
				if (FrameDatas.LocalFrame >= 0)
				{
					Super::RecordDatas(FrameDatas.LocalFrame, &FrameDatas);
				}
			}
		}
	}

	virtual void NetSerialize(FArchive& Ar, UPackageMap* InPackageMap) override
	{
		Ar << Super::NumFrames;
		
		if (Super::NumFrames > GetMaxArraySize())
		{
			UE_LOG(LogChaos, Warning, TEXT("TNetRewindHistory: serialized array of size %d exceeds maximum size %d."), Super::NumFrames, GetMaxArraySize());
			Ar.SetError();
			return;
		}

		if (Ar.IsLoading())
		{
			Super::DatasArray.SetNum(Super::NumFrames);
		}

		for (DatasType& Data : Super::DatasArray)
		{
			NetSerializeDatas(Data, Ar, InPackageMap);
		}
	}

	/** Debug the datas from the archive */
	FORCEINLINE virtual void DebugDatas(const Chaos::FBaseRewindHistory& NewDatas, TArray<int32>& LocalFrames, TArray<int32>& ServerFrames, TArray<int32>& InputFrames) override
	{
		const TNetRewindHistory& NewNetDatas = static_cast<const TNetRewindHistory&>(NewDatas);

		if(NewNetDatas.NumFrames >= 0)
		{
			LocalFrames.SetNum(NewNetDatas.NumFrames);
			ServerFrames.SetNum(NewNetDatas.NumFrames);
			InputFrames.SetNum(NewNetDatas.NumFrames);

			DatasType FrameDatas;
			for (int32 FrameIndex = 0; FrameIndex < NewNetDatas.NumFrames; ++FrameIndex)
			{
				FrameDatas = NewNetDatas.DatasArray[FrameIndex];
				LocalFrames[FrameIndex] = FrameDatas.LocalFrame;
				ServerFrames[FrameIndex] = FrameDatas.ServerFrame;
				InputFrames[FrameIndex] = FrameDatas.InputFrame;
			}
		}
	}

private :

	/** Serialized array size limit to guard against invalid network data */
	static int32 GetMaxArraySize()
	{
		static int32 MaxArraySize = UPhysicsSettings::Get()->GetPhysicsHistoryCount() * 4;
		return MaxArraySize;
	}

	/** Use net serialize path to serialize datas  */
	FORCEINLINE bool NetSerializeDatas(DatasType& FrameDatas, FArchive& Ar, UPackageMap* PackageMap) const 
	{
		bool bOutSuccess = false;
		UScriptStruct* ScriptStruct = DatasType::StaticStruct();
		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, PackageMap, bOutSuccess, &FrameDatas);
		}
		else
		{
			UE_LOG(LogChaos, Error, TEXT("TNetRewindHistory::NetSerializeDatas called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());

			// Not working for now since the packagemap could be null
			// UNetConnection* Connection = CastChecked<UPackageMapClient>(PackageMap)->GetConnection();
			// UNetDriver* NetDriver = Connection ? Connection->GetDriver() : nullptr;
			// TSharedPtr<FRepLayout> RepLayout = NetDriver ? NetDriver->GetStructRepLayout(ScriptStruct) : nullptr;
			//
			// if (RepLayout.IsValid())
			// {
			// 	bool bHasUnmapped = false;
			// 	RepLayout->SerializePropertiesForStruct(ScriptStruct, Ar, PackageMap, &FrameDatas, bHasUnmapped);
			//
			// 	bOutSuccess = true;
			// }
		}
		return bOutSuccess;
	}
};

/**
 * Base struct for replicated rewind history properties
 */
USTRUCT()
struct FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()

	FNetworkPhysicsRewindDataProxy& operator=(const FNetworkPhysicsRewindDataProxy& Other);

	/** Causes the history to be serialized every time. If implemented, would prevent serializing if the history hasn't changed. */
	bool operator==(const FNetworkPhysicsRewindDataProxy& Other) const { return false; }

protected:
	bool NetSerializeBase(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, TUniqueFunction<TUniquePtr<Chaos::FBaseRewindHistory>()> CreateHistoryFunction);

public:
	/** The history to be serialized */
	TUniquePtr<Chaos::FBaseRewindHistory> History;

	/** Component that utilizes this data */
	UPROPERTY()
	TObjectPtr<UNetworkPhysicsComponent> Owner = nullptr;
};

/**
 * Struct suitable for use as a replicated property to replicate input rewind history
 */
USTRUCT()
struct FNetworkPhysicsRewindDataInputProxy : public FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()
		
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsRewindDataInputProxy> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsRewindDataInputProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};

/**
 * Struct suitable for use as a replicated property to replicate state rewind history
 */
USTRUCT()
struct FNetworkPhysicsRewindDataStateProxy : public FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsRewindDataStateProxy> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsRewindDataStateProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};

/**
 * Network physics rewind callback to manage all the sim callbacks rewind functionalities
 */
struct FNetworkPhysicsCallback : public Chaos::IRewindCallback
{
	FNetworkPhysicsCallback(UWorld* InWorld) : World(InWorld) 
	{
		UpdateNetMode();
	}

	// Delegate on the internal inputs process 
	FOnPreProcessInputsInternal PreProcessInputsInternal;
	FOnPostProcessInputsInternal PostProcessInputsInternal;
	// Bind to this for additional processing on the GT during InjectInputs_External()
	FOnInjectInputsExternal InjectInputsExternal;

	// Rewind API
	virtual void InjectInputs_External(int32 PhysicsStep, int32 NumSteps) override;
	virtual void ProcessInputs_External(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs);
	virtual void ProcessInputs_Internal(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs) override;
	virtual void ApplyCallbacks_Internal(int32 PhysicsStep, const TArray<Chaos::ISimCallbackObject*>& SimCallbackObjects) override;
	virtual int32 TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted) override;
	virtual void RegisterRewindableSimCallback_Internal(Chaos::ISimCallbackObject* SimCallbackObject) override
	{
		if (SimCallbackObject && SimCallbackObject->HasOption(Chaos::ESimCallbackOptions::Rewind))
		{
			RewindableCallbackObjects.Add(SimCallbackObject);
		}
	}

	// Updates the TMap on PhysScene that stores (non interpolated) physics data for replication.
	// 
	// Needs to be called from PT context to access fixed tick handle
	// but also needs to be able to access GT data (actor iterator, actor state)
	void UpdateReplicationMap_Internal(int32 PhysicsStep);

	// Update client player on GT
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	void UpdateClientPlayer_External(int32 PhysicsStep);

	// Update server player on GT
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	void UpdateServerPlayer_External(int32 PhysicsStep);

	// Cache the current netmode for use in PT
	void UpdateNetMode()
	{
		NetMode = World ? World->GetNetMode() : ENetMode::NM_Client;
	}

	// World owning that callback
	UWorld* World = nullptr;

	// Current NetMode
	ENetMode NetMode;

	// List of rewindable sim callback objects
	TArray<Chaos::ISimCallbackObject*> RewindableCallbackObjects;
};

/**
 * Network physics manager to initialize datas required for rewind/resim
 */
UCLASS(MinimalAPI)
class UNetworkPhysicsSystem : public UWorldSubsystem
{
public:

	GENERATED_BODY()
	ENGINE_API UNetworkPhysicsSystem();

	friend struct FNetworkPhysicsCallback;

	// Subsystem Init/Deinit
	ENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	ENGINE_API virtual void Deinitialize() override;

	// Delegate at world init 
	ENGINE_API void OnWorldPostInit(UWorld* World, const UWorld::InitializationValues);

	// Register a component to be used by the callback 
	void RegisterNetworkComponent(class UNetworkPhysicsComponent* NetworkComponent) {NetworkComponents.Add(NetworkComponent); }

	// Remove a network component from registered list
	void UnregisterNetworkComponent(class UNetworkPhysicsComponent* NetworkComponent) { NetworkComponents.Remove(NetworkComponent); }

private:

	// List of physics network components that will be used by the rewind callback
	TArray<class UNetworkPhysicsComponent*> NetworkComponents;
};

/**
 * Base network physics datas that will be used by physics
 */
 USTRUCT()
struct FNetworkPhysicsDatas
{
	GENERATED_USTRUCT_BODY()

	virtual ~FNetworkPhysicsDatas() = default;

	// Server frame at which this datas has been generated
	UPROPERTY()
	int32 ServerFrame = INDEX_NONE;

	// Local frame at which this datas has been generated
	UPROPERTY()
	int32 LocalFrame = INDEX_NONE;

	// Input frame used to generate the network datas
	UPROPERTY()
	int32 InputFrame = INDEX_NONE;

	// Serialize the datas into/from the archive
	void SerializeFrames(FArchive& Ar)
	{
		Ar << ServerFrame;
		Ar << LocalFrame;
		Ar << InputFrame;
	}

	// Apply the datas from onto the network physics component
	virtual void ApplyDatas(UActorComponent* NetworkComponent) const {}

	// Build the datas from the network physics component
	virtual void BuildDatas(const UActorComponent* NetworkComponent) {}
	
	/** Use to decay desired data during resimulation if data is forward predicted.
	* @param DecayAmount = Total amount of decay as a multiplier. 10% decay = 0.1.
	* NOTE: Decay is not accumulated, the data will be in its original state each time DecayDatas is called. DecayAmount will increase each time the input is predicted (reused).
	* EXAMPLE: Use to decay steering inputs to make resimulation not predict too much with a high steering value. Use DecayAmount of 0.1 to turn a steering value of 0.5 into 0.45 for example.
	*/ 
	virtual void DecayDatas(float DecayAmount) {}

	friend UNetworkPhysicsComponent;
};

/**
 * Network physics component that will be attached to any player controller
 */
UCLASS(BlueprintType, MinimalAPI)
class UNetworkPhysicsComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:
	ENGINE_API UNetworkPhysicsComponent();

	// Get the player controller on which the component is attached
	ENGINE_API virtual APlayerController* GetPlayerController() const;

	// Init the network physics component 
	ENGINE_API void InitPhysics();

	// Server RPC to receive inputs from client
	UFUNCTION(Server, unreliable)
	ENGINE_API void ServerReceiveInputsDatas(const FNetworkPhysicsRewindDataInputProxy& ClientInputs);

	// Async physics tick component function per frame from the solver
	ENGINE_API virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	// Function to init the replicated properties
	ENGINE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifeTimeProps) const override;

	// Send the inputs replicated datas
	ENGINE_API void SendLocalInputsDatas();

	// Send the states replicated datas
	ENGINE_API void SendLocalStatesDatas();

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPreProcessInputsInternal(const int32 PhysicsStep);

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPostProcessInputsInternal(const int32 PhysicsStep);

	// Correct the player controller Server to local offset based on the received replicated states
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	ENGINE_API void CorrectServerToLocalOffset(const int32 LocalToServerOffset);

	// Used to create any physics engine information for this component 
	ENGINE_API virtual void BeginPlay() override;

	// Register the component into the network manager
	ENGINE_API virtual void InitializeComponent() override;

	// Unregister the component from the network manager
	ENGINE_API virtual void UninitializeComponent() override;

	// Register and create the states/inputs history
	template<typename PhysicsTraits>
	void CreateDatasHistory(UActorComponent* HistoryComponent);

	// Remove states/inputs history from rewind datas
	ENGINE_API void RemoveDatasHistory();

	// Add states/inputs history to rewind datas
	ENGINE_API void AddDatasHistory();

	// Enable RewindData history caching and return the history size
	ENGINE_API int32 SetupRewindData();

	// Get the datas factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetStatesHistory() { return StatesHistory; }

	// Get the datas factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetInputsHistory() { return InputsHistory; }

	// Check if the world is on server
	ENGINE_API bool HasServerWorld() const;

	// Check if the player controller exists and is local
	UE_DEPRECATED(5.4, "Deprecated, use IsLocallyControlled() which takes both local player controlled and local relayed inputs into account.")
	ENGINE_API bool HasLocalController() const;
	
	// Check if this is controlled locally through relayed inputs or an existing local player controller
	ENGINE_API bool IsLocallyControlled() const;

	/** Mark this as controlled through locally relayed inputs rather than controlled as a pawn through a player controller.
	* Set if NetworkPhysicsComponent is implemented on an AActor instead of APawn and it's currently being fed inputs from the local player / autonomous proxy */
	ENGINE_API void SetIsRelayingLocalInputs(bool bInRelayingLocalInputs)
	{
		bIsRelayingLocalInputs = bInRelayingLocalInputs;
	}

	/** Check if this is controlled locally through relayed inputs from autonomous proxy. It's recommended to use IsLocallyControlled() when checking if this is locally controlled. */
	ENGINE_API const bool GetIsRelayingLocalInputs() const { return bIsRelayingLocalInputs; }

	/** Returns the current amount of input decay during resimulation as a magnitude from 0.0 to 1.0. Returns 0 if not currently resimulating. */
	ENGINE_API const float GetCurrentInputDecay(FNetworkPhysicsDatas* PhysicsDatas);

protected : 

	// repnotify for the inputs on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedInputs();

	// repnotify for the states on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedStates();

	// replicated physics inputs
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedInputs)
	FNetworkPhysicsRewindDataInputProxy ReplicatedInputs;

	// replicated physics states 
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedStates)
	FNetworkPhysicsRewindDataStateProxy ReplicatedStates;

	// Frame counter to compute the local to server offset
	int32 FrameCounter = 0;

private:

	friend FNetworkPhysicsCallback;
	friend struct FNetworkPhysicsRewindDataInputProxy;
	friend struct FNetworkPhysicsRewindDataStateProxy;

	// States history uses to rewind simulation 
	TSharedPtr<Chaos::FBaseRewindHistory> StatesHistory;

	// Inputs history used during simulation
	TSharedPtr<Chaos::FBaseRewindHistory> InputsHistory;

	// Local temporary inputs datas used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsDatas> InputsDatas;

	// Local temporary states datas used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsDatas> StatesDatas;

	// Specify how much times the network will resend the inputs in case of packet loss
	int8 InputsRedundancy = 4;

	// Current index used in the inputs offsets
	int8 InputsIndex = 0;

	// Inputs offsets defined on PT based on the newly recorded inputs datas
	TArray<int32> InputsOffsets;

	// Specify how much times the network will resend the states in case of packet loss
	int8 StatesRedundancy = 1;

	// Current index used in the states offsets
	int8 StatesIndex = 0;

	// States offsets defined on PT based on the newly recorded states datas
	TArray<int32> StatesOffsets;

	// Actor component that will be used to fill the histories
	TObjectPtr<UActorComponent> ActorComponent;

	// Locally relayed inputs makes this component act as if it's a locally controlled pawn.
	bool bIsRelayingLocalInputs = false;
};

template<typename PhysicsTraits>
FORCEINLINE void UNetworkPhysicsComponent::CreateDatasHistory(UActorComponent* HistoryComponent)
{
	const int32 NumFrames = SetupRewindData();

	APlayerController* Controller = GetPlayerController();
	const bool bIsLocalHistory = (Controller && Controller->IsLocalController()); // FIXME: The controller is null at this point, but bIsLocalHistory isn't currently used so doesn't create an issue.

	InputsHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::InputsType>>(NumFrames, bIsLocalHistory);
	StatesHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::StatesType>>(NumFrames, bIsLocalHistory);

	InputsDatas = MakeUnique<typename PhysicsTraits::InputsType>();
	StatesDatas = MakeUnique<typename PhysicsTraits::StatesType>();

	ReplicatedInputs.History = MakeUnique<TNetRewindHistory<typename PhysicsTraits::InputsType>>(NumFrames, bIsLocalHistory);
	ReplicatedInputs.Owner = this;

	ReplicatedStates.History = MakeUnique<TNetRewindHistory<typename PhysicsTraits::StatesType>>(NumFrames, bIsLocalHistory);
	ReplicatedStates.Owner = this;
	
	ActorComponent = HistoryComponent;
	
	AddDatasHistory();
}
