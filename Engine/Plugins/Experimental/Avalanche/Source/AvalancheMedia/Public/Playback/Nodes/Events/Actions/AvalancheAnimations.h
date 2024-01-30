// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequenceShared.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeAction.h"
#include "AvalancheAnimations.generated.h"

UENUM()
enum class EAvaMediaAnimAction
{
	None,
	Play,
	Continue,
	Stop,
	PreviewFrame,
	CameraCut
};


USTRUCT()
struct FAnimPlaySettings
{
	GENERATED_BODY()

	FAnimPlaySettings() : FAnimPlaySettings(NAME_None) {} 
	FAnimPlaySettings(FName InAnimationName) : AnimationName(InAnimationName) {}
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	EAvaMediaAnimAction Action = EAvaMediaAnimAction::None;
	
	UPROPERTY(VisibleAnywhere, Category = "Motion Design")
	FName AnimationName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Motion Design")
	float StartAtTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Motion Design")
	int32 LoopCount = 0;

	UPROPERTY(EditAnywhere, Category = "Motion Design")
	EAvaSequencePlayMode PlayMode = EAvaSequencePlayMode::Forward;

	UPROPERTY(EditAnywhere, Category = "Motion Design")
	float PlaybackSpeed = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	bool bRestoreState = false;

	friend uint32 GetTypeHash(const FAnimPlaySettings& InPlaySettings)
	{
		return GetTypeHash(InPlaySettings.AnimationName);
	}

	bool operator==(const FAnimPlaySettings& Other) const
	{
		return AnimationName == Other.AnimationName;
	}

	FAvaSequencePlayParams AsPlayParams() const
	{
		FAvaSequencePlayParams PlayParams;

		PlayParams.Start    = FAvaSequenceTime(StartAtTime);
		PlayParams.PlayMode = PlayMode;

		PlayParams.AdvancedSettings.PlaybackSpeed = PlaybackSpeed;
		PlayParams.AdvancedSettings.LoopCount     = LoopCount;
		PlayParams.AdvancedSettings.bRestoreState = bRestoreState;

		return PlayParams;
	}
};

USTRUCT()
struct FAvalancheAnimations
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	TSet<FAnimPlaySettings> AvailableAnimations;
};