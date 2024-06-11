// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "MovieSceneSequencePlayer.h"
#include "DaySequencePlayer.generated.h"

class UDaySequence;
struct FMovieSceneSequencePlaybackSettings;

class AActor;
class ADaySequenceActor;

/**
 * UDaySequencePlayer is used to actually "play" a Day sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * a DaySequence while its playing.
 */
UCLASS(BlueprintType)
class DAYSEQUENCE_API UDaySequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	UDaySequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InDaySequence The DaySequence to play.
	 * @param Owner The day sequence actor that owns this player
	 * @param Settings The desired playback settings
	 */
	void Initialize(UDaySequence* InDaySequence, ADaySequenceActor* Owner, const FMovieSceneSequencePlaybackSettings& Settings);

public:

	// IMovieScenePlayer interface
	virtual UObject* GetPlaybackContext() const override;

	void RewindForReplay();

protected:

	//~ UMovieSceneSequencePlayer interface
	virtual bool CanPlay() const override;

private:

	/** The owning Day Sequence Actor that created this player */
	TWeakObjectPtr<ADaySequenceActor> WeakOwner;
};
