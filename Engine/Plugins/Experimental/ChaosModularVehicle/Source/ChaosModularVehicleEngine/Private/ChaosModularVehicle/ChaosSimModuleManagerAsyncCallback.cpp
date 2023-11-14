// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"

#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "PBDRigidsSolver.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"

FSimModuleDebugParams GSimModuleDebugParams;

DECLARE_CYCLE_STAT(TEXT("AsyncCallback:OnPreSimulate_Internal"), STAT_AsyncCallback_OnPreSimulate, STATGROUP_ChaosSimModuleManager);

FName FChaosSimModuleManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FChaosSimModuleManagerAsyncCallback");
	return StaticName;
}

/**
 * Callback from Physics thread
 */

void FChaosSimModuleManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{
	const FChaosSimModuleManagerAsyncInput* AsyncInput = GetConsumerInput_Internal();
	if (AsyncInput == nullptr)
	{
		return;
	}

	for (const TUniquePtr<FModularVehicleAsyncInput>& VehicleInput : AsyncInput->VehicleInputs)
	{
		VehicleInput->ProcessInputs();
	}
}

/**
 * Callback from Physics thread
 */
void FChaosSimModuleManagerAsyncCallback::OnPreSimulate_Internal()
{
	using namespace Chaos;

	SCOPE_CYCLE_COUNTER(STAT_AsyncCallback_OnPreSimulate);

	float DeltaTime = GetDeltaTime_Internal();
	float SimTime = GetSimTime_Internal();

	const FChaosSimModuleManagerAsyncInput* Input = GetConsumerInput_Internal();
	if (Input == nullptr)
	{
		return;
	}

	const int32 NumVehicles = Input->VehicleInputs.Num();

	UWorld* World = Input->World.Get();	//only safe to access for scene queries
	if (World == nullptr || NumVehicles == 0)
	{
		//world is gone so don't bother, or nothing to simulate.
		return;
	}

	Chaos::FPhysicsSolver* PhysicsSolver = static_cast<Chaos::FPhysicsSolver*>(GetSolver());
	if (PhysicsSolver == nullptr)
	{
		return;
	}

	FChaosSimModuleManagerAsyncOutput& Output = GetProducerOutputData_Internal();
	Output.VehicleOutputs.AddDefaulted(NumVehicles);
	Output.Timestamp = Input->Timestamp;

	const TArray<TUniquePtr<FModularVehicleAsyncInput>>& InputVehiclesBatch = Input->VehicleInputs;
	TArray<TUniquePtr<FModularVehicleAsyncOutput>>& OutputVehiclesBatch = Output.VehicleOutputs;

	// beware running the vehicle simulation in parallel, code must remain threadsafe
	auto LambdaParallelUpdate = [World, DeltaTime, SimTime, &InputVehiclesBatch, &OutputVehiclesBatch](int32 Idx)
	{
		const FModularVehicleAsyncInput& VehicleInput = *InputVehiclesBatch[Idx];

		if (VehicleInput.Proxy == nullptr)
		{
			return;
		}

		bool bWake = false;
		OutputVehiclesBatch[Idx] = VehicleInput.Simulate(World, DeltaTime, SimTime, bWake);

	};

	bool ForceSingleThread = !GSimModuleDebugParams.EnableMultithreading;
	PhysicsParallelFor(OutputVehiclesBatch.Num(), LambdaParallelUpdate, ForceSingleThread);

	// Delayed application of forces - This is separate from Simulate because forces cannot be executed multi-threaded
	for (const TUniquePtr<FModularVehicleAsyncInput>& VehicleInput : InputVehiclesBatch)
	{
		if (VehicleInput.IsValid())
		{
			VehicleInput->ApplyDeferredForces();
		}
	}
}

/**
 * Contact modification currently unused
 */
void FChaosSimModuleManagerAsyncCallback::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifications)
{

}


TUniquePtr<FModularVehicleAsyncOutput> FModularVehicleAsyncInput::Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const
{
	TUniquePtr<FModularVehicleAsyncOutput> Output = MakeUnique<FModularVehicleAsyncOutput>();

	//support nullptr because it allows us to go wide on filling the async inputs
	if (Proxy == nullptr)
	{
		return Output;
	}

	if (Vehicle && Vehicle->VehicleSimulationPT)
	{
		// FILL OUTPUT DATA HERE THAT WILL GET PASSED BACK TO THE GAME THREAD
		Vehicle->VehicleSimulationPT->Simulate(World, DeltaSeconds, *this, *Output.Get(), Proxy);

		FModularVehicleAsyncOutput& OutputData = *Output.Get();
		Vehicle->VehicleSimulationPT->FillOutputState(OutputData);
	}


	Output->bValid = true;

	return MoveTemp(Output);
}

