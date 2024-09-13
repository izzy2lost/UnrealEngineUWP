// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Engine/World.h"
#include "FoliageEditorSubsystem.generated.h"

class ULevel;
class AActor;

UCLASS()
class UFoliageEditorSubsystem : public UEditorSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	UFoliageEditorSubsystem();
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

private:
	void OnActorMoved(AActor* InActor);
	void OnActorOuterChanged(AActor* InActor, UObject* OldOuter);
	void OnActorDeleted(AActor* InActor);
	void OnPostApplyLevelOffset(ULevel* InLevel, UWorld* InWorld, const FVector& InOffset, bool bWorldShift);
	void OnPostApplyLevelTransform(ULevel* InLevel, const FTransform& InTransform);
	void OnPostWorldInitialization(UWorld* InWorld, const UWorld::InitializationValues IVS);

	TMap<TWeakObjectPtr<UWorld>, TSet<TWeakObjectPtr<AActor>>> ActorsPendingMovementUpdatePerWorld;
};
