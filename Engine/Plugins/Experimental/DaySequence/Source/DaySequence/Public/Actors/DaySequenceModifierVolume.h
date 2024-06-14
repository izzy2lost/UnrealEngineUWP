// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DaySequenceModifierVolume.generated.h"

class ADaySequenceActor;
class APawn;

class UBoxComponent;
class UDaySequenceModifierComponent;

UCLASS(Blueprintable)
class DAYSEQUENCE_API ADaySequenceModifierVolume : public AActor
{
	GENERATED_BODY()

public:
	ADaySequenceModifierVolume(const FObjectInitializer& Init);

	UFUNCTION(BlueprintImplementableEvent)
	void OnDaySequenceActorBound(ADaySequenceActor* InActor);
	
protected:
	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	//~ End AActor interface
	
	// Primary initialization entry point to clarify initialization control flow and to consolidate editor and runtime initialization.
	// Called from BeginPlay or OnConstruction
	void Initialize();

	
	
	/** Volume Setup Functions */
	
	/**
	 * Unconditionally adds shape components to the day sequence modifier via AddShapeComponentsToModifier.
	 * At runtime this also unconditionally calls SetupVolumeCallbacks.
	 * In editor this conditionally calls SetupVolumeCallbacks if we are not in an Editor world.
	 */
	void VolumeSetup();

	/**
	 * By default this adds an FComponentReference pointing to DefaultBox to the day sequence modifier.
	 * Derived classes should override this function to add their own shape components at the proper time.
	 * If overridden, calling Super::AddShapeComponentsToModifier will determine if DefaultBox is used or ignored.
	 */
	virtual void AddShapeComponentsToModifier();

	
	
	/** Player Controller Setup Functions */
	
	/**
	 * At runtime this unconditionally calls CachePlayerController and SetupBlendTargetCallbacks.
	 * In editor this conditionally calls CachePlayerController and SetupBlendTargetCallbacks if we are not in an Editor world.
	 */
	void PlayerControllerSetup();

	/**
	 * Attempts to find a local player controller.
	 * If found, we get the controller's view target.
	 * If not found, we call QueuePlayerControllerQuery which will result in another call to this function next tick.
	 */
	void CachePlayerController();

	/** Set a timer to call CachePlayerController one tick from now. */
	void QueuePlayerControllerQuery();
	

	
	/** Day Sequence Actor Setup Functions */

	/** Dumb wrapper for SetupDaySequenceSubsystemCallbacks and BindToDaySequenceActor. */
	void DaySequenceActorSetup();

	/** Registers a callback that calls BindToDaySequenceActor when the world's current Day Sequence Actor changes. */
	void SetupDaySequenceSubsystemCallbacks();

	/**
	 * Attempts to get the world's current Day Sequence Actor.
	 * 
	 * If a Day Sequence Actor is found that is not the currently bound actor,
	 * we notify the day sequence modifier and call VolumeSetup and PlayerControllerSetup.
	 */
	void BindToDaySequenceActor();
	

	
	/** Enables this modifier if the current blend target is in the volume. */
	void TryEnableModifier() const;
	
	void SetBlendTarget(APlayerController* InPC);
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Day Sequence", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDaySequenceModifierComponent> DaySequenceModifier;

	UPROPERTY(VisibleAnywhere, Category = "Day Sequence", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DefaultBox;
	
	UPROPERTY(Transient, DuplicateTransient)
    TObjectPtr<ADaySequenceActor> DaySequenceActor;
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<APlayerController> CachedPlayerController;
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AActor> CurrentBlendTarget;
	
	FDelegateHandle ViewTargetChangedHandle;
};
