// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubTrackEditorMode.h"

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "EditorViewportClient.h"
#include "MovieScene.h"
#include "SequencerChannelTraits.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Systems/MovieSceneTransformOriginSystem.h"
#include "Tracks/MovieSceneSubTrack.h"

FName FSubTrackEditorMode::ModeName("EditMode.SubTrackEditMode");

FSubTrackEditorMode::FSubTrackEditorMode()
{
	CachedLocation.Reset();
}

FSubTrackEditorMode::~FSubTrackEditorMode()
{
}

void FSubTrackEditorMode::Initialize()
{
	CachedLocation.Reset();
}

bool FSubTrackEditorMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if(AreAnyActorsSelected())
	{
		return false;
	}
	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
	const bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
	const bool bMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);
	const bool bAnyModifiers = bAltDown || bCtrlDown || bShiftDown;
	
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();

	if(bMouseButtonDown && !bAnyModifiers && CurrentAxis != EAxisList::None)
	{
		OnOriginValueChanged.Broadcast( InDrag, InRot);
		return true;
	}
	
	return false;
}

bool FSubTrackEditorMode::UsesTransformWidget() const
{
	const UMovieSceneSubSection* SubSection = GetSelectedSection();
	if(!AreAnyActorsSelected() && SubSection)
	{
		return DoesSubSectionHaveTransformOverrides(*SubSection);
	}
	return FEdMode::UsesTransformWidget();
}

bool FSubTrackEditorMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	const UMovieSceneSubSection* SubSection = GetSelectedSection();
	if(!AreAnyActorsSelected() && SubSection)
	{
		return DoesSubSectionHaveTransformOverrides(*SubSection);
	}
	return FEdMode::UsesTransformWidget(CheckMode);
}

TOptional<FMovieSceneSequenceID> FSubTrackEditorMode::GetSequenceIDForSubSeciton(const UMovieSceneSubSection* InSubSection) const
{
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	if(!Sequencer)
	{
		return TOptional<FMovieSceneSequenceID>();
	}
	
	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = Sequencer->GetEvaluationTemplate();
	
	UMovieSceneCompiledDataManager* CompiledDataManager = EvaluationTemplate.GetCompiledDataManager();
	UMovieSceneSequence* RootSequence = EvaluationTemplate.GetSequence(Sequencer->GetRootTemplateID());
	const FMovieSceneCompiledDataID DataID = CompiledDataManager->Compile(RootSequence);

	const FMovieSceneSequenceHierarchy& Hierarchy = CompiledDataManager->GetHierarchyChecked(DataID);
	
	TArray<FMovieSceneSequenceID> SubSequenceHierarchy = Sequencer->GetSubSequenceHierarchy();

	UE::MovieScene::FSubSequencePath Path;
	const FMovieSceneSequenceID ParentSequenceID = SubSequenceHierarchy.Last();

	Path.Reset(ParentSequenceID, &Hierarchy);

	return Path.ResolveChildSequenceID(InSubSection->GetSequenceID());
}

FTransform FSubTrackEditorMode::GetFinalTransformOriginForSubSection(const UMovieSceneSubSection* InSubSection) const
{
	FTransform TransformOrigin = FTransform();

	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	if(!Sequencer)
	{
		return TransformOrigin;
	}

	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = Sequencer->GetEvaluationTemplate();

	const TOptional<FMovieSceneSequenceID> ChildSequenceID = GetSequenceIDForSubSeciton(InSubSection);
	
	const UMovieSceneEntitySystemLinker* EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
	if(!EntityLinker || !ChildSequenceID)
	{
		return TransformOrigin;
	}

	const UMovieSceneTransformOriginSystem* TransformOriginSystem = EntityLinker->FindSystem<UMovieSceneTransformOriginSystem>();

	if(!TransformOriginSystem)
	{
		return TransformOrigin;
	}
	
	const TSparseArray<FTransform>& TransformOrigins = TransformOriginSystem->GetTransformOriginsByInstanceID();
	const TMap<FMovieSceneSequenceID, UE::MovieScene::FInstanceHandle> SequenceIDToInstanceHandle = TransformOriginSystem->GetSequenceIDToInstanceHandle();

	if(SequenceIDToInstanceHandle.Contains(ChildSequenceID.GetValue()))
	{
		const UE::MovieScene::FInstanceHandle CurrentHandle = SequenceIDToInstanceHandle[ChildSequenceID.GetValue()];
		if(TransformOrigins.IsValidIndex(CurrentHandle.InstanceID))
		{
			TransformOrigin = TransformOrigins[CurrentHandle.InstanceID];
		}
	}

	return TransformOrigin;
}

