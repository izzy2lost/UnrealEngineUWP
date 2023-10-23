// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextMeshComponent.h"
#include "GenerationTools.h"

UAnimNextMeshComponent::UAnimNextMeshComponent()
{
	// Disable regular skeletal mesh ticking
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
}

void UAnimNextMeshComponent::CompleteAndDispatch(TConstArrayView<FBoneIndexType> InParentIndices, TConstArrayView<FBoneIndexType> InRequiredBoneIndices, TConstArrayView<FTransform> InLocalSpaceTransforms)
{
	// Fill the component space transform buffer
	TArrayView<FTransform> ComponentSpaceTransforms = GetEditableComponentSpaceTransforms();
	if(ComponentSpaceTransforms.Num() > 0)
	{
		UE::AnimNext::FGenerationTools::ConvertLocalSpaceToComponentSpace(InParentIndices, InLocalSpaceTransforms, InRequiredBoneIndices, ComponentSpaceTransforms);

		// Flag buffer for flip
		bNeedToFlipSpaceBaseBuffers = true;
		bHasValidBoneTransform = false;
		FlipEditableSpaceBases();
		bHasValidBoneTransform = true;

		InvalidateCachedBounds();
		UpdateBounds();

		// Send updated transforms to the renderer
		SendRenderDynamicData_Concurrent();
	}
}
