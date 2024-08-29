// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTrackFilter_Keyed.h"
#include "Filters/SequencerFilterBar.h"
#include "Filters/SequencerTrackFilterCommands.h"
#include "Framework/Commands/Commands.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/CategoryModel.h"
#include "Sequencer.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SequencerTrackFilter_Keyed"

FSequencerTrackFilter_Keyed::FSequencerTrackFilter_Keyed(ISequencerTrackFilters& InFilterInterface, TSharedPtr<FFilterCategory> InCategory)
	: FSequencerTrackFilter(InFilterInterface, MoveTemp(InCategory))
{
}

bool FSequencerTrackFilter_Keyed::ShouldUpdateOnTrackValueChanged() const
{
	return true;
}

FText FSequencerTrackFilter_Keyed::GetDefaultToolTipText() const
{
	return LOCTEXT("SequencerTrackFilter_KeyedTip", "Show only Keyed tracks"); 
}

TSharedPtr<FUICommandInfo> FSequencerTrackFilter_Keyed::GetToggleCommand() const
{
	return FSequencerTrackFilterCommands::Get().ToggleFilter_Keyed;
}

bool FSequencerTrackFilter_Keyed::SupportsSequence(UMovieSceneSequence* const InSequence) const
{
	return FSequencerTrackFilter::SupportsSequence(InSequence) || SupportsUMGSequence(InSequence);
}

bool FSequencerTrackFilter_Keyed::PassesFilter(FSequencerTrackFilterType InItem) const
{
	FSequencerFilterData& FilterData = FilterInterface.GetFilterData();

	for (const TViewModelPtr<IOutlinerExtension>& ChildNode : InItem->GetDescendantsOfType<IOutlinerExtension>(true))
	{
		const UMovieSceneTrack* const TrackObject = ResolveMovieSceneTrackObject(InItem, FilterData);
		if (DoesMovieSceneTrackHaveKeys(TrackObject))
		{
			return true;
		}
	}

	return false;
}

FText FSequencerTrackFilter_Keyed::GetDisplayName() const
{
	return LOCTEXT("SequenceTrackFilter_Keyed", "Keyed");
}

FSlateIcon FSequencerTrackFilter_Keyed::GetIcon() const
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Sequencer.IconKeyUser"));
}

FString FSequencerTrackFilter_Keyed::GetName() const
{
	return TEXT("Keyed");
}

bool FSequencerTrackFilter_Keyed::DoesMovieSceneTrackHaveKeys(const UMovieSceneTrack* const InTrackObject)
{
	if (!IsValid(InTrackObject))
	{
		return false;
	}

	for (const UMovieSceneSection* const Section : InTrackObject->GetAllSections())
	{
		for (const FMovieSceneChannelEntry& ChannelEntry : Section->GetChannelProxy().GetAllEntries())
		{
			const TConstArrayView<FMovieSceneChannel*> Channels = ChannelEntry.GetChannels();
			for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
			{
				const FMovieSceneChannel* const Channel = Channels[ChannelIndex];
				if (Channel && Channel->GetNumKeys() > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
