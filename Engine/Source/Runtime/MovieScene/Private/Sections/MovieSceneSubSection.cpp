// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTimeHelpers.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Misc/FrameRate.h"
#include "Logging/MessageLog.h"
#include "MovieSceneTransformTypes.h"
#include "Evaluation/MovieSceneRootOverridePath.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Tracks/MovieSceneTimeWarpTrack.h"
#include "Sections/MovieSceneSectionTimingParameters.h"
#include "Variants/MovieSceneTimeWarpGetter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSubSection)

float DeprecatedMagicNumber = TNumericLimits<float>::Lowest();

/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection(const FObjectInitializer& ObjInitializer)
	: Super(ObjInitializer)
	, StartOffset_DEPRECATED(DeprecatedMagicNumber)
	, TimeScale_DEPRECATED(DeprecatedMagicNumber)
	, PrerollTime_DEPRECATED(DeprecatedMagicNumber)
{
	NetworkMask = (uint8)(EMovieSceneServerClientMask::Server | EMovieSceneServerClientMask::Client);

	SetBlendType(EMovieSceneBlendType::Absolute);
}

void UMovieSceneSubSection::DeleteChannels(TArrayView<const FName> ChannelNames)
{
	bool bDeletedAny = false;

	if (Parameters.TimeScale.GetType() == EMovieSceneTimeWarpType::Custom && TryModify())
	{
		if (UMovieSceneTimeWarpGetter* Getter = Parameters.TimeScale.AsCustom())
		{
			for (FName ChannelName : ChannelNames)
			{
				bDeletedAny |= Getter->DeleteChannel(Parameters.TimeScale, ChannelName);
			}
		}
	}

	if (bDeletedAny)
	{
		ChannelProxy = nullptr;
	}
}

EMovieSceneChannelProxyType UMovieSceneSubSection::CacheChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

	if (Parameters.TimeScale.GetType() == EMovieSceneTimeWarpType::Custom)
	{
		if (UMovieSceneTimeWarpGetter* Curve = Parameters.TimeScale.AsCustom())
		{
			Curve->PopulateChannelProxy(Channels, UMovieSceneTimeWarpGetter::EAllowTopLevelChannels::No);
		}
	}

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
	return EMovieSceneChannelProxyType::Dynamic;
}


FMovieSceneSequenceTransform UMovieSceneSubSection::OuterToInnerTransform_NoInnerTimeWarp() const
{
	UMovieSceneSequence* SequencePtr   = GetSequence();
	if (!SequencePtr)
	{
		return FMovieSceneSequenceTransform();
	}

	UMovieScene* MovieScenePtr = SequencePtr->GetMovieScene();

	TRange<FFrameNumber> SubRange = GetRange();
	if (!MovieScenePtr || SubRange.GetLowerBound().IsOpen())
	{
		return FMovieSceneSequenceTransform();
	}

	const FFrameRate   InnerFrameRate = MovieScenePtr->GetTickResolution();
	const FFrameRate   OuterFrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	const TRange<FFrameNumber> MovieScenePlaybackRange = GetValidatedInnerPlaybackRange(Parameters, *MovieScenePtr);

	FMovieSceneSectionTimingParametersFrames TimingParams = {
		Parameters.TimeScale.ShallowCopy(),
		Parameters.StartFrameOffset,
		Parameters.EndFrameOffset,
		Parameters.FirstLoopStartFrameOffset,
		Parameters.bCanLoop,
		false, // do not clamp sub-sections by default
		false
	};

	return TimingParams.MakeTransform(OuterFrameRate, SubRange, InnerFrameRate, MovieScenePtr->GetPlaybackRange());
}

FMovieSceneSequenceTransform UMovieSceneSubSection::OuterToInnerTransform() const
{
	FMovieSceneSequenceTransform OuterToInner = OuterToInnerTransform_NoInnerTimeWarp();
	AppendInnerTimeWarpTransform(OuterToInner);
	return OuterToInner;
}

