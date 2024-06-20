// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/AvaPlayableLibrary.h"

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Playable/AvaPlayableGroup.h"
#include "Playable/Playables/AvaPlayableLevelStreaming.h"
#include "Playable/Transition/AvaPlayableTransition.h"

namespace UE::AvaPlayableLibrary::Private
{
	ULevel* GetLevel(const UObject* InWorldContextObject)
	{
		if (!InWorldContextObject)
		{
			return nullptr;
		}

		ULevel* Level = InWorldContextObject->GetTypedOuter<ULevel>();
		if (!Level && GEngine)
		{
			if (const UWorld* World = GEngine->GetWorldFromContextObject(InWorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
			{
				Level = World->PersistentLevel;
			}
		}
		return Level;
	}
}


UAvaPlayable* UAvaPlayableLibrary::GetPlayable(const UObject* InWorldContextObject)
{
	using namespace UE::AvaPlayableLibrary::Private;
	const ULevel* Level = GetLevel(InWorldContextObject);
	if (!Level)
	{
		return nullptr;
	}

	UAvaPlayableGroup* PlayableGroup = UAvaPlayableGroup::FindPlayableGroupForWorld(Level->OwningWorld);
	if (!PlayableGroup)
	{
		return nullptr;
	}

	UAvaPlayable* FoundPlayable = nullptr;
	PlayableGroup->ForEachPlayable([&FoundPlayable, Level](UAvaPlayable* InPlayable)
	{
		if (const UAvaPlayableLevelStreaming* PlayableLevelStreaming = Cast<UAvaPlayableLevelStreaming>(InPlayable))
		{
			if (const ULevelStreaming* LevelStreaming = PlayableLevelStreaming->GetLevelStreaming())
			{
				if (LevelStreaming->GetLoadedLevel() == Level)
				{
					FoundPlayable = InPlayable;
					return false;
				}
			}
		}
		return true;
	});

	return FoundPlayable;
}

UAvaPlayableTransition* UAvaPlayableLibrary::GetPlayableTransition(const UAvaPlayable* InPlayable)
{
	if (!InPlayable)
	{
		return nullptr;
	}

	UAvaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup();
	if (!PlayableGroup)
	{
		return nullptr;
	}

	UAvaPlayableTransition* FoundTransition = nullptr;
	PlayableGroup->ForEachPlayableTransition([&FoundTransition, InPlayable](UAvaPlayableTransition* InTransition)
	{
		if (InTransition->IsEnterPlayable(InPlayable)
			|| InTransition->IsPlayingPlayable(InPlayable)
			|| InTransition->IsExitPlayable(InPlayable))
		{
			FoundTransition = InTransition;
			return false;
		}		
		return true;
	});

	return FoundTransition;
}

void UAvaPlayableLibrary::UpdateRemoteControlValues(const UObject* InWorldContextObject)
{
	if (UAvaPlayable* Playable = GetPlayable(InWorldContextObject))
	{
		if (UAvaPlayableTransition* Transition = GetPlayableTransition(Playable))
		{
			if (const TSharedPtr<FAvaPlayableRemoteControlValues> RemoteControlValues = Transition->GetValuesForPlayable(Playable))
			{
				Playable->UpdateRemoteControlCommand(RemoteControlValues.ToSharedRef());
			}
		}
	}
}