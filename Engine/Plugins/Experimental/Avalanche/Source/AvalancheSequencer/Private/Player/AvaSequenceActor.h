// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceActor.h"
#include "AvaSequenceActor.generated.h"

class UAvaSequence;
class UAvaSequencePlayer;

UCLASS()
class AAvaSequenceActor : public ALevelSequenceActor
{
	GENERATED_BODY()

public:
	AAvaSequenceActor(const FObjectInitializer& InObjectInitializer);

	void Initialize(UAvaSequence* InSequence);

	//~ Begin AActor
	virtual void PostInitializeComponents() override;
	//~ End AActor

private:
	void InitSequencePlayer(UAvaSequence* InSequence);
};
