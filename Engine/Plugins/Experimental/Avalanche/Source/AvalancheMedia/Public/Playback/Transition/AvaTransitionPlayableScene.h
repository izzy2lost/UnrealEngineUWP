// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "AvaTransitionScene.h"
#include "AvaTransitionPlayableScene.generated.h"

class UAvalanchePlayable;
class UAvalanchePlayableTransition;

USTRUCT()
struct AVALANCHEMEDIA_API FAvaTransitionPlayableScene : public FAvaTransitionScene
{
	GENERATED_BODY()

	FAvaTransitionPlayableScene() = default;

	explicit FAvaTransitionPlayableScene(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InPlayableTransition);

	explicit FAvaTransitionPlayableScene(const FAvaTagHandle& InTransitionLayer, UAvalanchePlayableTransition* InPlayableTransition);

	//~ Begin FAvaTransitionScene
	virtual EAvaTransitionComparisonResult Compare(const FAvaTransitionScene& InOther) const override;
	virtual ULevel* GetLevel() const override;
	virtual void GetOverrideTransitionLayer(FAvaTagHandle& OutTransitionLayer) const override;
	virtual void OnFlagsChanged() override;
	//~ End FAvaTransitionScene

	TWeakObjectPtr<UAvalanchePlayableTransition> PlayableTransitionWeak;

	TOptional<FAvaTagHandle> OverrideTransitionLayer;
};
