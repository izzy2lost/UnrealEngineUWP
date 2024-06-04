// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeBasedRootMotionComponent.generated.h"

class ACharacter;

UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class MOTIONWARPING_API UAttributeBasedRootMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeBasedRootMotionComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** Gets the character this component belongs to */
	FORCEINLINE ACharacter* GetCharacterOwner() const { return CharacterOwner.Get(); }

	UPROPERTY(BlueprintReadWrite, Category = "Runtime")
	bool bEnableRootMotion = true;
	
protected:

	/** Character this component belongs to */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> CharacterOwner;

	FTransform PrevTransform;
	bool PrevTransformValid = false;
};