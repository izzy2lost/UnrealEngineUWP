// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/SequencerTrackFilterBase.h"
#include "Animation/WidgetAnimation.h"
#include "Filters/SequencerFilterBar.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "MovieSceneSequence.h"
#include "MVVM/ViewModelPtr.h"
#include "Sequencer.h"

using namespace UE::Sequencer;

FSequencerTrackFilter::FSequencerTrackFilter(ISequencerTrackFilters& InOutFilterInterface, TSharedPtr<FFilterCategory>&& InCategory)
	: FFilterBase<FSequencerTrackFilterType>(MoveTemp(InCategory))
	, FilterInterface(InOutFilterInterface)
{
}

FText FSequencerTrackFilter::GetToolTipText() const
{
	if (const TSharedPtr<FUICommandInfo> ToggleCommand = GetToggleCommand())
	{
		return BuildTooltipTextForCommand(GetDefaultToolTipText(), ToggleCommand);
	}
	return GetDefaultToolTipText();
}

bool FSequencerTrackFilter::SupportsSequence(UMovieSceneSequence* const InSequence) const
{
	return SupportsLevelSequence(InSequence);
}

void FSequencerTrackFilter::BindCommands()
{
	if (const TSharedPtr<FUICommandInfo> ToggleCommand = GetToggleCommand())
	{
		MapToggleAction(ToggleCommand);
	}
}

void FSequencerTrackFilter::BroadcastChangedEvent() const
{
	ChangedEvent.Broadcast();
}

ISequencerTrackFilters& FSequencerTrackFilter::GetFilterInterface() const
{
	return FilterInterface;
}

ISequencer& FSequencerTrackFilter::GetSequencer() const
{
	return FilterInterface.GetSequencer();
}

UMovieSceneSequence* FSequencerTrackFilter::GetFocusedMovieSceneSequence() const
{
	return GetSequencer().GetFocusedMovieSceneSequence();
}

UMovieScene* FSequencerTrackFilter::GetFocusedGetMovieScene() const
{
	const UMovieSceneSequence* const FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	return IsValid(FocusedMovieSceneSequence) ? FocusedMovieSceneSequence->GetMovieScene() : nullptr;
}

bool FSequencerTrackFilter::SupportsLevelSequence(UMovieSceneSequence* const InSequence)
{
	const UClass* const LevelSequenceClass = ULevelSequence::StaticClass();
	return IsValid(InSequence)
		&& IsValid(LevelSequenceClass)
		&& InSequence->GetClass()->IsChildOf(LevelSequenceClass);
}

bool FSequencerTrackFilter::SupportsUMGSequence(UMovieSceneSequence* const InSequence)
{
	static UClass* const WidgetAnimationClass = FindObject<UClass>(nullptr, TEXT("/Script/UMG.WidgetAnimation"), true);
	return IsValid(InSequence)
		&& IsValid(WidgetAnimationClass)
		&& InSequence->GetClass()->IsChildOf(WidgetAnimationClass);
}

UMovieSceneTrack* FSequencerTrackFilter::ResolveMovieSceneTrackObject(FSequencerTrackFilterType InNode, FSequencerFilterData& FilterData)
{
	if (!InNode.IsValid())
	{
		return nullptr;
	}

	const TWeakViewModelPtr<IOutlinerExtension> WeakOutlinerNode = InNode.ImplicitCast();

	// Use cache version if it exists, otherwise resolve below
	if (FilterData.ResolvedTrackObjects.Contains(WeakOutlinerNode))
	{
		if (FilterData.ResolvedTrackObjects[WeakOutlinerNode].IsValid())
		{
			return FilterData.ResolvedTrackObjects[WeakOutlinerNode].Get();
		}

		FilterData.ResolvedTrackObjects.Remove(WeakOutlinerNode);
	}

	UMovieSceneTrack* TrackObject = nullptr;

	if (const TViewModelPtr<ITrackExtension> AncestorTrackModel = InNode->FindAncestorOfType<ITrackExtension>(true))
	{
		TrackObject = AncestorTrackModel->GetTrack();
	}

	if (IsValid(TrackObject))
	{
		FilterData.ResolvedTrackObjects.Add(WeakOutlinerNode, TrackObject);
	}

	return TrackObject;
}

UObject* FSequencerTrackFilter::ResolveTrackBoundObject(ISequencer& InSequencer, FSequencerTrackFilterType InNode, FSequencerFilterData& FilterData)
{
	if (!InNode.IsValid())
	{
		return nullptr;
	}

	const TWeakViewModelPtr<IOutlinerExtension> WeakOutlinerNode = InNode.ImplicitCast();

	// Use cache version if it exists, otherwise resolve below
	if (FilterData.ResolvedBoundObjects.Contains(WeakOutlinerNode))
	{
		if (FilterData.ResolvedBoundObjects[WeakOutlinerNode].IsValid())
		{
			return FilterData.ResolvedBoundObjects[WeakOutlinerNode].Get();
		}

		FilterData.ResolvedBoundObjects.Remove(WeakOutlinerNode);
	}

	UObject* BoundObject = nullptr;

	if (const TViewModelPtr<IObjectBindingExtension> ObjectBindingModel = InNode->FindAncestorOfType<IObjectBindingExtension>(true))
	{
		BoundObject = InSequencer.FindSpawnedObjectOrTemplate(ObjectBindingModel->GetObjectGuid());
	}

	if (IsValid(BoundObject))
	{
		FilterData.ResolvedBoundObjects.Add(WeakOutlinerNode, BoundObject);
	}

	return BoundObject;
}

UObject* FSequencerTrackFilter::ResolveTrackBoundObject(const TViewModelPtr<FViewModel> InNode, FSequencerFilterData& FilterData) const
{
	return ResolveTrackBoundObject(GetSequencer(), InNode, FilterData);
}

FText FSequencerTrackFilter::BuildTooltipTextForCommand(const FText& InBaseText, const TSharedPtr<FUICommandInfo>& InCommand)
{
	const TSharedRef<const FInputChord> FirstValidChord = InCommand->GetFirstValidChord();
	if (FirstValidChord->IsValidChord())
	{
		return FText::Format(NSLOCTEXT("Sequencer", "TrackFilterTooltipText", "{0} ({1})"), InBaseText, FirstValidChord->GetInputText());
	}
	return InBaseText;
}

bool FSequencerTrackFilter::CanToggleFilter() const
{
	const FString FilterName = GetDisplayName().ToString();
	return FilterInterface.IsFilterActiveByDisplayName(FilterName);
}

void FSequencerTrackFilter::ToggleFilter()
{
	const FString FilterName = GetDisplayName().ToString();
	const bool bNewState = !FilterInterface.IsFilterActiveByDisplayName(FilterName);
	FilterInterface.SetFilterActiveByDisplayName(FilterName, bNewState);
}

void FSequencerTrackFilter::MapToggleAction(const TSharedPtr<FUICommandInfo>& InCommand)
{
	FilterInterface.GetCommandList()->MapAction(
		InCommand,
		FExecuteAction::CreateSP(this, &FSequencerTrackFilter::ToggleFilter),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSequencerTrackFilter::CanToggleFilter));
}
