// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "PBDRigidsSolver.h"
#include "Engine/EngineBaseTypes.h"
#include "BuoyancySubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuoyancySubsystem, Log, All);

//
// Buoyancy Settings
//

struct FBuoyancySettings
{
	// Force buoyant particles which are in water to stay awake
	bool bKeepAwake = false;

	// Density of water is about 1g/cm^3
	// Source: https://en.wikipedia.org/wiki/Properties_of_water
	float WaterDensity = 0.0001f; // kg/cm^3

	float MaxDeltaV = 200.f; // cm/s

	float MaxDeltaW = 2.f; // rad/s

	float WaterDrag = 1.f; // unitless

	int32 MaxNumBoundsSubdivisions = 2;

	float MinBoundsSubdivisionVol = FMath::Pow(100.f, 3.f); // 1m^3

	ECollisionChannel WaterCollisionChannel = ECollisionChannel::ECC_MAX;
};


//
// Buoyancy Subsystem
//

UCLASS()
class BUOYANCY_API UBuoyancySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

		UBuoyancySubsystem()
		: UTickableWorldSubsystem()
		, bBuoyancySettingsChanged(false)
		, BuoyancySettings(FBuoyancySettings())
		, SimCallback(nullptr)
	{ }

protected:

	// UTickableWorldSubsystem begin interface
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// UTickableWorldSubsystem end interface


private:

	Chaos::FPhysicsSolver* GetSolver() const;

	// When water plugin settings change, this callback will apply changes
	void ApplyRuntimeSettings(const class UWaterRuntimeSettings* InSettings, EPropertyChangeType::Type ChangeType);

	bool bBuoyancySettingsChanged;

	FBuoyancySettings BuoyancySettings;

	class FBuoyancySubsystemSimCallback* SimCallback;
};


//
// Buoyancy Sim Callback
//

struct FBuoyancySubsystemSimCallbackInput : public Chaos::FSimCallbackInput
{
	// Here we use a unique ptr so that it is possible to provide an async
	// input _without_ buoyancy settings (which may be eventually desirable
	// when we eventually are passing lists of water bodies or water wave
	// data).
	mutable TUniquePtr<FBuoyancySettings> BuoyancySettings;

	void Reset();
};

// NOTE: The Presimulate option is only needed for proper registry with the solver.
//       We don't actually need (or want!) a presimulate tick.
class FBuoyancySubsystemSimCallback : public Chaos::TSimCallbackObject<
	FBuoyancySubsystemSimCallbackInput,
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::MidPhaseModification>
{
private:
	virtual void OnPreSimulate_Internal() override { }
	virtual void OnMidPhaseModification_Internal(Chaos::FMidPhaseModifierAccessor& Modifier) override;

	// Initially we won't have any settings - they have to get passed down
	// via async input. I used TUniquePtr to control access to the same
	// memory that was allocated by GT to minimize copies.
	TUniquePtr<FBuoyancySettings> BuoyancySettings;
};
