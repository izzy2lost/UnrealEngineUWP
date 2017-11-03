// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineLeaderboardInterfaceLive.h"
#include "OnlineEventsInterfaceLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveGetLeaderboardForUsers.h"

const TCHAR* FOnlineLeaderboardsLive::SortOrder = L"descending";

FOnlineLeaderboardsLive::FOnlineLeaderboardsLive( class FOnlineSubsystemLive* InSubsystem ) : LiveSubsystem( InSubsystem )
{
}

bool FOnlineLeaderboardsLive::ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	if ( !LiveSubsystem )
	{
		UE_LOG_ONLINE(Warning, TEXT( "ReadLeaderboards failed. LiveSubsystem is null."));
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	FOnlineAsyncTaskManager* AsyncTaskManager = LiveSubsystem->GetAsyncTaskManager();
	if(!AsyncTaskManager)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to get AsyncTaskManager from Online Subsystem"));
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	//Currently Xbox One only supports single column leaderboards. If more than one column is requested, fail the request for now.
	if(ReadObject->ColumnMetadata.Num() > 1)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failing Leaderboards request for multiple columns. Xbox One currently only supports single-column leaderboards."));
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}
		
	//Attempt to retrieve an XboxLiveContext from the signed in users for making leaderboard requests.
	//This is necessary because the user actively requesting this data is not provided explicitly and not guaranteed to be in the Players list.
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = nullptr;
	
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ Users = Identity->GetCachedUsers();
	for(unsigned int i = 0; i < Users->Size; i++)
	{
		Windows::Xbox::System::User^ User = Users->GetAt(i);
		if(!User->IsSignedIn)
		{
			continue;
		}

		LiveContext  = LiveSubsystem->GetLiveContext(User);
		if(LiveContext != nullptr)
		{
			break;
		}
	}
	
	if(LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to obtain an XboxLiveContext to retrieve leaderboards data."));
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

	// Clear out any existing data
	ReadObject->Rows.Empty();

	TArray<FOnlineLeaderboardReadRef> LeaderboardReads;

	try
	{
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			FOnlineLeaderboardReadRef LeaderboardRead = FOnlineLeaderboardReadRef(new FOnlineLeaderboardRead());
			LeaderboardRead->LeaderboardName = ReadObject->LeaderboardName;
			for (int32 j = 0; j < ReadObject->ColumnMetadata.Num(); ++j)
			{
				LeaderboardRead->ColumnMetadata.Add(ReadObject->ColumnMetadata[j]);
			}
			LeaderboardReads.Add(LeaderboardRead);

			Platform::String^ LeaderboardName = ref new Platform::String(*ReadObject->LeaderboardName.GetPlainNameString());
			FUniqueNetIdLive LiveId(*Players[i]);
			Platform::String^ PlayerName = ref new Platform::String(*LiveId.ToString());

			Windows::Foundation::IAsyncOperation<LeaderboardResult^>^ pAsyncOp = LiveContext->LeaderboardService->GetLeaderboardWithSkipToUserAsync(
																	// @ATG_CHANGE : BEGIN - UWP LIVE support
																	LiveContext->AppConfig->ServiceConfigurationId,
																	// @ATG_CHANGE : END
																	LeaderboardName,
																	PlayerName,
																	1
																	);
			if(!pAsyncOp)
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to create async leaderboard request operation"));
				ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
				TriggerOnLeaderboardReadCompleteDelegates(false);
				return false;
			}

			LeaderboardRead->ReadState = EOnlineAsyncTaskState::InProgress;

			concurrency::create_task( pAsyncOp ).then( [=, this] (concurrency::task<LeaderboardResult^> Results)
			{
				try
				{
					LeaderboardResult^ Leaderboard = Results.get();

					FOnlineAsyncTaskLiveGetLeaderboard* AsyncTask = new FOnlineAsyncTaskLiveGetLeaderboard(
													LiveSubsystem,			//The LiveOnlineSubsystem used to retrieve the leaderboards interface
													Leaderboard,			//The leaderboard data returned from Live
													LeaderboardRead,    	//Storage for the returned leaderboard data
													false					//Do not fire the OnLeaderboardReadComplete delegate when the async task completes
													);
					AsyncTaskManager->AddToOutQueue(AsyncTask);
				}
				catch (Platform::Exception^ Ex)
				{
					UE_LOG_ONLINE(Warning, TEXT("Leaderboard serivce request failed. Exception: %s."), Ex->ToString()->Data());
					LeaderboardRead->ReadState = EOnlineAsyncTaskState::Failed;
				}
			});
		}

		FOnlineAsyncTaskLiveGetLeaderboardForUsers* AsyncTask = new FOnlineAsyncTaskLiveGetLeaderboardForUsers(LiveSubsystem, LeaderboardReads, ReadObject);
		AsyncTaskManager->AddToInQueue(AsyncTask);
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("GetLeaderboard failed. Exception: %s."), Ex->ToString()->Data());
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}
	return true;
}

