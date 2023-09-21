// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/TrackInstance/MovieSceneTrackInstance.h"
#include "TrackInstances/MovieSceneCameraCutViewportPreviewer.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneCameraCutTrackInstance.generated.h"

class IMovieScenePlayer;
class UMovieSceneCameraCutSection;

namespace UE::MovieScene
{ 
	struct FCameraCutAnimator; 
	struct FCameraCutPlaybackCapability;
	struct FOnCameraCutUpdatedParams;
	struct FSequenceInstance;

	// Backwards compatibilty wrapper for camera cut playback capability.
	struct FCameraCutPlaybackCapabilityCompatibilityWrapper
	{
		FCameraCutPlaybackCapabilityCompatibilityWrapper(const FSequenceInstance& SequenceInstance);

		bool ShouldUpdateCameraCut();
		void OnCameraCutUpdated(const FOnCameraCutUpdatedParams& Params);

		FCameraCutPlaybackCapability* CameraCutCapability;
		IMovieScenePlayer* Player;
	};
}

/**
 * Track instance used to animate camera cuts.
 */
UCLASS()
class UMovieSceneCameraCutTrackInstance : public UMovieSceneTrackInstance
{
	GENERATED_BODY()

private:
	virtual void OnAnimate() override;
	virtual void OnInputAdded(const FMovieSceneTrackInstanceInput& InInput) override;
	virtual void OnEndUpdateInputs() override;
	virtual void OnDestroyed() override;

private:
	/**
	 * Stores information about the last set camera in order to differentiate
	 * between new and pre-existing cuts.
	 */
	struct FCameraCutCache
	{
		TWeakObjectPtr<> LastLockedCamera;
		UE::MovieScene::FInstanceHandle LastInstanceHandle;
		TObjectPtr<UMovieSceneSection> LastSection;
	};

	/**
	 * Track instance input qualified with the global start time of its corresponding
	 * section, used for sorting inputs and prioritizing more "recent" camera cuts
	 * over "older" ones.
	 */
	struct FCameraCutInputInfo
	{
		FMovieSceneTrackInstanceInput Input;
		float GlobalStartTime = 0.f;
	};

	FCameraCutCache CameraCutCache;
	TArray<FCameraCutInputInfo> SortedInputInfos;

#if WITH_EDITOR
	UE::MovieScene::FCameraCutViewportPreviewer ViewportPreviewer;
#endif

private:

	friend struct UE::MovieScene::FCameraCutAnimator;
};

