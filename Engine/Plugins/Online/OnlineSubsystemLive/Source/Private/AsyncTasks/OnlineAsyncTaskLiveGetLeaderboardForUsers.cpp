// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveGetLeaderboardForUsers.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineLeaderboardInterfaceLive.h"

#include <collection.h>

FOnlineAsyncTaskLiveGetLeaderboardForUsers::FOnlineAsyncTaskLiveGetLeaderboardForUsers(
	FOnlineSubsystemLive* InLiveSubsystem,
	TArray<FOnlineLeaderboardReadRef> InLeaderboardReads,
	const FOnlineLeaderboardReadRef& InReadObject)
	: FOnlineAsyncTaskBasic(InLiveSubsystem),
	LeaderboardReads(InLeaderboardReads),
	ReadObject(InReadObject)
{
}

void FOnlineAsyncTaskLiveGetLeaderboardForUsers::Tick()
{
	//wait for all LeaderboardReads to be populated with data
	bool bComplete = true;
	for(int i = 0; i < LeaderboardReads.Num(); i++)
	{
		if(LeaderboardReads[i]->ReadState == EOnlineAsyncTaskState::NotStarted || LeaderboardReads[i]->ReadState == EOnlineAsyncTaskState::InProgress)
		{
			bComplete = false;
			break;
		}
	}

	//Once all requests are completed, if all LeaderboardReads were successful then this async task was successful
	if(bComplete)
	{
		bool bSuccessful = true;
		for(int i = 0; i < LeaderboardReads.Num(); i++)
		{
			if(LeaderboardReads[i]->ReadState != EOnlineAsyncTaskState::Done)
			{
				bSuccessful = false;
				break;
			}
		}
		bIsComplete = bComplete;
		bWasSuccessful = bSuccessful;
	}
}

void FOnlineAsyncTaskLiveGetLeaderboardForUsers::Finalize()
{
	FOnlineAsyncTaskBasic::Finalize();
	if (bWasSuccessful && LeaderboardReads.Num() > 0)
	{		
		//For each leaderboard result returned by an async request
		for(int ResultIndex = 0; ResultIndex < LeaderboardReads.Num(); ResultIndex++)
		{
			//take every row in the result and copy it into the single FOnlineLeaderboardRead passed into this request
			for(int ResultRow = 0; ResultRow < LeaderboardReads[ResultIndex]->Rows.Num(); ResultRow++)
			{
				//add the leaderboard row into the single FOnlineLeaderboardRead in sorted order by rank
				int InsertIndex = 0;
				for(; InsertIndex < ReadObject->Rows.Num(); InsertIndex++)
				{
					if(ReadObject->Rows[InsertIndex].Rank >= LeaderboardReads[ResultIndex]->Rows[ResultRow].Rank)
					{
						break;
					}
				}
				ReadObject->Rows.Insert(LeaderboardReads[ResultIndex]->Rows[ResultRow], InsertIndex);
			}
		}
	}

	ReadObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
}

void FOnlineAsyncTaskLiveGetLeaderboardForUsers::TriggerDelegates() 
{
	FOnlineAsyncTaskBasic::TriggerDelegates();
	FOnlineLeaderboardsLivePtr Leaderboards = Subsystem->GetLeaderboardsInterfaceLive();
	Leaderboards->TriggerOnLeaderboardReadCompleteDelegates(bWasSuccessful);
}
