// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalanchePageTransitionBuilder.h"

#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Transition/AvalanchePageTransition.h"

FAvalanchePageTransitionBuilder::~FAvalanchePageTransitionBuilder()
{
	if (Playlist)
	{
		for (UAvalanchePageTransition* PageTransition : PageTransitions)
		{
			// Add any remaining playing pages in the channel that are not either exit or enter page already.
			for (UAvalanchePagePlayer* PagePlayer : Playlist->GetPagePlayers())
			{
				if (PagePlayer->ChannelFName == PageTransition->GetChannelName())
				{
					if (!PageTransition->HasPagePlayer(PagePlayer))
					{
						PageTransition->AddPlayingPage(PagePlayer);
					}
				}
			}
			
			Playlist->AddPageTransition(PageTransition);

			bool bShouldDiscard = false;
			if (PageTransition->CanStart(bShouldDiscard))
			{
				PageTransition->Start();
			}
			else
			{
				// Some playables are still loading, push the command for later execution.
				Playlist->GetPlaybackManager().PushPlaybackTransitionStartCommand(PageTransition);
			}
		}
	}
}

UAvalanchePageTransition* FAvalanchePageTransitionBuilder::FindTransition(const UAvalanchePagePlayer* InPlayer) const
{
	// Currently, the only batching criteria is the channel.
	for (UAvalanchePageTransition* PageTransition : PageTransitions)
	{
		if (PageTransition->GetChannelName() == InPlayer->ChannelFName)
		{
			return PageTransition;
		}
	}
	return nullptr;	
}

UAvalanchePageTransition* FAvalanchePageTransitionBuilder::FindOrAddTransition(const UAvalanchePagePlayer* InPlayer)
{
	if (UAvalanchePageTransition* PageTransition = FindTransition(InPlayer))
	{
		return PageTransition;
	}
		
	UAvalanchePageTransition* PageTransition = NewObject<UAvalanchePageTransition>(Playlist);
	if (PageTransition)
	{
		PageTransitions.Add(PageTransition);
	}
	return PageTransition;	
}