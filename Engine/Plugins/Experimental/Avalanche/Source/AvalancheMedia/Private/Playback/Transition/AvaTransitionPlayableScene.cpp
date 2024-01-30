// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaTransitionPlayableScene.h"
#include "IAvaSceneInterface.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvalanchePlayableTransition.h"
#include "Transition/Extensions/IAvaTransitionRCExtension.h"

namespace UE::AvaMedia::Private
{
	/** Controllers aren't applied to the Preset, instead, this compares the latest remote control values for a given playable */
	class FAvaRCTransitionPlayableExtension : public IAvaRCTransitionExtension
	{
		virtual EAvaTransitionComparisonResult CompareControllers(const FGuid& InControllerId, const FAvaTransitionScene& InMyScene, const FAvaTransitionScene& InOtherScene) const override
		{
			const UAvalanchePlayable* MyPlayable = InMyScene.GetDataView().GetPtr<UAvalanchePlayable>();
			const UAvalanchePlayable* OtherPlayable = InOtherScene.GetDataView().GetPtr<UAvalanchePlayable>();
			if (!MyPlayable || !OtherPlayable)
			{
				return EAvaTransitionComparisonResult::None;
			}

			const FAvalancheRemoteControlValue* MyValue = MyPlayable->GetLatestRemoteControlValues().ControllerValues.Find(InControllerId);
			const FAvalancheRemoteControlValue* OtherValue = OtherPlayable->GetLatestRemoteControlValues().ControllerValues.Find(InControllerId);
			if (!MyValue || !OtherValue)
			{
				return EAvaTransitionComparisonResult::None;
			}

			return MyValue->IsSameValueAs(*OtherValue)
				? EAvaTransitionComparisonResult::Same
				: EAvaTransitionComparisonResult::Different;
		}
	};
}

FAvaTransitionPlayableScene::FAvaTransitionPlayableScene(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InPlayableTransition)
	: FAvaTransitionScene(InPlayable)
	, PlayableTransitionWeak(InPlayableTransition)
{
	AddExtension<UE::AvaMedia::Private::FAvaRCTransitionPlayableExtension>();
}

FAvaTransitionPlayableScene::FAvaTransitionPlayableScene(const FAvaTagHandle& InTransitionLayer, UAvalanchePlayableTransition* InPlayableTransition)
	: FAvaTransitionPlayableScene(nullptr, InPlayableTransition)
{
	OverrideTransitionLayer = InTransitionLayer;
}

EAvaTransitionComparisonResult FAvaTransitionPlayableScene::Compare(const FAvaTransitionScene& InOther) const
{
	const UAvalanchePlayable* MyPlayable    = GetDataView().GetPtr<UAvalanchePlayable>();
	const UAvalanchePlayable* OtherPlayable = InOther.GetDataView().GetPtr<UAvalanchePlayable>();

	if (!MyPlayable || !OtherPlayable)
	{
		return EAvaTransitionComparisonResult::None;
	}

	// Determine if Template is the same via the Package Name To Load (i.e. Source Level)
	if (MyPlayable->GetSourceAssetPath() == OtherPlayable->GetSourceAssetPath())
	{
		return EAvaTransitionComparisonResult::Same;
	}

	return EAvaTransitionComparisonResult::Different;
}

ULevel* FAvaTransitionPlayableScene::GetLevel() const
{
	const UAvalanchePlayable* Playable = GetDataView().GetPtr<UAvalanchePlayable>();
	const IAvaSceneInterface* SceneInterface = Playable ? Playable->GetSceneInterface() : nullptr;
	return SceneInterface ? SceneInterface->GetSceneLevel() : nullptr;
}

void FAvaTransitionPlayableScene::GetOverrideTransitionLayer(FAvaTagHandle& OutTransitionLayer) const
{
	if (OverrideTransitionLayer.IsSet())
	{
		OutTransitionLayer = *OverrideTransitionLayer;
	}
}

void FAvaTransitionPlayableScene::OnFlagsChanged()
{
	UAvalanchePlayable* Playable = GetDataView().GetMutablePtr<UAvalanchePlayable>();
	UAvalanchePlayableTransition* PlayableTransition = PlayableTransitionWeak.Get();
	if (!Playable || !PlayableTransition)
	{
		return;
	}

	// Event received when the playable can be discarded/recycled.
	if (HasAnyFlags(EAvaTransitionSceneFlags::NeedsDiscard))
	{
		// Do some error checking.
		if (PlayableTransition->IsEnterPlayable(Playable))
		{
			UE_LOG(LogAvalanchePlayable, Error, TEXT("Playable Transition \"%s\" Error: An \"enter\" playable is being discarded."), *PlayableTransition->GetFullName());
		}
		PlayableTransition->MarkPlayableAsDiscard(Playable);
	}
}
