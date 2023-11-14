// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleSimulationGC.h"
#include "Components/MeshComponent.h"
#include "UObject/ObjectMacros.h"
#include "Curves/CurveFloat.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/SimModulesInclude.h"
#include "ChaosModularVehicle/ModularVehicleInputRate.h"

#include "ModularVehicleComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogModularVehicle, Log, All);

namespace Chaos
{ 
	class FWheelSimModule;
}

struct FModularVehicleAsyncInput;
struct FChaosSimModuleManagerAsyncOutput;
class AHUD;
class FDebugDisplayInfo;
class UCanvas;

USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleReplicatedState
{
	GENERATED_USTRUCT_BODY()

	FModularVehicleReplicatedState()
	{
		Controls.Init(0.0f, EModularVehicleInputType::Max);
		PrevControls.Init(0.0f, EModularVehicleInputType::Max);
	}

	TArray<float> Controls;
	TArray<float> PrevControls;
};


/**
 * Replicated data for a modular vehicle when bEnableReplication is true for
 * that component. See UpdateRepData/ProcessRepData
 */
USTRUCT()
struct FModularVehicleRepData
{
	GENERATED_BODY()

	struct FControlChanges
	{
		FControlChanges() : ID(0), Value(0.0f) {}
		FControlChanges(int InID, float InValue) : ID(InID), Value(InValue) {}

		friend FArchive& operator<<(FArchive& Ar, FControlChanges& objToSerialize)
		{
			Ar << objToSerialize.ID;
			Ar << objToSerialize.Value;
			return Ar;
		}

		int ID;
		float Value;
	};

	FModularVehicleRepData()
		: Version(0), ServerFrame(0)
	{

	}

	// Version counter, every write to the rep data is a new state so Identical only references this version
	// as there's no reason to compare the Poses array.
	int32 Version;

	// For Network Prediction Mode we require the frame number on the server when the data was gathered
	int32 ServerFrame;

	FModularVehicleReplicatedState Data;

	// Just test version to skip having to traverse the whole pose array for replication
	bool Identical(const FModularVehicleRepData* Other, uint32 PortFlags) const
	{
		return Other && (Version == Other->Version);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		Ar << Version;
		Ar << ServerFrame;

		TArray<FControlChanges> ControlInputChanges;
		
		if (Ar.IsSaving())
		{
			for (int I = 0; I < Data.Controls.Num(); I++)
			{
				// TODO: isn't working with this test, not sure why??	
				//if (Data.Controls[I] != Data.PrevControls[I])
				{
					Data.PrevControls[I] = Data.Controls[I];
					ControlInputChanges.Add(FControlChanges(I, Data.Controls[I]));
				}
			}
		}

		Ar << ControlInputChanges;

		if (Ar.IsLoading())
		{
			for (FControlChanges Change : ControlInputChanges)
			{
				Data.PrevControls[Change.ID] = Data.Controls[Change.ID];
				Data.Controls[Change.ID] = Change.Value;
			}
		}

		return bOutSuccess;
	}

};

template<>
struct TStructOpsTypeTraits<FModularVehicleRepData> : public TStructOpsTypeTraitsBase2<FModularVehicleRepData>
{
	enum
	{
		WithNetSerializer = true,
		WithIdentical = true,
	};
};


