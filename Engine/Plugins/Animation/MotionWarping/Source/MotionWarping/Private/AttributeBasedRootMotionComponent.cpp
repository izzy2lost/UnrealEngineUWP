// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttributeBasedRootMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttributeBasedRootMotionComponent)

// UAttributeBasedRootMotionComponent
///////////////////////////////////////////////////////////////////////

UAttributeBasedRootMotionComponent::UAttributeBasedRootMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bCanEverTick = true;
}

void UAttributeBasedRootMotionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());
}

void UAttributeBasedRootMotionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!bEnableRootMotion)
	{
		return;
	}
	
	static const FName RootMotionAttributeName = "RootMotionDelta";
	static const UE::Anim::FAttributeId RootMotionAttributeId = { RootMotionAttributeName , FCompactPoseBoneIndex(0) };
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	ACharacter* Character = GetCharacterOwner();
	check(Character);

	UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement<UCharacterMovementComponent>();

	if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (const FTransformAnimationAttribute* RootMotionAttribute = Mesh->GetCustomAttributes().Find<FTransformAnimationAttribute>(RootMotionAttributeId))
		{
			CharacterMovement->RootMotionParams.Set(RootMotionAttribute->Value);
		}
	}
}