void UMovieSceneSubSection::AppendInnerTimeWarpTransform(FMovieSceneSequenceTransform& OutTransform) const
{
	UMovieSceneSequence* Sequence   = GetSequence();
	UMovieScene*         MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

	if (!MovieScene)
	{
		return;
	}

	// Look for any time warp tracks inside the sub sequence
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		UMovieSceneTimeWarpTrack* TimeWarpTrack = Cast<UMovieSceneTimeWarpTrack>(Track);
		if (TimeWarpTrack && !TimeWarpTrack->IsEvalDisabled())
		{
			FMovieSceneNestedSequenceTransform TimeWarpTransform = TimeWarpTrack->GenerateTransform();

			if (!TimeWarpTransform.IsIdentity())
			{
				if (TimeWarpTransform.IsLinear() && OutTransform.IsLinear())
				{
					OutTransform = FMovieSceneSequenceTransform(OutTransform.AsLinear() * TimeWarpTransform.AsLinear());
				}
				else
				{
					OutTransform.NestedTransforms.Add(TimeWarpTransform);
				}
			}

			// Only 1 timewarp track supported
			return;
		}
	}
}

bool UMovieSceneSubSection::GetValidatedInnerPlaybackRange(TRange<FFrameNumber>& OutInnerPlaybackRange) const
{
	UMovieSceneSequence* SequencePtr = GetSequence();
	if (SequencePtr != nullptr)
	{
		UMovieScene* MovieScenePtr = SequencePtr->GetMovieScene();
		if (MovieScenePtr != nullptr)
		{
			OutInnerPlaybackRange = GetValidatedInnerPlaybackRange(Parameters, *MovieScenePtr);
			return true;
		}
	}
	return false;
}

TRange<FFrameNumber> UMovieSceneSubSection::GetValidatedInnerPlaybackRange(const FMovieSceneSectionParameters& SubSectionParameters, const UMovieScene& InnerMovieScene)
{
	const TRange<FFrameNumber> InnerPlaybackRange = InnerMovieScene.GetPlaybackRange();
	TRangeBound<FFrameNumber> ValidatedLowerBound = InnerPlaybackRange.GetLowerBound();
	TRangeBound<FFrameNumber> ValidatedUpperBound = InnerPlaybackRange.GetUpperBound();
	if (ValidatedLowerBound.IsClosed() && ValidatedUpperBound.IsClosed())
	{
		const FFrameRate TickResolution = InnerMovieScene.GetTickResolution();
		const FFrameRate DisplayRate = InnerMovieScene.GetDisplayRate();
		const FFrameNumber OneFrameInTicks = FFrameRate::TransformTime(FFrameTime(1), DisplayRate, TickResolution).FloorToFrame();

		ValidatedLowerBound.SetValue(ValidatedLowerBound.GetValue() + SubSectionParameters.StartFrameOffset);
		ValidatedUpperBound.SetValue(FMath::Max(ValidatedUpperBound.GetValue() - SubSectionParameters.EndFrameOffset, ValidatedLowerBound.GetValue() + OneFrameInTicks));
		return TRange<FFrameNumber>(ValidatedLowerBound, ValidatedUpperBound);
	}
	return InnerPlaybackRange;
}

FString UMovieSceneSubSection::GetPathNameInMovieScene() const
{
	UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
	check(OuterMovieScene);
	return GetPathName(OuterMovieScene);
}

FMovieSceneSequenceID UMovieSceneSubSection::GetSequenceID() const
{
	FString FullPath = GetPathNameInMovieScene();
	if (SubSequence)
	{
		FullPath += TEXT(" / ");
		FullPath += SubSequence->GetPathName();
	}

	return FMovieSceneSequenceID(FCrc::Strihash_DEPRECATED(*FullPath));
}

