// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvalanchePlayableTransition.h"

#include "Algo/Accumulate.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvaLocalPlayableTransition.h"
#include "Playback/Transition/AvaPlayableTransitionPrivate.h"
#include "Playback/Transition/AvaRemotePlayableTransition.h"

bool UAvalanchePlayableTransition::Start()
{
	using namespace UE::AvaPlayableTransition::Private;

	// Accumulate all the playable groups this transition is part of.
	PlayableGroupsWeak.Reset();
	auto AccumulatePlayableGroups = [this](const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
	{
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (const UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				PlayableGroupsWeak.Add(Playable->GetPlayableGroup());
			}
		}
	};

	AccumulatePlayableGroups(EnterPlayablesWeak);
	AccumulatePlayableGroups(ExitPlayablesWeak);

	for (const TWeakObjectPtr<UAvaMediaPlayableGroup>& PlayableGroupWeak : PlayableGroupsWeak)
	{
		if (UAvaMediaPlayableGroup* PlayableGroup = PlayableGroupWeak.Get())
		{
			PlayableGroup->RegisterPlayableTransition(this);
		}
	}
	return true;
}

void UAvalanchePlayableTransition::Stop()
{
	for (const TWeakObjectPtr<UAvaMediaPlayableGroup>& PlayableGroupWeak : PlayableGroupsWeak)
	{
		if (UAvaMediaPlayableGroup* PlayableGroup = PlayableGroupWeak.Get())
		{
			PlayableGroup->UnregisterPlayableTransition(this);
		}
	}
}

void UAvalanchePlayableTransition::SetTransitionFlags(EAvalanchePlayableTransitionFlags InFlags)
{
	TransitionFlags = InFlags;
}

void UAvalanchePlayableTransition::SetEnterPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak)
{
	EnterPlayablesWeak = MoveTemp(InPlayablesWeak);
}

void UAvalanchePlayableTransition::SetPlayingPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak)
{
	PlayingPlayablesWeak = MoveTemp(InPlayablesWeak);
}

void UAvalanchePlayableTransition::SetExitPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak)
{
	ExitPlayablesWeak = MoveTemp(InPlayablesWeak);
}

bool UAvalanchePlayableTransition::IsEnterPlayable(UAvalanchePlayable* InPlayable) const
{
	return EnterPlayablesWeak.Contains(InPlayable);
}

bool UAvalanchePlayableTransition::IsPlayingPlayable(UAvalanchePlayable* InPlayable) const
{
	return PlayingPlayablesWeak.Contains(InPlayable);
}

bool UAvalanchePlayableTransition::IsExitPlayable(UAvalanchePlayable* InPlayable) const
{
	return ExitPlayablesWeak.Contains(InPlayable);
}

void UAvalanchePlayableTransition::SetEnterPlayableValues(TArray<TSharedPtr<FAvalancheRemoteControlValues>>&& InPlayableValues)
{
	EnterPlayableValues = MoveTemp(InPlayableValues);
}

void UAvalanchePlayableTransition::MarkPlayableAsDiscard(UAvalanchePlayable* InPlayable)
{
	DiscardPlayablesWeak.AddUnique(InPlayable);
	UAvalanchePlayable::OnTransitionEvent().Broadcast(InPlayable, this, EAvalanchePlayableTransitionEventFlags::MarkPlayableDiscard);
}

FString UAvalanchePlayableTransition::GetPrettyInfo() const
{
	auto MakePrettyPlayableList = [](const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak) -> FString
	{
		FString List;
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (const UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				List += List.IsEmpty() ? TEXT("") : TEXT(", ");
				List += Playable->GetUserData();
			}
		}
		return List;
	};

	const FString InList = MakePrettyPlayableList(EnterPlayablesWeak);
	const FString OutList = MakePrettyPlayableList(ExitPlayablesWeak);

	if (InList.IsEmpty())
	{
		return FString::Printf(TEXT("{Out:{%s}}"), *OutList);
	}

	if (OutList.IsEmpty())
	{
		return FString::Printf(TEXT("{In:{%s}}"), *InList);
	}

	return FString::Printf(TEXT("{In:{%s}, Out:{%s}}"), *InList, *OutList);
}

UAvalanchePlayable* UAvalanchePlayableTransition::FindPlayable(const FGuid& InInstanceId) const
{
	auto IsPlayablePredicate = [&InInstanceId](const TWeakObjectPtr<UAvalanchePlayable>& InPlayableWeak) -> bool
	{
		if (const UAvalanchePlayable* Playable = InPlayableWeak.Get())
		{
			return Playable->GetInstanceId() == InInstanceId;		
		}
		return false;
	};
	
	const TWeakObjectPtr<UAvalanchePlayable>* PlayableWeak = EnterPlayablesWeak.FindByPredicate(IsPlayablePredicate);
	if (PlayableWeak && PlayableWeak->IsValid())
	{
		return PlayableWeak->Get();
	}

	PlayableWeak = PlayingPlayablesWeak.FindByPredicate(IsPlayablePredicate);
	if (PlayableWeak && PlayableWeak->IsValid())
	{
		return PlayableWeak->Get();
	}

	PlayableWeak = ExitPlayablesWeak.FindByPredicate(IsPlayablePredicate);
	if (PlayableWeak && PlayableWeak->IsValid())
	{
		return PlayableWeak->Get();
	}
	
	return nullptr;
}