bool FSubTrackEditorMode::AreAnyActorsSelected() const
{
	FSelectedActorIterator SelectedActorIter(GetWorld());
	if(SelectedActorIter)
	{
		return true;
	}
	return false;
}

FVector FSubTrackEditorMode::GetWidgetLocation() const
{
	const UMovieSceneSubSection* SubSection = GetSelectedSection();
	if(!AreAnyActorsSelected() && SubSection && DoesSubSectionHaveTransformOverrides(*SubSection))
	{
		FVector NewLocation = GetFinalTransformOriginForSubSection(SubSection).GetLocation();

		if(!CachedLocation.IsSet() || !NewLocation.Equals(CachedLocation.GetValue()))
		{
			CachedLocation = NewLocation;
			// Invalidate hit proxies, otherwise the hit proxy for the widget can be out of sync, and still at the old widget location
			GEditor->RedrawLevelEditingViewports(true);
		}
		return CachedLocation.GetValue();
	}
	
	return FEdMode::GetWidgetLocation();
}

bool FSubTrackEditorMode::ShouldDrawWidget() const
{
	const UMovieSceneSubSection* SubSection = GetSelectedSection();
	if(!AreAnyActorsSelected() && SubSection && DoesSubSectionHaveTransformOverrides(*SubSection))
	{
		return true;
	}
	// If the widget is not being drawn, its hit proxies need to be invalidated the next time it is drawn.
	// Resetting the cached location will trigger the invalidation in GetWidgetLocation
	CachedLocation.Reset();
	return false;
}

bool FSubTrackEditorMode::GetPivotForOrbit(FVector& OutPivot) const
{
	return FEdMode::GetPivotForOrbit(OutPivot);
}

bool FSubTrackEditorMode::GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	// @todo support custom coordinate systems based on parent rotation.
	return false;
}

bool FSubTrackEditorMode::GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	return FEdMode::GetCustomInputCoordinateSystem(OutMatrix, InData);
}

bool FSubTrackEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	if(IncompatibleEditorModes.Contains(OtherModeID))
	{
		return false;
	}
	return true;
}

bool FSubTrackEditorMode::DoesSubSectionHaveTransformOverrides(const UMovieSceneSubSection& SubSection)
{
	if (SubSection.IsActive())
	{
		const EMovieSceneTransformChannel SectionTransformChannels = SubSection.GetMask().GetChannels();

		return EnumHasAnyFlags(SectionTransformChannels, EMovieSceneTransformChannel::Translation) || EnumHasAnyFlags(SectionTransformChannels, EMovieSceneTransformChannel::Rotation);
	}
	
	return false;
}

UMovieSceneSubSection* FSubTrackEditorMode::GetSelectedSection() const
{
	const TSharedPtr<ISequencer> PinnedSequencer = WeakSequencer.Pin();
	UMovieSceneSubSection* SelectedSection = nullptr;
	if(!PinnedSequencer.IsValid())
	{
		return SelectedSection;
	}

	TArray<UMovieSceneSection*> SelectedSections;
	PinnedSequencer->GetSelectedSections(SelectedSections);
	
	for(UMovieSceneSection* Section : SelectedSections)
	{
		if(UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
		{
			// Mirror behavior when multiple actors are selected in the level editor, and pick the last selected item.
			SelectedSection = SubSection;
		}
	}

	if(SelectedSection)
	{
		return SelectedSection;
	}

	TArray<UMovieSceneTrack*> SelectedTracks;
	PinnedSequencer->GetSelectedTracks(SelectedTracks);
	for(UMovieSceneTrack* Track : SelectedTracks)
	{
		// Similarly to section selection, pick the last selected track.
		if(const UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track))
		{
			if(SubTrack->GetSectionToKey())
			{
				SelectedSection = Cast<UMovieSceneSubSection>(SubTrack->GetSectionToKey());
			}
			else if(SubTrack->GetAllSections().Num())
			{
				// Since the first section is the section that will be keyed by default, select the first section from the track.
				SelectedSection = Cast<UMovieSceneSubSection>(SubTrack->FindSection(PinnedSequencer.Get()->GetLocalTime().Time.FrameNumber));
			}
		}
			
	}
	return SelectedSection;
}
