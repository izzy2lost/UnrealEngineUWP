// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaEffectorSubsystem.generated.h"

class AAvaEffectorActor;
class UNiagaraDataChannelAsset;

UCLASS()
class AVALANCHEEFFECTORS_API UAvaEffectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, const UWorld*)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	/** Get this subsystem instance */
	static UAvaEffectorSubsystem* Get(const UWorld* InWorld = GWorld);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	//~ End USubsystem

	/** Registers an effector actor to use it within a effector channel */
	bool RegisterChannelEffector(AAvaEffectorActor* InEffector);

	/** Unregister an effector actor used within a effector channel */
	bool UnregisterChannelEffector(AAvaEffectorActor* InEffector);

protected:
	//~ Begin FTickableGameObject interface	
	virtual bool IsTickableInEditor() const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float InDeltaTime) override;
	//~ End FTickableGameObject interface

	/** Updates all registered effectors */
	void UpdateEffectorChannel();

	/** Ordered effectors included in this channel */
	UPROPERTY()
	TArray<TWeakObjectPtr<AAvaEffectorActor>> EffectorsWeak;

	/** This represents the data channel structure for effector */
	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelAsset> EffectorDataChannelAsset;
};