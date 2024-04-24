// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimationAsset.h"

class UAnimationAsset;
class USkeletalMeshComponent;

/// Class to work with animations within a SkeletalMesh
class FShooterTestsAnimationTestHelper
{
public:

	/**
	 * Find an animation asset within a SkeletalMesh by name.
	 *
	 * @return Animation asset reference if found.
	 */
	UAnimationAsset* FindAnimationAsset(const FString& AnimationName);

	/**
	 * Check if an animation asset is currently playing.
	 *
	 * @return true if animation asset is playing.
	 */
	bool IsAnimationPlaying(const UAnimationAsset* ExpectedAnimation);
	
	/**
	 * Sets the SkeletalMesh to be used with the above methods.
	 *
	 * @param SkeletalMesh reference to be used.
	 */
	void SetSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
	{
		check(InSkeletalMeshComponent);
		SkeletalMeshComponent = InSkeletalMeshComponent;
	}

private:
	USkeletalMeshComponent* SkeletalMeshComponent;
};