void FModularVehicleAsyncInput::ApplyDeferredForces() const
{
	if (Vehicle && Proxy)
	{
		if (Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
		{
			Vehicle->VehicleSimulationPT->ApplyDeferredForces(static_cast<Chaos::FClusterUnionPhysicsProxy*>(Proxy));
		}
		else if (Proxy->GetType() == EPhysicsProxyType::GeometryCollectionType)
		{
			Vehicle->VehicleSimulationPT->ApplyDeferredForces(static_cast<FGeometryCollectionPhysicsProxy*>(Proxy));
		}

	}

}

void FModularVehicleAsyncInput::ProcessInputs()
{
	if (!GetVehicle())
	{
		return;
	}

	FModularVehicleSimulationCU* VehicleSim = GetVehicle()->VehicleSimulationPT.Get();

	if (VehicleSim == nullptr || !GetVehicle()->bUsingNetworkPhysicsPrediction || GetVehicle()->GetWorld() == nullptr)
	{
		return;
	}
	bool bIsResimming = false;
	if (FPhysScene* PhysScene = GetVehicle()->GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* LocalSolver = PhysScene->GetSolver())
		{
			bIsResimming = LocalSolver->GetEvolution()->IsResimming();
		}
	}

	APlayerController* PlayerController = GetVehicle()->GetPlayerController();
	if (PlayerController && PlayerController->IsLocalController() && !bIsResimming)
	{
		VehicleSim->VehicleInputs = PhysicsInputs.NetworkInputs.VehicleInputs;
	}
	else
	{
		PhysicsInputs.NetworkInputs.VehicleInputs = VehicleSim->VehicleInputs;
	}

}

bool FNetworkModularVehicleInputs::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsDatas::SerializeFrames(Ar);

	Ar << VehicleInputs.Steering;
	Ar << VehicleInputs.Throttle;
	Ar << VehicleInputs.Brake;
	Ar << VehicleInputs.Handbrake;
	Ar << VehicleInputs.Pitch;
	Ar << VehicleInputs.Roll;
	Ar << VehicleInputs.Yaw;
	Ar << VehicleInputs.Boost;
	Ar << VehicleInputs.Drift;
	Ar << VehicleInputs.Reverse;
	Ar << VehicleInputs.KeepAwake;

	bOutSuccess = true;
	return bOutSuccess;
}

void FNetworkModularVehicleInputs::ApplyDatas(UActorComponent* NetworkComponent) const
{
	if (GSimModuleDebugParams.EnableNetworkStateData)
	{
		if (FModularVehicleSimulationCU* VehicleSimulation = Cast<UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleSimulation->VehicleInputs = VehicleInputs;
		}
	}
}

void FNetworkModularVehicleInputs::BuildDatas(const UActorComponent* NetworkComponent)
{
	if (GSimModuleDebugParams.EnableNetworkStateData && NetworkComponent)
	{
		if (const FModularVehicleSimulationCU* VehicleSimulation = Cast<const UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleInputs = VehicleSimulation->VehicleInputs;
		}
	}
}

