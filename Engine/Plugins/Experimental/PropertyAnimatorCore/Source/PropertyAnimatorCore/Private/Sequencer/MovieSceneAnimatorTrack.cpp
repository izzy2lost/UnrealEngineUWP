// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneAnimatorTrack.h"

#include "Evaluation/Blending/MovieSceneBlendType.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScene.h"
#include "Sequencer/MovieSceneAnimatorEvalTemplate.h"
#include "Sequencer/MovieSceneAnimatorSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimatorTrack"

UMovieSceneAnimatorTrack::UMovieSceneAnimatorTrack()
	: UMovieSceneNameableTrack()
{
	SupportedBlendTypes = FMovieSceneBlendTypeField::None();
}

int32 UMovieSceneAnimatorTrack::GetChannelCount(uint8 InChannel) const
{
	int32 Count = 0;

	for (UMovieSceneSection* Section : GetAllSections())
	{
		if (const UMovieSceneAnimatorSection* AnimatorSection = Cast<UMovieSceneAnimatorSection>(Section))
		{
			if (AnimatorSection->GetChannel() == InChannel)
			{
				Count++;
			}
		}
	}

	return Count;
}

bool UMovieSceneAnimatorTrack::SupportsType(TSubclassOf<UMovieSceneSection> InSectionClass) const
{
	return InSectionClass == UMovieSceneAnimatorSection::StaticClass();
}

UMovieSceneSection* UMovieSceneAnimatorTrack::CreateNewSection()
{
	UMovieSceneAnimatorSection* NewSection = NewObject<UMovieSceneAnimatorSection>(this, NAME_None, RF_Transactional);

	if (const UMovieScene* MovieScene = GetTypedOuter<UMovieScene>())
	{
		NewSection->SetStartFrame(MovieScene->GetPlaybackRange().GetLowerBound());
		NewSection->SetEndFrame(MovieScene->GetPlaybackRange().GetUpperBound());
	}

	return NewSection;
}

void UMovieSceneAnimatorTrack::AddSection(UMovieSceneSection& InSection)
{
	Sections.Add(&InSection);
}

const TArray<UMovieSceneSection*>& UMovieSceneAnimatorTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneAnimatorTrack::HasSection(const UMovieSceneSection& InSection) const
{
	return Sections.Contains(&InSection);
}

bool UMovieSceneAnimatorTrack::IsEmpty() const
{
	return Sections.IsEmpty();
}

void UMovieSceneAnimatorTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

void UMovieSceneAnimatorTrack::RemoveSection(UMovieSceneSection& InSection)
{
	Sections.Remove(&InSection);
}

void UMovieSceneAnimatorTrack::RemoveSectionAt(int32 InSectionIndex)
{
	Sections.RemoveAt(InSectionIndex);
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneAnimatorTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("MovieSceneAnimatorTrackName", "Animator Channel");
}

bool UMovieSceneAnimatorTrack::CanRename() const
{
	return true;
}
#endif

FMovieSceneEvalTemplatePtr UMovieSceneAnimatorTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneAnimatorSection* AnimatorSection = Cast<UMovieSceneAnimatorSection>(&InSection);

	if (!AnimatorSection)
	{
		return FMovieSceneEvalTemplatePtr();
	}

	return FMovieSceneAnimatorEvalTemplate(AnimatorSection->GetChannel());
}

#undef LOCTEXT_NAMESPACE


