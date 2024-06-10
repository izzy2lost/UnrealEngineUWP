// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanComponentUE.h"
#include "Components/SkeletalMeshComponent.h"


void UMetaHumanComponentUE::OnRegister()
{
	Super::OnRegister();
}

void UMetaHumanComponentUE::BeginPlay()
{
	Super::BeginPlay();

	if (Face)
	{
		PostInitAnimBP(Face.Get(), Face->GetPostProcessInstance());
	}

	if (Body)
	{
		UAnimInstance* AnimInstance = Body->GetPostProcessInstance();
		if (AnimInstance)
		{
			MetaHumanComponentHelpers::ConnectVariable<FBoolProperty, bool>(AnimInstance, TEXT("Enable Body Correctives"), bEnableBodyCorrectives);
		}
	}

	ConnectBodyPartAnimBPVariables(Torso);
	ConnectBodyPartAnimBPVariables(Legs);
	ConnectBodyPartAnimBPVariables(Feet);
}

void UMetaHumanComponentUE::OnUnregister()
{
	Super::OnUnregister();
}

void UMetaHumanComponentUE::ConnectBodyPartAnimBPVariables(const FMetaHumanCustomizableBodyPart& BodyPart) const
{
	USkeletalMeshComponent* SkelMeshComponent = BodyPart.SkeletalMeshComponent;
	if (!SkelMeshComponent)
	{
		return;
	}

	UAnimInstance* PostProcessAnimInstance = SkelMeshComponent->GetPostProcessInstance();
	PostConnectAnimBPVariables(BodyPart, SkelMeshComponent, PostProcessAnimInstance);
}