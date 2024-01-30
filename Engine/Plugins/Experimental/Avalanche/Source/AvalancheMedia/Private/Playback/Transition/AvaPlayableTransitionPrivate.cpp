// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaPlayableTransitionPrivate.h"

#include "Playback/AvalanchePlayable.h"
#include "Playback/Playables/AvalancheRemoteProxyPlayable.h"

namespace UE::AvaPlayableTransition::Private
{
	/**
	 * Convert the array of weak objects to an array of objects than can be used (hasn't expired).
	 */
	TArray<UAvalanchePlayable*> Pin(const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
	{
		TArray<UAvalanchePlayable*> Playables;
		Playables.Reserve(InPlayablesWeak.Num());
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				Playables.Add(Playable);
			}
		}
		return Playables;
	}

	TArray<FGuid> GetInstanceIds(const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
	{
		TArray<FGuid> InstanceIds;
		InstanceIds.Reserve(InPlayablesWeak.Num());
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (const UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				InstanceIds.Add(Playable->GetInstanceId());
			}
		}
		return InstanceIds;
	}

	bool AreAllPlayablesRemote(const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
	{
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (const UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				if (!Playable->IsA<UAvalancheRemoteProxyPlayable>())
				{
					return false;
				}
			}
		}
		return true;
	}

	void GetChannelNamesFromPlayables(const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak, TArray<FName>& OutChannelNames)
	{
		for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
		{
			if (const UAvalanchePlayable* Playable = PlayableWeak.Get())
			{
				if (const UAvalancheRemoteProxyPlayable* RemotePlayable = Cast<UAvalancheRemoteProxyPlayable>(Playable))
				{
					OutChannelNames.AddUnique(RemotePlayable->GetPlayingChannelFName());
				}
			}
		}
	}

	FString GetPrettyPlayableInfo(const UAvalanchePlayable* InPlayable)
	{
		if (InPlayable)
		{
			return FString::Printf(TEXT("Id:%s, Asset:%s, Status:%s"),
				*InPlayable->GetInstanceId().ToString(),
				*InPlayable->GetSourceAssetPath().ToString(),
				*StaticEnum<EAvalanchePlayableStatus>()->GetNameByValue(static_cast<int32>(InPlayable->GetPlayableStatus())).ToString());
		}
		return TEXT("(nullptr)");
	}
}