FAvaPlayableTransitionBuilder::FAvaPlayableTransitionBuilder() = default;

void FAvaPlayableTransitionBuilder::AddEnterPlayableValues(const TSharedPtr<FAvalancheRemoteControlValues>& InValues)
{
	EnterPlayableValues.Add(InValues);
}

bool FAvaPlayableTransitionBuilder::AddEnterPlayable(UAvalanchePlayable* InPlayable)
{
	if (ExitPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"enter\" playable because it is already in the \"exit\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}

	if (PlayingPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"enter\" playable because it is already in the \"playing\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}
	
	EnterPlayablesWeak.AddUnique(InPlayable);
	return true;
}

bool FAvaPlayableTransitionBuilder::AddPlayingPlayable(UAvalanchePlayable* InPlayable)
{
	if (EnterPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"exit\" playable because it is already in the \"enter\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}

	if (ExitPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"exit\" playable because it is already in the \"exit\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}

	
	PlayingPlayablesWeak.AddUnique(InPlayable);
	return true;
}

bool FAvaPlayableTransitionBuilder::AddExitPlayable(UAvalanchePlayable* InPlayable)
{
	if (EnterPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"exit\" playable because it is already in the \"enter\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}

	if (PlayingPlayablesWeak.Contains(InPlayable))
	{
		using namespace UE::AvaPlayableTransition::Private;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition setup error: Playable {%s} can't added as an \"enter\" playable because it is already in the \"playing\" list."),
			*GetPrettyPlayableInfo(InPlayable));
		return false;
	}

	ExitPlayablesWeak.AddUnique(InPlayable);
	return true;
}

UAvalanchePlayableTransition* FAvaPlayableTransitionBuilder::MakeTransition(UObject* InOuter)
{
	// Determine if we have remote playables.
	using namespace UE::AvaPlayableTransition::Private;

	// A transition that has only "playing" playables does nothing.
	if (EnterPlayablesWeak.IsEmpty() && ExitPlayablesWeak.IsEmpty())
	{
		return nullptr;
	}

	// A remote transition is created only if all playables are remote.
	const bool bCreateRemoteTransition = AreAllPlayablesRemote(EnterPlayablesWeak)
		&& AreAllPlayablesRemote(ExitPlayablesWeak)
		&& AreAllPlayablesRemote(PlayingPlayablesWeak);
	
	UAvalanchePlayableTransition* PlayableTransition;
	if (bCreateRemoteTransition)
	{
		UAvaRemotePlayableTransition* RemotePlayableTransition = NewObject<UAvaRemotePlayableTransition>(InOuter);
		TArray<FName> ChannelNames;
		ChannelNames.Reserve(1);
		GetChannelNamesFromPlayables(EnterPlayablesWeak, ChannelNames);
		GetChannelNamesFromPlayables(ExitPlayablesWeak, ChannelNames);
		GetChannelNamesFromPlayables(PlayingPlayablesWeak, ChannelNames);
		if (ChannelNames.Num() > 1)
		{
			const FString ChannelNameList = Algo::Accumulate(ChannelNames, FString(), [](FString InResult, const FName& InName)
			{
				InResult = InResult.IsEmpty() ? InName.ToString() : InResult + ", " + InName.ToString();
				return InResult;
			});
		
			UE_LOG(LogAvalanchePlayable, Warning,
				TEXT("Playable Transition setup warning: Playables from different channels (%s) are in the same transition."),
				*ChannelNameList);
		}
		
		if (ChannelNames.Num() > 0)
		{
			RemotePlayableTransition->SetChannelName(ChannelNames[0]);
		}
		else
		{
			UE_LOG(LogAvalanchePlayable, Error,
				TEXT("Playable Transition setup error: Couldn't get a channel name from the list of playables."));
		}
		
		PlayableTransition = RemotePlayableTransition;
	}
	else
	{
		PlayableTransition = NewObject<UAvaLocalPlayableTransition>(InOuter);
	}

	// There must be the same number of entries then there are playables.
	if (ensure(EnterPlayableValues.Num() == EnterPlayablesWeak.Num()))
	{
		PlayableTransition->SetEnterPlayableValues(MoveTemp(EnterPlayableValues));
	}

	PlayableTransition->SetEnterPlayables(MoveTemp(EnterPlayablesWeak));
	PlayableTransition->SetPlayingPlayables(MoveTemp(PlayingPlayablesWeak));
	PlayableTransition->SetExitPlayables(MoveTemp(ExitPlayablesWeak));
	
	return PlayableTransition;
}
