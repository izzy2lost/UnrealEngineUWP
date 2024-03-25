// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
#include "Engine/HitResult.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "LyraContextEffectsInterface.generated.h"

class UAnimSequenceBase;
class UObject;
class USceneComponent;
struct FFrame;
struct FHitResult;

/**
 *
 */
UENUM()
enum EEffectsContextMatchType: int
{
	ExactMatch,
	BestMatch
};

/**
 *
 */
 UINTERFACE(Blueprintable)
 class LYRAGAME_API ULyraContextEffectsInterface : public UInterface
 {
	 GENERATED_BODY()

 };
 
 class LYRAGAME_API ILyraContextEffectsInterface : public IInterface
 {
	 GENERATED_BODY()

 public:

	/** */
 	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AnimMotionEffect(const FName Bone
		, const FGameplayTag MotionEffect
		, USceneComponent* StaticMeshComponent
		, const FVector LocationOffset
		, const FRotator RotationOffset
		, const UAnimSequenceBase* AnimationSequence
		, const bool bHitSuccess
		, const FHitResult HitResult
		, FGameplayTagContainer Contexts
		, FVector VFXScale = FVector(1)
		, float AudioVolume = 1
		, float AudioPitch = 1);
 };

