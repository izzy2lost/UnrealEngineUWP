// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveKeySelection.h"

#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "EaseCurveTool/AvaEaseCurveTangents.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "IKeyArea.h"
#include "MVVM/ViewModels/ChannelModel.h"

using namespace UE::Sequencer;

FAvaEaseCurveKeySelection::FAvaEaseCurveKeySelection(const TWeakPtr<FSequencerSelection>& InSequencerSelectionWeak)
	: SequencerSelectionWeak(InSequencerSelectionWeak)
{
	const TSharedPtr<FSequencerSelection> SequencerSelection = InSequencerSelectionWeak.Pin();
	if (!SequencerSelection.IsValid())
	{
		return;
	}

	for (const FKeyHandle Key : SequencerSelection->KeySelection)
	{
		if (Key == FKeyHandle::Invalid())
		{
			continue;
		}

		const TViewModelPtr<FChannelModel> ChannelModel = SequencerSelection->KeySelection.GetModelForKey(Key);
		if (!ChannelModel.IsValid())
		{
			continue;
		}

		const TSharedPtr<IKeyArea> KeyArea = ChannelModel->GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

		const FMovieSceneChannelHandle& ChannelHandle = KeyArea->GetChannel();

		FChannelData& Entry = ChannelKeyData.FindOrAdd(ChannelModel->GetChannelName());
		Entry.ChannelModel = ChannelModel;
		Entry.Section = ChannelModel->GetSection();
		Entry.Channel = ChannelHandle;
		Entry.KeyHandles.Add(Key);

		TotalSelectedKeys++;
		
		const int32 AllKeyCount = ChannelHandle.Get()->GetNumKeys();
		const int32 SelectedKeyCount = Entry.KeyHandles.Num();
		
		if (SelectedKeyCount == 1 && TotalSelectedKeys == 1)
		{
			const int32 KeyIndex = ChannelHandle.Get()->GetIndex(Entry.KeyHandles[0]);
			if (KeyIndex == AllKeyCount - 1)
			{
				bIsLastOnlySelectedKey = true;
			}
		}
		else if (SelectedKeyCount > 1)
		{
			bAllChannelSingleKeySelections = false;
		}
	}
}

void FAvaEaseCurveKeySelection::ForEachEaseableKey(const bool bInIncludeEqualValueKeys
	, const TFunctionRef<bool(const FKeyHandle& /*InKeyHandle*/, const FKeyHandle& /*InNextKeyHandle*/, const FChannelData&)>& InCallable)
{
	for (const TPair<FName, FChannelData>& ChannelEntry : ChannelKeyData)
	{
		const FName ChannelTypeName = ChannelEntry.Value.Channel.GetChannelTypeName();
		if (ChannelTypeName == FMovieSceneDoubleChannel::StaticStruct()->GetFName())
		{
			if (!CheckMatchingValues<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(bInIncludeEqualValueKeys, ChannelEntry.Value, InCallable))
			{
				return;
			}
		}
		else if (ChannelTypeName == FMovieSceneFloatChannel::StaticStruct()->GetFName())
		{
			if (!CheckMatchingValues<FMovieSceneFloatChannel, FMovieSceneFloatValue>(bInIncludeEqualValueKeys, ChannelEntry.Value, InCallable))
			{
				return;
			}
		}
	}
}

