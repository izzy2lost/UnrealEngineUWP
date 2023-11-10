// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/BindingLifetimeTrackModel.h"

#include "MovieSceneTrack.h"
#include "Templates/Casts.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"

namespace UE
{
	namespace Sequencer
	{

		TSharedPtr<FTrackModel> FBindingLifetimeTrackModel::CreateTrackModel(UMovieSceneTrack* Track)
		{
			if (UMovieSceneBindingLifetimeTrack* BindingLifetimeTrack = Cast<UMovieSceneBindingLifetimeTrack>(Track))
			{
				return MakeShared<FBindingLifetimeTrackModel>(BindingLifetimeTrack);
			}
			return nullptr;
		}

		FBindingLifetimeTrackModel::FBindingLifetimeTrackModel(UMovieSceneBindingLifetimeTrack* Track)
			: FTrackModel(Track)
		{
		}

		void FBindingLifetimeTrackModel::OnConstruct()
		{
			FTrackModel::OnConstruct();
			RecalculateInverseLifetimeRange();
		}

		void FBindingLifetimeTrackModel::RecalculateInverseLifetimeRange()
		{
			InverseLifetimeRange.Reset();
			InverseLifetimeRange.Add(TRange<FFrameNumber>::All());
			// Iterate through our sections, removing them from the range

			auto RemoveRangeFromSet = [&](TRange<FFrameNumber> SectionRange) {
				for (int32 Index = 0; Index < InverseLifetimeRange.Num(); ++Index)
				{
					const TRange<FFrameNumber>& Current = InverseLifetimeRange[Index];
					if (Current.Overlaps(SectionRange))
					{
						// Special case that difference doesn't handle well
						if (!SectionRange.HasLowerBound() && !SectionRange.HasUpperBound())
						{
							InverseLifetimeRange.Reset();
							return;
						}

						TArray<TRange<FFrameNumber>> SplitRanges = TRange<FFrameNumber>::Difference(Current, SectionRange);
						for (const TRange<FFrameNumber>& NewRange : SplitRanges)
						{
							// Splitting infinite ranges keeps an infinite range, which we don't want to keep
							if (!NewRange.HasLowerBound() && !NewRange.HasUpperBound())
							{
								continue;
							}
							
							InverseLifetimeRange.Add(NewRange);
						}
						InverseLifetimeRange.RemoveAtSwap(Index--);
					}
				}
			};

			for (const TViewModelPtr<FSectionModel>& Item : GetSectionModels().IterateSubList<FSectionModel>())
			{
				RemoveRangeFromSet(Item->GetRange());
			}
		}

		FSortingKey FBindingLifetimeTrackModel::GetSortingKey() const
		{
			FSortingKey SortingKey;
			if (UMovieSceneTrack* Track = GetTrack())
			{
				SortingKey.CustomOrder = Track->GetSortingOrder();
			}
			return SortingKey.PrioritizeBy(4);
		}

		void FBindingLifetimeTrackModel::OnDeferredModifyFlush()
		{
			// Do the child update
			FTrackModel::OnDeferredModifyFlush();

			// Recalculate our valid range
			RecalculateInverseLifetimeRange();
		}


	} // namespace Sequencer
} // namespace UE

