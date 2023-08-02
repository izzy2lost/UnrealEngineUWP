// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "BuoyancyRuntimeSettings.generated.h"

enum ECollisionChannel : int;

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Buoyancy"))
class BUOYANCY_API UBuoyancyRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	virtual FName GetCategoryName() const;

	/** Whether or not the buoyancy subsystem should run.
	    The subsystem has a public interface for enabling/disabling, so
	    it's possible that a game overrides this at runtime */
	UPROPERTY(EditAnywhere, config, Category = Buoyancy, Meta = (ClampMin = 0, ForceUnits = "g/cm3"))
	bool bBuoyancyEnabled = true;

	/** Prevent floating particles from falling asleep by explicitly setting
	    object state to dynamic when they are detected. */
	UPROPERTY(EditAnywhere, config, Category = Buoyancy)
	bool bKeepFloatingObjectsAwake = false;

	/** Density of water to use in buoyancy calculations. The density of water is approximately 1g/cm3 */
	UPROPERTY(EditAnywhere, config, Category = Buoyancy, Meta = (ClampMin = 0, ForceUnits = "g/cm3"))
	float WaterDensity = 1.f;

	/** Maximum number of times that a buoyancy bounds can be split into 8 */
	UPROPERTY(EditAnywhere, config, Category = Buoyancy, Meta = (ClampMin = 0))
	int32 MaxNumBoundsSubdivisions = 2;

	/** Minimum volume bounding box which can be produced by a subdivision operation */
	UPROPERTY(EditAnywhere, Config, Category = Buoyancy)
	float MinBoundsSubdivisionVol = FMath::Pow(100.f, 3.f); // 1m^3

	/** Collision channel to use for water ObjectTypes */
	UPROPERTY(EditAnywhere, config, Category = Collision)
	TEnumAsByte<ECollisionChannel> CollisionChannelForWaterObjects;

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	// Delegate called whenever the curve data is updated
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdateSettings, const UBuoyancyRuntimeSettings* /*Settings*/, EPropertyChangeType::Type /*ChangeType*/);
	static FOnUpdateSettings OnSettingsChange;
#endif
};
