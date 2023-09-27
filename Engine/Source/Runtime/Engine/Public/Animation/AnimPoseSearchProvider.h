// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FAnimationBaseContext;
class UAnimationAsset;

namespace UE::Anim
{

/** Modular feature interface for PoseSearch */
class ENGINE_API IPoseSearchProvider : public IModularFeature
{
public:
	virtual ~IPoseSearchProvider() {}

	static FName GetModularFeatureName();
	static bool IsAvailable();
	static IPoseSearchProvider* Get();
	
	struct FSearchResult
	{
		UAnimationAsset* AnimationAsset = nullptr;
		float TimeOffsetSeconds = 0.f;
		float Dissimilarity = MAX_flt;
	};

	/**
	* Finds a matching pose in the input Object given the current graph context
	* 
	* @param	GraphContext	Graph execution context used to construct a pose search query
	* @param	AnimationAssets	The animation assets to search for the pose query
	* 
	* @return	The pose in the AnimationAssets that most closely matches the query
	*/
	virtual FSearchResult Search(const FAnimationBaseContext& GraphContext, TConstArrayView<UAnimationAsset*> AnimationAssets) = 0;
};

} // namespace UE::Anim