void FNetworkModularVehicleInputs::InterpolateDatas(const FNetworkModularVehicleInputs& MinDatas, const FNetworkModularVehicleInputs& MaxDatas)
{
	const float LerpFactor = (LocalFrame - MinDatas.LocalFrame) / (MaxDatas.LocalFrame - MinDatas.LocalFrame);

	VehicleInputs.Steering = FMath::Lerp(MinDatas.VehicleInputs.Steering, MaxDatas.VehicleInputs.Steering, LerpFactor);
	VehicleInputs.Throttle = FMath::Lerp(MinDatas.VehicleInputs.Throttle, MaxDatas.VehicleInputs.Throttle, LerpFactor);
	VehicleInputs.Brake = FMath::Lerp(MinDatas.VehicleInputs.Brake, MaxDatas.VehicleInputs.Brake, LerpFactor);
	VehicleInputs.Handbrake = FMath::Lerp(MinDatas.VehicleInputs.Handbrake, MaxDatas.VehicleInputs.Handbrake, LerpFactor);
	VehicleInputs.Pitch = FMath::Lerp(MinDatas.VehicleInputs.Pitch, MaxDatas.VehicleInputs.Pitch, LerpFactor);
	VehicleInputs.Roll = FMath::Lerp(MinDatas.VehicleInputs.Roll, MaxDatas.VehicleInputs.Roll, LerpFactor);
	VehicleInputs.Yaw = FMath::Lerp(MinDatas.VehicleInputs.Yaw, MaxDatas.VehicleInputs.Yaw, LerpFactor);
	VehicleInputs.Boost = FMath::Lerp(MinDatas.VehicleInputs.Boost, MaxDatas.VehicleInputs.Boost, LerpFactor);
	VehicleInputs.Drift = FMath::Lerp(MinDatas.VehicleInputs.Drift, MaxDatas.VehicleInputs.Drift, LerpFactor);
	VehicleInputs.Reverse = MinDatas.VehicleInputs.Reverse;
	VehicleInputs.KeepAwake = MinDatas.VehicleInputs.KeepAwake;
}

bool FNetworkModularVehicleStates::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FNetworkPhysicsDatas::SerializeFrames(Ar);

	int32 NumNetModules = ModuleData.Num();
	Ar << NumNetModules;

	for (int I = 0; I < NumNetModules; I++)
	{
		if (Ar.IsLoading())
		{
			if (NumNetModules > 0)
			{
				int32 ModuleType = Chaos::eSimType::Undefined;
				int32 SimArrayIndex = 0;
				Ar << ModuleType;
				Ar << SimArrayIndex;

				if (!ModuleData.IsEmpty())
				{
					ensure(I <= ModuleData.Num());
				}

				if (ModuleData.Num() != NumNetModules)
				{
					ModuleData.Reserve(NumNetModules);
					switch (ModuleType)
					{
					case Chaos::eSimType::Suspension:
					{
						ModuleData.Emplace(MakeShared<Chaos::FSuspensionSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Transmission:
					{
						ModuleData.Emplace(MakeShared<Chaos::FTransmissionSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Engine:
					{
						ModuleData.Emplace(MakeShared<Chaos::FEngineSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Clutch:
					{
						ModuleData.Emplace(MakeShared<Chaos::FClutchSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					case Chaos::eSimType::Wheel:
					{
						ModuleData.Emplace(MakeShared<Chaos::FWheelSimModuleDatas>(SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							, FString()
#endif
						));
					}
					break;

					default:
					{
						checkf(false, TEXT("Unhandled NetModuleType case"));
					}
					}
				}
			}
		}
		else
		{
			int32 ModuleType = (int32)ModuleData[I]->GetType();
			Ar << ModuleType;
			Ar << ModuleData[I]->SimArrayIndex;
		}

		ModuleData[I]->Serialize(Ar);
	}

	return true;
}

void FNetworkModularVehicleStates::ApplyDatas(UActorComponent* NetworkComponent) const
{
	if (FModularVehicleSimulationCU* VehicleSimulation = Cast<UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
	{
		VehicleSimulation->AccessSimComponentTree()->SetSimState(ModuleData);
	}
}

void FNetworkModularVehicleStates::BuildDatas(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const FModularVehicleSimulationCU* VehicleSimulation = Cast<const UModularVehicleBaseComponent>(NetworkComponent)->VehicleSimulationPT.Get())
		{
			VehicleSimulation->GetSimComponentTree()->SetNetState(ModuleData);
		}
	}
}

void FNetworkModularVehicleStates::InterpolateDatas(const FNetworkModularVehicleStates& MinDatas, const FNetworkModularVehicleStates& MaxDatas)
{
	const float LerpFactor = (LocalFrame - MinDatas.LocalFrame) / (MaxDatas.LocalFrame - MinDatas.LocalFrame);

	for (int I = 0; I < ModuleData.Num(); I++)
	{
		// if these don't match then something has gone terribly wrong
		check(ModuleData[I]->GetType() == MinDatas.ModuleData[I]->GetType());
		check(ModuleData[I]->GetType() == MaxDatas.ModuleData[I]->GetType());

		ModuleData[I]->Lerp(LerpFactor, *MinDatas.ModuleData[I].Get(), *MaxDatas.ModuleData[I].Get());
	}
}

