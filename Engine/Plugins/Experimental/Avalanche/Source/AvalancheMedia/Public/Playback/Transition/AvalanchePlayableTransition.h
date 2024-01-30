// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playback/AvalancheRemoteControlValues.h"
#include "UObject/Object.h"

#include "AvalanchePlayableTransition.generated.h"

class UAvalanchePlayable;
class UAvaMediaPlayableGroup;

/**
 * @brief Defines the playable entry role in the transition.
 */
enum class EAvaMediaPlayableTransitionEntryRole : uint8
{
	/** The playable corresponding to this entry is a entering the scene. */
	Enter,
	/** The playable corresponding to this entry is already in the scene and may react to the transition, but is otherwise neutral. */
	Playing,
	/** The playable corresponding to this entry is already in the scene but is commanded to exit. */
	Exit
};

UCLASS()
class AVALANCHEMEDIA_API UAvalanchePlayableTransition : public UObject
{
	GENERATED_BODY()
	
public:
	virtual bool Start();
	virtual void Stop();
	virtual bool IsRunning() const { return false; }
	virtual void Tick(double InDeltaSeconds) {}

	void SetTransitionFlags(EAvalanchePlayableTransitionFlags InFlags);
	void SetEnterPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak);
	void SetPlayingPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak);
	void SetExitPlayables(TArray<TWeakObjectPtr<UAvalanchePlayable>>&& InPlayablesWeak);
	
	bool IsEnterPlayable(UAvalanchePlayable* InPlayable) const;
	bool IsPlayingPlayable(UAvalanchePlayable* InPlayable) const;
	bool IsExitPlayable(UAvalanchePlayable* InPlayable) const;

	void SetEnterPlayableValues(TArray<TSharedPtr<FAvalancheRemoteControlValues>>&& InPlayableValues);
	
	/** This is called during the transition evaluation to indicate discarded playables. */
	void MarkPlayableAsDiscard(UAvalanchePlayable* InPlayable);

	/** Returns information on this transition suitable for logging. */
	virtual FString GetPrettyInfo() const;

	EAvalanchePlayableTransitionFlags GetTransitionFlags() const { return TransitionFlags; }
	
protected:
	UAvalanchePlayable* FindPlayable(const FGuid& InstanceId) const;
	
protected:
	EAvalanchePlayableTransitionFlags TransitionFlags = EAvalanchePlayableTransitionFlags::None;
	
	TArray<TSharedPtr<FAvalancheRemoteControlValues>> EnterPlayableValues;
	
	TArray<TWeakObjectPtr<UAvalanchePlayable>> EnterPlayablesWeak;
	TArray<TWeakObjectPtr<UAvalanchePlayable>> PlayingPlayablesWeak;
	TArray<TWeakObjectPtr<UAvalanchePlayable>> ExitPlayablesWeak;

	/** Keep track of the discarded playables so events can be sent when the transition ends. */
	TArray<TWeakObjectPtr<UAvalanchePlayable>> DiscardPlayablesWeak;

	TSet<TWeakObjectPtr<UAvaMediaPlayableGroup>> PlayableGroupsWeak;
};

class AVALANCHEMEDIA_API FAvaPlayableTransitionBuilder
{
public:
	FAvaPlayableTransitionBuilder();

	void AddEnterPlayableValues(const TSharedPtr<FAvalancheRemoteControlValues>& InValues);
	
	bool AddEnterPlayable(UAvalanchePlayable* InPlayable);
	bool AddPlayingPlayable(UAvalanchePlayable* InPlayable);
	bool AddExitPlayable(UAvalanchePlayable* InPlayable);
	
	bool AddPlayable(UAvalanchePlayable* InPlayable, EAvaMediaPlayableTransitionEntryRole InPlayableRole)
	{
		switch(InPlayableRole)
		{
			case EAvaMediaPlayableTransitionEntryRole::Enter:
				return AddEnterPlayable(InPlayable);
			case EAvaMediaPlayableTransitionEntryRole::Playing:
				return AddPlayingPlayable(InPlayable);
			case EAvaMediaPlayableTransitionEntryRole::Exit:
				return AddExitPlayable(InPlayable);
		}
		return false;
	}

	UAvalanchePlayableTransition* MakeTransition(UObject* InOuter);

private:
	TArray<TSharedPtr<FAvalancheRemoteControlValues>> EnterPlayableValues;

	TArray<TWeakObjectPtr<UAvalanchePlayable>> EnterPlayablesWeak;
	TArray<TWeakObjectPtr<UAvalanchePlayable>> PlayingPlayablesWeak;
	TArray<TWeakObjectPtr<UAvalanchePlayable>> ExitPlayablesWeak;
};