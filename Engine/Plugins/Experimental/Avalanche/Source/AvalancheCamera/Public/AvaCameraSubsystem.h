// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AvaCameraSubsystem.generated.h"

class AActor;
class APlayerController;
class IAvaTransitionExecutor;
class UAvaCameraPriorityModifier;
struct FViewTargetTransitionParams;
class ULevel;

USTRUCT()
struct FAvaViewTarget
{
	GENERATED_BODY()

	bool IsValid() const;

	/** The View Target Actor */
	UPROPERTY()
	TObjectPtr<AActor> Actor;

	/** The Camera Modifier that the View Target Actor has */
	UPROPERTY()
	TObjectPtr<UAvaCameraPriorityModifier> CameraPriorityModifier;
};

UCLASS(MinimalAPI)
class UAvaCameraSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	AVALANCHECAMERA_API static UAvaCameraSubsystem* Get(const UObject* InObject);

	AVALANCHECAMERA_API void RegisterScene(const ULevel* InSceneLevel);
	AVALANCHECAMERA_API void UnregisterScene(const ULevel* InSceneLevel);

	bool IsBlendingToViewTarget(const ULevel* InSceneLevel) const;

	void UpdatePlayerControllerViewTarget(const FViewTargetTransitionParams* InOverrideTransitionParams = nullptr) const;

protected:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type InWorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

private:
	void OnTransitionStart(const IAvaTransitionExecutor& InExecutor);

	bool HasCustomViewTargetting(const ULevel* InSceneLevel) const;

	UPROPERTY()
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY()
	TArray<FAvaViewTarget> ViewTargets;
};
