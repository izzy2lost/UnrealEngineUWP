// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/TimeWarpTrackEditor.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "SequencerSectionPainter.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Sections/MovieSceneTimeWarpSection.h"

#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/EditorSharedViewModelData.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

#define LOCTEXT_NAMESPACE "TimeWarpTrackEditor"

namespace UE::Sequencer
{

struct FTimeWarpSection : FSequencerSection
{
	FTimeWarpSection(UMovieSceneSection& InSection)
		: FSequencerSection(InSection)
	{}

	int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		TSharedPtr<FEditorSharedViewModelData> SharedEditorData = CastViewModel<FEditorSharedViewModelData>(InPainter.SectionModel->GetSharedData());
		TSharedPtr<FSequencerEditorViewModel>  SequencerEditor  = SharedEditorData ? CastViewModel<FSequencerEditorViewModel>(SharedEditorData->GetEditor()) : nullptr;
		if (!SequencerEditor)
		{
			return InPainter.LayerId;
		}

		InPainter.LayerId = InPainter.PaintSectionBackground();

		TSharedPtr<ISequencer> Sequencer = SequencerEditor->GetSequencer();

		UMovieSceneTimeWarpSection* TimeWarpSection = Cast<UMovieSceneTimeWarpSection>(InPainter.SectionModel->GetSection());
		if (TimeWarpSection)
		{
			// Paint the unwarped current time
			FFrameTime LocalTime = Sequencer->GetLocalTime().Time;
			FFrameTime NewLocalTime = LocalTime;

			FMovieSceneInverseNestedSequenceTransform Inverse = TimeWarpSection->GenerateTransform().Inverse();

			if (Inverse.IsLinear())
			{
				NewLocalTime = LocalTime * Inverse.AsLinear();
			}
			else
			{
				FMovieSceneSequenceTransform Transform = Sequencer->GetFocusedMovieSceneSequenceTransform();

				// Time warp track transforms are always added last
				if (Transform.NestedTransforms.Num() > 0)
				{
					Transform.NestedTransforms.RemoveAt(Transform.NestedTransforms.Num()-1);
				}

				NewLocalTime = Sequencer->GetGlobalTime().Time * Transform;
			}

			if (LocalTime != NewLocalTime)
			{
				const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
				float PixelPosition = InPainter.GetTimeConverter().SecondsToPixel(NewLocalTime / Sequencer->GetFocusedTickResolution());

				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId++,
					InPainter.SectionGeometry.ToPaintGeometry(
						FVector2f(1.0f, InPainter.SectionGeometry.Size.Y),
						FSlateLayoutTransform(FVector2f(PixelPosition, 0.f))
					),
					FAppStyle::GetBrush("WhiteBrush"),
					DrawEffects,
					FColor(255, 255, 255, 128)	// 0, 75, 50 (HSV)
				);
			}
		}

		return InPainter.LayerId;
	}
};

} // namespace UE::Sequencer

void FTimeWarpTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddTimeWarpTrack", "Time Warp"),
		LOCTEXT("AddTimeWarpTrackTooltip", "Adds a new track that manipulates the time of the current sequence."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Slomo"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FTimeWarpTrackEditor::HandleAddTimeWarpTrack)
		)
	);
}

TSharedRef<ISequencerSection> FTimeWarpTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShared<UE::Sequencer::FTimeWarpSection>(SectionObject);
}

void FTimeWarpTrackEditor::HandleAddTimeWarpTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTimeWarpTrack_Transaction", "Add Time Warp Track"));

	FocusedMovieScene->Modify();

	UMovieSceneTimeWarpTrack* NewTrack = NewObject<UMovieSceneTimeWarpTrack>(FocusedMovieScene, NAME_None, RF_Transactional);
	NewTrack->AddSection(*NewTrack->CreateNewSection());

	FocusedMovieScene->AddGivenTrack(NewTrack);
	SequencerPtr->OnAddTrack(NewTrack, FGuid());
}

#undef LOCTEXT_NAMESPACE