namespace UE::Ava::EaseCurveTool::Private
{
	template <class ChannelHandle, class ChannelValue>
	void UpdateChannelValues(const FKeyHandle& InKeyHandle, const FKeyHandle& InNextKeyHandle
		, const FAvaEaseCurveKeySelection::FChannelData& InChannelData
		, const bool bInAutoFlipTangents
		, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution
		, TArray<FAvaEaseCurveTangents>& OutKeySetTangents
		, TArray<FAvaEaseCurveTangents>& OutChangingTangents)
	{
		TMovieSceneChannelHandle<ChannelHandle> Channel = InChannelData.Channel.Cast<ChannelHandle>();
		TMovieSceneChannelData<ChannelValue> ChannelData = Channel.Get()->GetData();

		const TArrayView<ChannelValue> ChannelValues = ChannelData.GetValues();
		const TArrayView<FFrameNumber> ChannelTimes = ChannelData.GetTimes();

		const int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
		const int32 NextKeyIndex = ChannelData.GetIndex(InNextKeyHandle);

		// If there is a key frame after this key frame that we are editing, we check if the that key frame value is less
		// than or greater than this key frame value. If less, flip the tangent (if option is set).
		const bool bIncreasingValue = ChannelValues[NextKeyIndex].Value >= ChannelValues[KeyIndex].Value;

		FAvaEaseCurveTangents Tangents = FAvaEaseCurveTangents(ChannelValues[KeyIndex], ChannelValues[NextKeyIndex]);

		if (bInAutoFlipTangents && !bIncreasingValue)
		{
			Tangents.Start *= -1.f;
			Tangents.End *= -1.f;
		}

		// Scale time/value to normalized tangent range
		FAvaEaseCurveTangents ScaledTangents = Tangents;
		ScaledTangents.Normalize(ChannelTimes[KeyIndex], ChannelValues[KeyIndex].Value
			, ChannelTimes[NextKeyIndex], ChannelValues[NextKeyIndex].Value
			, InDisplayRate, InTickResolution);

		OutKeySetTangents.Add(ScaledTangents);
		if (ChannelValues[KeyIndex].Value != ChannelValues[NextKeyIndex].Value)
		{
			OutChangingTangents.Add(ScaledTangents);
		}
	}
}

FAvaEaseCurveTangents FAvaEaseCurveKeySelection::AverageTangents(const FFrameRate& InDisplayRate
	, const FFrameRate& InTickResolution
	, const bool bInAutoFlipTangents)
{
	using namespace UE::Ava::EaseCurveTool::Private;
	
	TArray<FAvaEaseCurveTangents> KeySetTangents;
	TArray<FAvaEaseCurveTangents> ChangingTangents;

	ForEachEaseableKey(/*bInIncludeEqualValueKeys=*/true, [&InDisplayRate, &InTickResolution, bInAutoFlipTangents, &KeySetTangents, &ChangingTangents]
		(const FKeyHandle& InKeyHandle, const FKeyHandle& InNextKeyHandle, const FAvaEaseCurveKeySelection::FChannelData& InChannelData)
		{
			const FName ChannelTypeName = InChannelData.Channel.GetChannelTypeName();
			if (ChannelTypeName == FMovieSceneDoubleChannel::StaticStruct()->GetFName())
			{
				UpdateChannelValues<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(InKeyHandle, InNextKeyHandle, InChannelData
					, bInAutoFlipTangents, InDisplayRate, InTickResolution, KeySetTangents, ChangingTangents);
			}
			else if (ChannelTypeName == FMovieSceneFloatChannel::StaticStruct()->GetFName())
			{
				UpdateChannelValues<FMovieSceneFloatChannel, FMovieSceneFloatValue>(InKeyHandle, InNextKeyHandle, InChannelData
					, bInAutoFlipTangents, InDisplayRate, InTickResolution, KeySetTangents, ChangingTangents);
			}

			return true;
		});
	
	return FAvaEaseCurveTangents::Average(ChangingTangents);
}