/**
*	ModularVehicleComponent
*/
UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent), hidecategories = (PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class CHAOSMODULARVEHICLEENGINE_API UModularVehicleComponent : public UGeometryCollectionComponent
{
	friend struct FModularVehicleDefaultAsyncInput;

	GENERATED_UCLASS_BODY()

public:
	~UModularVehicleComponent();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	/** Return true if it's suitable to create a physics representation of the vehicle at this time */
	virtual bool ShouldCreatePhysicsState() const override;

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;

	/** handle stand-alone and networked mode control inputs */
	void ProcessControls(float DeltaTime);

	/** Updates the vehicle tuning and other state such as user input. */
	virtual void PreTickGT(float DeltaTime);

	void CreateVehicleSim();
	void DestroyVehicleSim();

	TUniquePtr<Chaos::FSimModuleTree> GenerateSimTree();


	TUniquePtr<FPhysicsVehicleOutput>& PhysicsVehicleOutput()
	{
		return PVehicleOutput;
	}

	TUniquePtr<FModularVehicleAsyncInput> SetCurrentAsyncData(int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp);

	void SetCurrentAsyncDataInternal(FModularVehicleAsyncInput* CurInput, int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp);

	virtual void Update(float DeltaTime);
	virtual void PostUpdate();

	virtual void ResetVehicleState();

	// Get output data from Physics Thread
	virtual void ParallelUpdate(float DeltaSeconds);
	void FinalizeSimCallbackData(FChaosSimModuleManagerAsyncInput& Input);
	void ShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

protected:

	// accumulator for RB replication errors 
	float AngErrorAccumulator;

	// What the player has the steering set to. Range -1...1
	UPROPERTY(Transient)
	float RawSteeringInput;

	// What the player has the accelerator set to. Range -1...1
	UPROPERTY(Transient)
	float RawThrottleInput;

	// What the player has the brake set to. Range -1...1
	UPROPERTY(Transient)
	float RawBrakeInput;

	// What the player has the brake set to. Range -1...1
	UPROPERTY(Transient)
	float RawHandbrakeInput;

	// What the player has the clutch set to. Range -1...1
	UPROPERTY(Transient)
	float RawClutchInput;

	// What the player has the pitch set to. Range -1...1
	UPROPERTY(Transient)
	float RawPitchInput;

	// What the player has the roll set to. Range -1...1
	UPROPERTY(Transient)
	float RawRollInput;

	// What the player has the yaw set to. Range -1...1
	UPROPERTY(Transient)
	float RawYawInput;

	// latest gear selected
	UPROPERTY(Transient)
	int RawGearInput;

	// Steering output to physics system. Range -1...1
	UPROPERTY(Transient)
	float SteeringInput;

	// Accelerator output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ThrottleInput;

	// Brake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float BrakeInput;

	// Clutch output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ClutchInput;

	// Body Pitch output to physics system. Range -1...1
	UPROPERTY(Transient)
	float PitchInput;

	// Body Roll output to physics system. Range -1...1
	UPROPERTY(Transient)
	float RollInput;

	// Body Yaw output to physics system. Range -1...1
	UPROPERTY(Transient)
	float YawInput;

	// Handbrake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float HandbrakeInput;

	// Bypass the need for a controller in order for the controls to be processed.
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	bool bRequiresControllerForInputs;

	// How much to press the brake when the player has release throttle
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	float IdleBrakeInput;

	// Auto-brake when absolute vehicle forward speed is less than this (cm/s)
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	float StopThreshold;

	// Auto-brake when vehicle forward speed is opposite of player input by at least this much (cm/s)
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	float WrongDirectionThreshold;

	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	TArray<FModularVehicleInputRate> InputInterpolationRates;

	/** Set the user input for the vehicle throttle [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetThrottleInput(float Throttle);

	/** Increase the vehicle throttle position [throttle range normalized 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void IncreaseThrottleInput(float ThrottleDelta);

	/** Decrease the vehicle throttle position  [throttle range normalized 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void DecreaseThrottleInput(float ThrottleDelta);

	/** Set the user input for the vehicle Brake [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetBrakeInput(float Brake);

	/** Set the user input for the vehicle steering [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetSteeringInput(float Steering);

	/** Set the user input for the vehicle pitch [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetPitchInput(float Pitch);

	/** Set the user input for the vehicle roll [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetRollInput(float Roll);

	/** Set the user input for the vehicle yaw [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetYawInput(float Yaw);

	/** Set the user input for handbrake */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetHandbrakeInput(float Handbrake);

	/** Set the gear directly */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ChaosVehicleMovement")
	void SetGearInput(int Gear);

	void SetSleeping(bool bEnableSleep);

	//~ Begin Networking Replication Interface.
	UPROPERTY(Replicated)
	FModularVehicleRepData ModularVehicleRepData;

	int32 MVRepVersionProcessed = INDEX_NONE;

	/** Called post solve to allow authoritative components to update their replication data */
	void UpdateMVRepData();

	/** Called post solve to allow clients to receive their replication data */
	void ProcessMVRepData();

	virtual bool ProcessRepData(float DeltaTime, float SimTime) override;
	//~ End Networking Replication Interface.

	void PerformRaycasts(FModularVehicleDefaultAsyncInput* Inputs);
	Chaos::FWheelSimModule* LocatePhysicallyClosestWheel(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree, int SuspensionTreeIndex);
	void FixupTreeLinks(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree);
	void EnableAnimationForPhysicsDrivenTransforms(TUniquePtr<Chaos::FSimModuleTree>& SimModuleTree);
	void ClearRawInput();

	void CalcThrottleBrakeInput(float& ThrottleOut, float& BrakeOut);
	void UpdateChassisUnionCollision();

	EChaosAsyncVehicleDataType CurAsyncType;
	FModularVehicleAsyncInput* CurAsyncInput;
	struct FModularVehicleAsyncOutput* CurAsyncOutput;
	struct FModularVehicleAsyncOutput* NextAsyncOutput;
	float OutputInterpAlpha;

	struct FAsyncOutputWrapper
	{
		int32 Idx;
		int32 Timestamp;

		FAsyncOutputWrapper()
			: Idx(INDEX_NONE)
			, Timestamp(INDEX_NONE)
		{
		}
	};
	TArray<FAsyncOutputWrapper> OutputsWaitingOn;
	TUniquePtr<FPhysicsVehicleOutput> PVehicleOutput;	/* physics simulation data output from the async physics thread */
	TUniquePtr<FModularVehicleSimulationGC> VehicleSimulationPT;	/* simulation code running on the physics thread async callback */

	// Method of applying control inputs at a specific time - for network prediction
	UFUNCTION(reliable, server)
	void ServerApplyControls(float InSteeringInput, float InThrottleInput, float InBrakeInput
			, float InHandbrakeInput, int32 InGearInput, float InRollInput, float InPitchInput, float InYawInput, const FAsyncPhysicsTimestamp& AsyncPhysicsTimestamp);

private:
	// The function that actually applies controls - designed to run on both server and client
	void ApplyControls_Imp(float InSteeringInput, float InThrottleInput, float InBrakeInput
		, float InHandbrakeInput, int32 InGearInput, float InRollInput, float InPitchInput, float InYawInput, const FAsyncPhysicsTimestamp& AsyncPhysicsTimestamp);

	Chaos::FPBDRigidClusteredParticleHandle* GetChassisParticle();

	bool bUsingNetworkPhysicsPrediction;
	float PrevSteeringInput;
};


