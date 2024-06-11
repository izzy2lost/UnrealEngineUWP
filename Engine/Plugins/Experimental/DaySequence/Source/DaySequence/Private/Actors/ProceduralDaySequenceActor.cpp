// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ProceduralDaySequenceActor.h"

#include "DaySequence.h"
#include "DaySequenceCollectionAsset.h"
#include "ProceduralDaySequenceBuilder.h"
#include "Sections/MovieSceneSubSection.h"

void AProceduralDaySequenceActor::OnConstruction(const FTransform& Transform)
{
	CreateProceduralSequence();
	
	Super::OnConstruction(Transform);
}

#if WITH_EDITOR
void AProceduralDaySequenceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AProceduralDaySequenceActor::BuildSequence(UProceduralDaySequenceBuilder* SequenceBuilder) {}

void AProceduralDaySequenceActor::InitializeDaySequences()
{
	Super::InitializeDaySequences();
	
	CreateProceduralSequence();

	InitializeDaySequence(ProceduralSequence);
}

void AProceduralDaySequenceActor::CreateProceduralSequence()
{
	if (ProceduralSequence && !bProceduralSequenceInvalid)
	{
		return;
	}
	
	UProceduralDaySequenceBuilder* SequenceBuilder = NewObject<UProceduralDaySequenceBuilder>();
	ProceduralSequence = SequenceBuilder->Initialize(this, ProceduralSequence);
	BuildSequence(SequenceBuilder);
	
	bProceduralSequenceInvalid = false;
}