void UMovieSceneSubSection::PostLoad()
{
	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	TOptional<double> StartOffsetToUpgrade;
	if (StartOffset_DEPRECATED != DeprecatedMagicNumber)
	{
		StartOffsetToUpgrade = StartOffset_DEPRECATED;

		StartOffset_DEPRECATED = DeprecatedMagicNumber;
	}
	else if (Parameters.StartOffset_DEPRECATED != 0.f)
	{
		StartOffsetToUpgrade = Parameters.StartOffset_DEPRECATED;
	}

	if (StartOffsetToUpgrade.IsSet())
	{
		FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartOffsetToUpgrade.GetValue());
		Parameters.StartFrameOffset = StartFrame;
	}

	if (TimeScale_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.TimeScale = TimeScale_DEPRECATED;

		TimeScale_DEPRECATED = DeprecatedMagicNumber;
	}

	if (PrerollTime_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.PrerollTime_DEPRECATED = PrerollTime_DEPRECATED;

		PrerollTime_DEPRECATED = DeprecatedMagicNumber;
	}

	// Pre and post roll is now supported generically
	if (Parameters.PrerollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPreRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PrerollTime_DEPRECATED);
		SetPreRollFrames(ClampedPreRollFrames.Value);
	}

	if (Parameters.PostrollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPostRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PostrollTime_DEPRECATED);
		SetPreRollFrames(ClampedPostRollFrames.Value);
	}

	Super::PostLoad();
}

bool UMovieSceneSubSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	if (SubSequence)
	{
		const int32 EntityIndex   = OutFieldBuilder->FindOrAddEntity(this, 0);
		const int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);
		OutFieldBuilder->AddPersistentEntity(EffectiveRange, EntityIndex, MetaDataIndex);
	}

	return true;
}

void UMovieSceneSubSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	OutImportedEntity->AddBuilder(
		FEntityBuilder().AddTag(FBuiltInComponentTypes::Get()->Tags.Root)
	);

	BuildDefaultSubSectionComponents(EntityLinker, Params, OutImportedEntity);
}

void UMovieSceneSubSection::SetSequence(UMovieSceneSequence* Sequence)
{
	if (!TryModify())
	{
		return;
	}

	SubSequence = Sequence;

#if WITH_EDITOR
	OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
#endif
}

UMovieSceneSequence* UMovieSceneSubSection::GetSequence() const
{
	return SubSequence;
}

#if WITH_EDITOR
void UMovieSceneSubSection::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		// Store the current subsequence in case it needs to be restored in PostEditChangeProperty because the new value would introduce a circular dependency
		PreviousSubSequence = SubSequence;
	}

	return Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSubSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		// Check whether the subsequence that was just set has tracks that contain the sequence that this subsection is in.
		UMovieScene* SubSequenceMovieScene = SubSequence ? SubSequence->GetMovieScene() : nullptr;

		UMovieSceneSubTrack* TrackOuter = Cast<UMovieSceneSubTrack>(GetOuter());

		if (SubSequenceMovieScene && TrackOuter)
		{
			if (UMovieSceneSequence* CurrentSequence = TrackOuter->GetTypedOuter<UMovieSceneSequence>())
			{
				TArray<UMovieSceneSubTrack*> SubTracks;

				for (UMovieSceneTrack* Track : SubSequenceMovieScene->GetTracks())
				{
					if (UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track))
					{
						SubTracks.Add(SubTrack);
					}
				}

				for (const FMovieSceneBinding& Binding : SubSequenceMovieScene->GetBindings())
				{
					for (UMovieSceneTrack* Track : SubSequenceMovieScene->FindTracks(UMovieSceneSubTrack::StaticClass(), Binding.GetObjectGuid()))
					{
						if (UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track))
						{
							SubTracks.Add(SubTrack);
						}
					}
				}

				for (UMovieSceneSubTrack* SubTrack : SubTracks)
				{
					if ( SubTrack->ContainsSequence(*CurrentSequence, true))
					{
						UE_LOG(LogMovieScene, Error, TEXT("Invalid level sequence %s. It is already contained by: %s."), *SubSequence->GetDisplayName().ToString(), *CurrentSequence->GetDisplayName().ToString());

						// Restore to the previous sub sequence because there was a circular dependency
						SubSequence = PreviousSubSequence;
						break;
					}
				}
			}
		}

		PreviousSubSequence = nullptr;
	}

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMovieSceneSectionParameters, TimeScale))
	{
		ChannelProxy = nullptr;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// recreate runtime instance when sequence is changed
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
	}
}
#endif