bool FOnlineLeaderboardsLive::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	if ( !LiveSubsystem )
	{
		UE_LOG_ONLINE(Warning, TEXT( "ReadLeaderboardsForFriends failed. LiveSubsystem is null."));
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	FOnlineAsyncTaskManager* AsyncTaskManager = LiveSubsystem->GetAsyncTaskManager();
	if(!AsyncTaskManager)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to get AsyncTaskManager from Online Subsystem"));
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	//Currently Xbox One only supports single column leaderboards. If more than one column is requested, fail the request for now.
	if(ReadObject->ColumnMetadata.Num() > 1)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failing Leaderboards request for multiple columns. Xbox One currently only supports single-column leaderboards."));
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if(LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to obtain an XboxLiveContext to retrieve leaderboards data."));
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	try
	{
		Platform::String^ StatName;
		if (ReadObject->ColumnMetadata.Num() > 0)
		{
			StatName = ref new Platform::String(*ReadObject->ColumnMetadata[0].ColumnName.GetPlainNameString());
		}

		if (StatName->Length() == 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failing Leaderboards request. No statistic requested for friends leaderboard."));
			ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
			TriggerOnLeaderboardReadCompleteDelegates(false);
			return false;
		}

		ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

		// Clear out any existing data
		ReadObject->Rows.Empty();

		Windows::Foundation::IAsyncOperation<LeaderboardResult^>^ pAsyncOp = LiveContext->LeaderboardService->GetLeaderboardForSocialGroupAsync(
											LiveContext->User->XboxUserId,
											// @ATG_CHANGE : BEGIN UWP LIVE support
											LiveContext->AppConfig->ServiceConfigurationId,
											// @ATG_CHANGE : END
											StatName,
											Microsoft::Xbox::Services::Social::SocialGroupConstants::People,
											ref new Platform::String(SortOrder),
											0);

		if(!pAsyncOp)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to create async leaderboard request operation"));
			ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
			TriggerOnLeaderboardReadCompleteDelegates(false);
			return false;
		}
		
		concurrency::create_task( pAsyncOp ).then( [=, this] (concurrency::task<LeaderboardResult^> Results)
		{
			try
			{
				LeaderboardResult^ leaderboard = Results.get();

				FOnlineAsyncTaskLiveGetLeaderboard* AsyncTask = new FOnlineAsyncTaskLiveGetLeaderboard(
												LiveSubsystem,			//The LiveOnlineSubsystem that the AsyncTask will use to retrieve data for Leaderboards call
												leaderboard,			//The leaderboard data returned from Live
												ReadObject,				//Storage for the returned leaderboard data
												true					//Fire the OnLeaderboardReadComplete delegate when the async task completes
												);

				AsyncTaskManager->AddToOutQueue(AsyncTask);
			}
			catch (Platform::Exception^ ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Leaderboard serivce request failed. Exception: %s."), ex->ToString()->Data());
				ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
				AsyncTaskManager->AddToOutQueue(new FOnlineAsyncTaskLiveGetLeaderboardFailed(LiveSubsystem));
			}
		});
	}
	catch(Platform::Exception^ ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("GetLeaderboard failed. Exception: %s."), ex->ToString()->Data());
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return false;
	}

	return true;
}

bool FOnlineLeaderboardsLive::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsLive::ReadLeaderboardsAroundRank is currently not supported."));
	return false;
}
bool FOnlineLeaderboardsLive::ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsLive::ReadLeaderboardsAroundUser is currently not supported."));
	return false;
}

void FOnlineLeaderboardsLive::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	UNREFERENCED_PARAMETER(ReadObject);
	// NOOP
}

bool FOnlineLeaderboardsLive::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	UNREFERENCED_PARAMETER(SessionName);
	UNREFERENCED_PARAMETER(Player);
	UNREFERENCED_PARAMETER(WriteObject);
	return false;
}

bool FOnlineLeaderboardsLive::FlushLeaderboards(const FName& SessionName)
{
	UNREFERENCED_PARAMETER(SessionName);
	return false;
}

bool FOnlineLeaderboardsLive::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	UNREFERENCED_PARAMETER(SessionName);
	UNREFERENCED_PARAMETER(LeaderboardId);
	UNREFERENCED_PARAMETER(PlayerScores);
	return false;
}