namespace UE::Ava::EaseCurveTool::Private
{
	template <class ChannelHandle, class ChannelValue>
	void SetChannelValues(const FAvaEaseCurveTangents& InTangents
		, const EAvaEaseCurveToolOperation InOperation
		, const FKeyHandle& InKeyHandle, const FKeyHandle& InNextKeyHandle
		, const FAvaEaseCurveKeySelection::FChannelData& InChannelData
		, const bool bInAutoFlipTangents
		, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution)
	{
		TMovieSceneChannelHandle<ChannelHandle> Channel = InChannelData.Channel.Cast<ChannelHandle>();
		TMovieSceneChannelData<ChannelValue> ChannelData = Channel.Get()->GetData();

		TArrayView<ChannelValue> ChannelValues = ChannelData.GetValues();
		const TArrayView<FFrameNumber> ChannelTimes = ChannelData.GetTimes();

		const int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
		const int32 NextKeyIndex = ChannelData.GetIndex(InNextKeyHandle);

		FAvaEaseCurveTangents ScaledTangents = InTangents;

		// If there is a key frame after this key frame that we are editing, we check if the that key frame value is less
		// than or greater than this key frame value. If less, flip the tangent (if option is set).
		const bool bIncreasingValue = ChannelValues[NextKeyIndex].Value >= ChannelValues[KeyIndex].Value;

		if (bInAutoFlipTangents && !bIncreasingValue)
		{
			ScaledTangents.Start *= -1.f;
			ScaledTangents.End *= -1.f;
		}

		// Scale normalized tangents to time/value range
		ScaledTangents.ScaleUp(ChannelTimes[KeyIndex], ChannelValues[KeyIndex].Value
			, ChannelTimes[NextKeyIndex], ChannelValues[NextKeyIndex].Value
			, InDisplayRate, InTickResolution);

		const FScopedTransaction Transaction(NSLOCTEXT("EaseCurveTool", "SetSequencerCurveTangents", "Set Sequencer Curve Tangents"));
		InChannelData.Section->Modify();
		InChannelData.Section->MarkAsChanged();

		// Set this keys leave tangent
		if (InOperation == EAvaEaseCurveToolOperation::Out || InOperation == EAvaEaseCurveToolOperation::InOut)
		{
			ChannelValues[KeyIndex].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			ChannelValues[KeyIndex].Tangent.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
			ChannelValues[KeyIndex].TangentMode = ERichCurveTangentMode::RCTM_Break;
			ChannelValues[KeyIndex].Tangent.LeaveTangent = ScaledTangents.Start;
			ChannelValues[KeyIndex].Tangent.LeaveTangentWeight = ScaledTangents.StartWeight;
		}

		// Set the next keys arrive tangent
		if (NextKeyIndex != INDEX_NONE && (InOperation == EAvaEaseCurveToolOperation::In || InOperation == EAvaEaseCurveToolOperation::InOut))
		{
			ChannelValues[NextKeyIndex].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			ChannelValues[NextKeyIndex].Tangent.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
			ChannelValues[NextKeyIndex].TangentMode = ERichCurveTangentMode::RCTM_Break;
			ChannelValues[NextKeyIndex].Tangent.ArriveTangent = ScaledTangents.End;
			ChannelValues[NextKeyIndex].Tangent.ArriveTangentWeight = ScaledTangents.EndWeight;
		}
	}
}

void FAvaEaseCurveKeySelection::SetTangents(const FAvaEaseCurveTangents& InTangents
	, const EAvaEaseCurveToolOperation InOperation
	, const FFrameRate& InDisplayRate
	, const FFrameRate& InTickResolution
	, const bool bInAutoFlipTangents)
{
	using namespace UE::Ava::EaseCurveTool::Private;
	
	if (TotalSelectedKeys == 0)
	{
		return;
	}
	
	ForEachEaseableKey(/*bInIncludeEqualValueKeys=*/true, [&InDisplayRate, &InTickResolution, &InTangents, InOperation, bInAutoFlipTangents](const FKeyHandle& InKeyHandle
		, const FKeyHandle& InNextKeyHandle, const FAvaEaseCurveKeySelection::FChannelData& InChannelData)
		{
			const FName ChannelTypeName = InChannelData.Channel.GetChannelTypeName();
			if (ChannelTypeName == FMovieSceneDoubleChannel::StaticStruct()->GetFName())
			{
				SetChannelValues<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(InTangents, InOperation, InKeyHandle, InNextKeyHandle, InChannelData
					, bInAutoFlipTangents, InDisplayRate, InTickResolution);
			}
			else if (ChannelTypeName == FMovieSceneFloatChannel::StaticStruct()->GetFName())
			{
				SetChannelValues<FMovieSceneFloatChannel, FMovieSceneFloatValue>(InTangents, InOperation, InKeyHandle, InNextKeyHandle, InChannelData
					, bInAutoFlipTangents, InDisplayRate, InTickResolution);
			}

			return true;
		});
}