TOptional<TRange<FFrameNumber> > UMovieSceneSubSection::GetAutoSizeRange() const
{
	UMovieScene* MovieScene = SubSequence ? SubSequence->GetMovieScene() : nullptr;
	if (MovieScene)
	{
		// We probably want to just auto-size the section to the sub-sequence's scaled playback range... if this section
		// is looping, however, it's hard to know what we want to do. Let's just size it to one loop.
		const FMovieSceneInverseSequenceTransform InnerToOuter = OuterToInnerTransform().Inverse();
		const TRange<FFrameNumber> InnerPlaybackRange = UMovieSceneSubSection::GetValidatedInnerPlaybackRange(Parameters, *MovieScene);

		const FFrameTime IncAutoStartTime = InnerToOuter.TryTransformTime(UE::MovieScene::DiscreteInclusiveLower(InnerPlaybackRange)).Get(InnerPlaybackRange.GetLowerBoundValue());
		const FFrameTime ExcAutoEndTime   = InnerToOuter.TryTransformTime(UE::MovieScene::DiscreteExclusiveUpper(InnerPlaybackRange)).Get(InnerPlaybackRange.GetUpperBoundValue());

		return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + (ExcAutoEndTime.RoundToFrame() - IncAutoStartTime.RoundToFrame()));
	}

	return Super::GetAutoSizeRange();
}

void UMovieSceneSubSection::TrimSection( FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	TRange<FFrameNumber> InitialRange = GetRange();
	if ( !InitialRange.Contains( TrimTime.Time.GetFrame() ) )
	{
		return;
	}

	SetFlags(RF_Transactional);
	if (!TryModify())
	{
		return;
	}

	// If trimming off the left, set the offset of the shot
	if (bTrimLeft && InitialRange.GetLowerBound().IsClosed() && GetSequence())
	{
		// Sections need their offsets calculated in their local resolution. Different sequences can have different tick resolutions 
		// so we need to transform from the parent resolution to the local one before splitting them.
		UMovieScene* LocalMovieScene = GetSequence()->GetMovieScene();
		const FFrameRate LocalTickResolution = LocalMovieScene->GetTickResolution();
		const FFrameTime LocalTickResolutionTrimTime = FFrameRate::TransformTime(TrimTime.Time, TrimTime.Rate, LocalTickResolution);

		// The new first loop start offset is where the trim time fell inside the sub-sequence (this time is already
		// normalized in the case of looping sub-sequences).
		const FMovieSceneSequenceTransform OuterToInner(OuterToInnerTransform());
		const FFrameTime LocalTrimTime = OuterToInner.TransformTime(LocalTickResolutionTrimTime);
		// LocalTrimTime is now in the inner sequence timespace, but StartFrameOffset is an offset from the inner sequence's own
		// playback start time, so we need to account for that.
		TRange<FFrameNumber> LocalPlaybackRange = LocalMovieScene->GetPlaybackRange();
		const FFrameNumber LocalPlaybackStart = LocalPlaybackRange.HasLowerBound() ? LocalPlaybackRange.GetLowerBoundValue() : FFrameNumber(0);
		FFrameNumber NewStartOffset = LocalTrimTime.FrameNumber - LocalPlaybackStart;

		// Make sure we don't have negative offsets (this shouldn't happen, though).
		NewStartOffset = FMath::Max(FFrameNumber(0), NewStartOffset);
		
		const bool bCanLoop = Parameters.bCanLoop;
		if (!bCanLoop)
		{
			Parameters.StartFrameOffset = NewStartOffset;
		}
		else
		{
			Parameters.FirstLoopStartFrameOffset = NewStartOffset;
		}
	}

	// Actually trim the section range!
	UMovieSceneSection::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
}

void UMovieSceneSubSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	using namespace UE::MovieScene;

	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame   = GetExclusiveEndFrame();

	UMovieSceneSequence* Sequence = GetSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	auto VisitBoundary = [&OutSnapTimes](FFrameTime InTime)
	{
		OutSnapTimes.Add(InTime.RoundToFrame());
		return true;
	};

	FMovieSceneSequenceTransform OuterToInner = OuterToInnerTransform();

	if (!OuterToInner.ExtractBoundariesWithinRange(StartFrame, EndFrame, VisitBoundary))
	{
		FMovieSceneInverseSequenceTransform InnerToOuterTransform = OuterToInner.Inverse();

		TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

		TOptional<FFrameTime> SequenceStart = InnerToOuterTransform.TryTransformTime(PlaybackRange.GetLowerBoundValue());
		TOptional<FFrameTime> SequenceEnd   = InnerToOuterTransform.TryTransformTime(PlaybackRange.GetUpperBoundValue());

		if (SequenceStart && SequenceStart.GetValue() >= StartFrame && SequenceStart.GetValue() < EndFrame)
		{
			VisitBoundary(SequenceStart.GetValue());
		}

		if (SequenceEnd && SequenceEnd.GetValue() >= StartFrame && SequenceEnd.GetValue() < EndFrame)
		{
			VisitBoundary(SequenceEnd.GetValue());
		}
	}
}

void UMovieSceneSubSection::MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	if (Parameters.StartFrameOffset.Value > 0)
	{
		FFrameNumber NewStartFrameOffset = ConvertFrameTime(FFrameTime(Parameters.StartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.StartFrameOffset = NewStartFrameOffset;
	}
	if (Parameters.EndFrameOffset.Value > 0)
	{
		FFrameNumber NewEndFrameOffset = ConvertFrameTime(FFrameTime(Parameters.EndFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.EndFrameOffset = NewEndFrameOffset;
	}
	if (Parameters.FirstLoopStartFrameOffset.Value > 0)
	{
		FFrameNumber NewFirstLoopStartFrameOffset = ConvertFrameTime(FFrameTime(Parameters.FirstLoopStartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.FirstLoopStartFrameOffset = NewFirstLoopStartFrameOffset;
	}
}

FMovieSceneSubSequenceData UMovieSceneSubSection::GenerateSubSequenceData(const FSubSequenceInstanceDataParams& Params) const
{
	return FMovieSceneSubSequenceData(*this);
}

FFrameNumber UMovieSceneSubSection::MapTimeToSectionFrame(FFrameTime InPosition) const
{
	FFrameNumber LocalPosition = ((InPosition - Parameters.StartFrameOffset) * OuterToInnerTransform()).GetFrame();
	return LocalPosition;
}

void UMovieSceneSubSection::BuildDefaultSubSectionComponents(UMovieSceneEntitySystemLinker* EntityLinker, const UE::MovieScene::FEntityImportParams& Params, UE::MovieScene::FImportedEntity* OutImportedEntity) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

	const bool bHasEasing = Easing.GetEaseInDuration() > 0 || Easing.GetEaseOutDuration() > 0;

	const FSubSequencePath PathToRoot = EntityLinker->GetInstanceRegistry()->GetInstance(Params.Sequence.InstanceHandle).GetSubSequencePath();
	FMovieSceneSequenceID ResolvedSequenceID = PathToRoot.ResolveChildSequenceID(this->GetSequenceID());

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.Add(Components->SequenceID, ResolvedSequenceID)
		.AddTag(Components->Tags.SubInstance)
		.AddConditional(Components->HierarchicalEasingProvider, ResolvedSequenceID, bHasEasing)
	);
}


