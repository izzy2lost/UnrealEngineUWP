// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "ChaosModularVehicle/ModularVehicleInputRate.h"
#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/SimModulesInclude.h"
#include "Curves/CurveFloat.h"

struct FModularVehicleAsyncInput;
struct FChaosSimModuleManagerAsyncOutput;
struct FModularVehicleDefaultAsyncInput;



class CHAOSMODULARVEHICLEENGINE_API FModularVehicleSimulationGC
{
public:
	FModularVehicleSimulationGC(bool InUsingNetworkPhysicsPrediction)
		: bUsingNetworkPhysicsPrediction(InUsingNetworkPhysicsPrediction)
		, StartFrame(0)
	{
	}

	virtual ~FModularVehicleSimulationGC()
	{
		SimModuleTree.Reset();
	}

	void Initialize(TUniquePtr<Chaos::FSimModuleTree>& InSimModuleTree);
	void Terminate();

	void Tick(UWorld* InWorld, float DeltaSeconds, const FModularVehicleDefaultAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, FGeometryCollectionPhysicsProxy* Proxy)
	{
		Simulate(InWorld, DeltaSeconds, InputData, OutputData, Proxy);
		FillOutputState(OutputData);
	}

	void SyncHistoryInputs(float DeltaSeconds, FGeometryCollectionPhysicsProxy* Proxy);
	void InterpolateInputs(float DeltaSeconds, const Chaos::FControlInputs& ExternalInputIn, Chaos::FControlInputs& InterpolatedInputsInOut);

	/** Update called from Physics Thread */
	virtual void Simulate(UWorld* InWorld, float DeltaSeconds, const FModularVehicleDefaultAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, FGeometryCollectionPhysicsProxy* Proxy);

	void ApplyDeferredForces(FGeometryCollectionPhysicsProxy* RigidHandle);

	void PerformAdditionalSimWork(UWorld* InWorld, const FModularVehicleDefaultAsyncInput& InputData, FGeometryCollectionPhysicsProxy* Proxy, Chaos::FAllInputs& AllInputs);

	void FillOutputState(FModularVehicleAsyncOutput& Output);

	Chaos::FControlInputs& AccessControlInputs();

	const TUniquePtr<Chaos::FSimModuleTree>& GetSimComponentTree() const {
		Chaos::EnsureIsInPhysicsThreadContext();
		return SimModuleTree;
		}

	TUniquePtr<Chaos::FSimModuleTree>& AccessSimComponentTree() {
		Chaos::EnsureIsInPhysicsThreadContext();
		return SimModuleTree; 
		}

	TArray<FModularVehicleInputRate>& AccessInputInterpolation() { return InputInterpolation; }

private:

	TUniquePtr<Chaos::FSimModuleTree> SimModuleTree;	/* Simulation modules stored in tree structure */
	TArray<FModularVehicleInputRate> InputInterpolation;
	Chaos::FAllInputs SimInputData;
	bool bUsingNetworkPhysicsPrediction;

	Chaos::FControlInputs AsyncControlInputs;  // Predictive networking relies on inputs being actioned ASAP via ExecuteAsyncPhysicsCommand with timestamp info
	TArray<FModularVehicleHistory> VehicleHistoryBuffer;
	int32 StartFrame = 0;
	int32 NumHistoryFrames = 0;
};