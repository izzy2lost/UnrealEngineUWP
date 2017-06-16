// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#if !USE_SOCIAL_MANAGER	
// @ATG_CHANGE :  END

#include "OnlineAsyncTaskLiveQueryFriends.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineFriendsInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlinePresenceInterfaceLive.h"
#include "../OnlineSessionInterfaceLive.h"

#include <collection.h>

using Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails;
using Microsoft::Xbox::Services::Presence::PresenceRecord;
using Microsoft::Xbox::Services::Social::XboxSocialRelationship;
using Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult;
using Microsoft::Xbox::Services::Social::XboxUserProfile;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::IAsyncOperation;

FOnlineAsyncTaskLiveQueryFriends::FOnlineAsyncTaskLiveQueryFriends(FOnlineSubsystemLive* const InLiveInterface,
																   XboxLiveContext^ InLiveContext,
																   const int32 InLocalUserNum,
																   const FString& InListName,
																   const FOnReadFriendsListComplete& InCompletionDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, LocalUserNum(InLocalUserNum)
	, ListName(InListName)
	, Delegate(InCompletionDelegate)
	, FoundItemCount(0)
{
}

IAsyncOperation<XboxSocialRelationshipResult^>^ FOnlineAsyncTaskLiveQueryFriends::CreateOperation()
{
	try
	{
		return LiveContext->SocialService->GetSocialRelationshipsAsync();
	}
	catch (Platform::Exception^ Ex)
	{
		OutError = FString::Printf(TEXT("Error querying friends, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
		UE_LOG_ONLINE(Error, *OutError);
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryFriends::ProcessResult(const Concurrency::task<XboxSocialRelationshipResult^>& CompletedTask)
{
	XboxSocialRelationshipResult^ Result = nullptr;
	try
	{
		Result = CompletedTask.get();
	}
	catch (Platform::Exception^ Ex)
	{
		OutError = FString::Printf(TEXT("Error querying friends, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
		UE_LOG_ONLINE(Error, *OutError);
		return false;
	}

	// Build base friends list
	IVectorView<XboxSocialRelationship^>^ XboxFriendItemsVector = Result->Items;

	FoundItemCount = XboxFriendItemsVector->Size;
	FriendsListMap.Reserve(FoundItemCount);

	for (int32 Index = 0; Index < FoundItemCount; ++Index)
	{
		XboxSocialRelationship^ FriendItem = XboxFriendItemsVector->GetAt(Index);
		TSharedRef<FOnlineFriendLive> LiveFriend = MakeShared<FOnlineFriendLive>(FriendItem);
		FriendsListMap.Emplace(*StaticCastSharedRef<const FUniqueNetIdLive>(LiveFriend->GetUserId()), LiveFriend);
	}

	// Sort favourites to the front of the list
	FriendsListMap.ValueSort([](const TSharedRef<FOnlineFriendLive>& FriendA, const TSharedRef<FOnlineFriendLive>& FriendB)
	{
		return static_cast<int32>(FriendA->IsFavorite()) < static_cast<int32>(FriendB->IsFavorite());
	});

	return true;
}

void FOnlineAsyncTaskLiveQueryFriends::Finalize()
{
	if (!bWasSuccessful)
	{
		return;
	}

	if (FoundItemCount > 0)
	{
		if (FOnlineAsyncTaskManagerLive* const MyTaskManager = Subsystem->GetAsyncTaskManager())
		{
			const FOnlinePresenceLivePtr PresencePtr = Subsystem->GetPresenceLive();

			// Build list of XUIDs to send to Live for more information
			Platform::Collections::Vector<Platform::String^>^ XUIDVector = ref new Platform::Collections::Vector<Platform::String^>();
			for (const FOnlineFriendsListLiveMap::ElementType& FriendPair : FriendsListMap)
			{
				Platform::String^ UserToQueryXuid = ref new Platform::String(*FriendPair.Key.UniqueNetIdStr);
				XUIDVector->Append(UserToQueryXuid);

				if (PresencePtr.IsValid())
				{
					if (!PresencePtr->IsSubscribedToPresenceUpdates(FriendPair.Key))
					{
						PresencePtr->AddPresenceUpdateSubscription(FriendPair.Key);
						try
						{
							// Subscribe to updates
							LiveContext->PresenceService->SubscribeToTitlePresenceChange(UserToQueryXuid, Subsystem->TitleId);
							LiveContext->PresenceService->SubscribeToDevicePresenceChange(UserToQueryXuid);
						}
						catch (Platform::Exception^ Ex)
						{
							UE_LOG_ONLINE(Warning, TEXT("Failed to subscribe to presence updates for user %s"), *FriendPair.Key.UniqueNetIdStr);
						}
					}
				}
			}

			IVectorView<Platform::String^>^ XUIDVectorView = XUIDVector->GetView();

			// Task to manage calling delegates and updating the local cache after the below tasks are finished
			FOnlineAsyncTaskLiveQueryFriendManagerTask* ManagerTask = new FOnlineAsyncTaskLiveQueryFriendManagerTask(Subsystem, MoveTemp(FriendsListMap), LocalUserNum, MoveTemp(ListName), MoveTemp(Delegate));
			{
				// Request Account Information (DisplayName / Potentially Icon information in the future)
				Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryFriendAccountDetails>(Subsystem, LiveContext, XUIDVectorView, *ManagerTask);

				// Request Presence Information
				Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryFriendPresenceDetails>(Subsystem, LiveContext, XUIDVectorView, *ManagerTask);
			}
			// Add Manager Last so it gets ticked after the tasks
			MyTaskManager->AddToParallelTasks(ManagerTask);
		}
		else
		{
			bWasSuccessful = false;
			OutError = FString(TEXT("FOnlineAsyncTaskLiveQueryFriends Could not load AsyncTaskManager"));
		}
	}
	else
	{
		TSharedPtr<const FUniqueNetIdLive> LiveUserId = StaticCastSharedPtr<const FUniqueNetIdLive>(Subsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
		if (!LiveUserId.IsValid())
		{
			OutError = FString::Printf(TEXT("Could not find localuser at index %d"), LocalUserNum);
			bWasSuccessful = false;
			return;
		}

		// We didn't find anything, so we don't need to query any additional info.  Just clear the map for this user
		Subsystem->GetFriendsLive()->FriendsMap.FindOrAdd(*LiveUserId).Empty();
	}
}

void FOnlineAsyncTaskLiveQueryFriends::TriggerDelegates()
{
	// If we successfully found nothing, we need to move our empty friendslist over now since we're done
	if (bWasSuccessful && FoundItemCount == 0)
	{
		FOnlineFriendsLivePtr FriendsInt = Subsystem->GetFriendsLive();
		if (FriendsInt.IsValid())
		{
			// Call friendslist changed delegate
			FriendsInt->TriggerOnFriendsChangeDelegates(LocalUserNum);
		}
	}

	if (!bWasSuccessful || FoundItemCount == 0)
	{
		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, OutError);
	}
}

void FOnlineAsyncTaskLiveQueryFriendManagerTask::Tick()
{
	if ((AccountDetailsStatus == EOnlineAsyncTaskState::Done || AccountDetailsStatus == EOnlineAsyncTaskState::Failed)
		&& (PresenceDetailsStatus == EOnlineAsyncTaskState::Done || PresenceDetailsStatus == EOnlineAsyncTaskState::Failed)
		&& (SessionDetailsStatus == EOnlineAsyncTaskState::Done || SessionDetailsStatus == EOnlineAsyncTaskState::Failed))
	{
		bIsComplete = true;
		bWasSuccessful = (AccountDetailsStatus == EOnlineAsyncTaskState::Done &&
			PresenceDetailsStatus == EOnlineAsyncTaskState::Done &&
			SessionDetailsStatus == EOnlineAsyncTaskState::Done);
	}
}

void FOnlineAsyncTaskLiveQueryFriendManagerTask::Finalize()
{
	if (bWasSuccessful)
	{
		TSharedPtr<const FUniqueNetIdLive> UserId = StaticCastSharedPtr<const FUniqueNetIdLive>(Subsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
		if (UserId.IsValid())
		{
			FOnlinePresenceLivePtr PresenceInt = Subsystem->GetPresenceLive();
			if (PresenceInt.IsValid())
			{
				// Copy our friends' presence into our presence cache
				for (const FOnlineFriendsListLiveMap::ElementType& FriendPair : FriendsListMap)
				{
					const FOnlineUserPresenceLive& PresenceToCopy = static_cast<const FOnlineUserPresenceLive&>(FriendPair.Value->GetPresence());
					PresenceInt->PresenceCache.Add(FriendPair.Key, MakeShared<FOnlineUserPresenceLive>(PresenceToCopy));
				}
			}

			FOnlineFriendsLivePtr FriendsInt = Subsystem->GetFriendsLive();
			if (FriendsInt.IsValid())
			{
				FriendsInt->FriendsMap.Emplace(*UserId, MoveTemp(FriendsListMap));
			}
			// FriendsListMap is now empty here
		}
		else
		{
			bWasSuccessful = false;
		}
	}
}

void FOnlineAsyncTaskLiveQueryFriendManagerTask::TriggerDelegates()
{
	FString ErrorString;

	if (!bWasSuccessful)
	{
		if (AccountDetailsStatus == EOnlineAsyncTaskState::Failed
			|| PresenceDetailsStatus == EOnlineAsyncTaskState::Failed
			|| SessionDetailsStatus == EOnlineAsyncTaskState::Failed)
		{
			if (AccountDetailsStatus == EOnlineAsyncTaskState::Failed)
			{
				ErrorString += TEXT("Failed to query account details. ");
			}
			if (PresenceDetailsStatus == EOnlineAsyncTaskState::Failed)
			{
				ErrorString += TEXT("Failed to query presence details. ");
			}
			if (SessionDetailsStatus == EOnlineAsyncTaskState::Failed)
			{
				ErrorString += TEXT("Failed to query session details. ");
			}
		}
		else
		{
			ErrorString = TEXT("An unknown error has occured");
		}
	}

	// Call delegates
	Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorString);
	if (bWasSuccessful)
	{
		Subsystem->GetFriendsLive()->TriggerOnFriendsChangeDelegates(LocalUserNum);
	}
}

FOnlineAsyncTaskLiveQueryFriendAccountDetails::FOnlineAsyncTaskLiveQueryFriendAccountDetails(FOnlineSubsystemLive* const InLiveInterface,
																							 XboxLiveContext^ InLiveContext,
																							 IVectorView<Platform::String^>^ InXUIDsVectorView,
																							 FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, XUIDsVectorView(InXUIDsVectorView)
	, ManagerTask(InManagerTask)
{
	check(XUIDsVectorView);
}

IAsyncOperation<IVectorView<XboxUserProfile^>^>^ FOnlineAsyncTaskLiveQueryFriendAccountDetails::CreateOperation()
{
	try
	{
		IAsyncOperation<IVectorView<XboxUserProfile^>^>^ AsyncTask = LiveContext->ProfileService->GetUserProfilesAsync(XUIDsVectorView);
		return AsyncTask;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend account details for friends list, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryFriendAccountDetails::ProcessResult(const Concurrency::task<IVectorView<XboxUserProfile^>^>& CompletedTask)
{
	try
	{
		IVectorView<XboxUserProfile^>^ UserProfilesVector = CompletedTask.get();
		int32 FoundProfileCount = UserProfilesVector->Size;
		for (int32 Index = 0; Index < FoundProfileCount; ++Index)
		{
			XboxUserProfile^ Profile = UserProfilesVector->GetAt(Index);
			check(Profile);

			TSharedRef<FOnlineFriendLive>& FoundFriend = ManagerTask.FriendsListMap.FindChecked(FUniqueNetIdLive(Profile->XboxUserId));

			FoundFriend->DisplayName = FOnlineUserInfoLive::FilterPlayerName(Profile->GameDisplayName);
			FoundFriend->UserAttributes.Emplace(FString(TEXT("Gamerscore")), FString(Profile->Gamerscore->Data()));
			// This is a the URI to a resizeable display image for the user.  For example, &format=png&w=64&h=64
			// Valid Format: png
			// Valid Width/Height: 64/64, 208/208, or 424/424
			FoundFriend->UserAttributes.Emplace(FString(TEXT("DisplayPictureUri")), FString(Profile->GameDisplayPictureResizeUri->AbsoluteCanonicalUri->Data()));
		}

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend account details, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return false;
}

void FOnlineAsyncTaskLiveQueryFriendAccountDetails::Finalize()
{
	ManagerTask.AccountDetailsStatus = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
}

FOnlineAsyncTaskLiveQueryFriendPresenceDetails::FOnlineAsyncTaskLiveQueryFriendPresenceDetails(FOnlineSubsystemLive* const InLiveInterface,
																							   XboxLiveContext^ InLiveContext,
																							   IVectorView<Platform::String^>^ InXUIDsVectorView,
																							   FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, XUIDsVectorView(InXUIDsVectorView)
	, ManagerTask(InManagerTask)
{
	check(XUIDsVectorView);
}

IAsyncOperation<IVectorView<PresenceRecord^>^>^ FOnlineAsyncTaskLiveQueryFriendPresenceDetails::CreateOperation()
{
	try
	{
		IAsyncOperation<IVectorView<PresenceRecord^>^>^ AsyncTask = LiveContext->PresenceService->GetPresenceForMultipleUsersAsync(XUIDsVectorView);
		return AsyncTask;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend presence details for friends list, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryFriendPresenceDetails::ProcessResult(const Concurrency::task<IVectorView<PresenceRecord^>^>& CompletedTask)
{
	try
	{
		IVectorView<PresenceRecord^>^ UserPresenceVector = CompletedTask.get();
		const int32 FoundPresenceCount = UserPresenceVector->Size;
		for (int32 Index = 0; Index < FoundPresenceCount; ++Index)
		{
			PresenceRecord^ XboxPresence = UserPresenceVector->GetAt(Index);
			check(XboxPresence);

			TSharedRef<FOnlineFriendLive>& FoundFriend = ManagerTask.FriendsListMap.FindChecked(FUniqueNetIdLive(XboxPresence->XboxUserId));

			FoundFriend->Presence = FOnlineUserPresenceLive(XboxPresence);
		}

		// Register for stat updates
		FOnlineSessionLivePtr SessionInt(Subsystem->GetSessionInterfaceLive());
		if (SessionInt.IsValid() && !SessionInt->SessionUpdateStatName.IsEmpty() && XUIDsVectorView->Size > 0)
		{
			Platform::String^ SessionUpdatedStatName = ref new Platform::String(*SessionInt->SessionUpdateStatName);
			for (Platform::String^ FriendXUID : XUIDsVectorView)
			{
				FUniqueNetIdLive FriendNetId(FriendXUID);
				if (!SessionInt->IsSubscribedToSessionStatUpdates(FriendNetId))
				{
					SessionInt->AddSessionUpdateStatSubscription(FriendNetId);
					// @ATG_CHANGE : BEGIN - UWP support
					LiveContext->UserStatisticsService->SubscribeToStatisticChange(FriendXUID, Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->ServiceConfigurationId, SessionUpdatedStatName);
					// @ATG_CHANGE : END - UWP support
				}
			}
		}

		// Now that our presence value has been set for everyone, we want to update the session value for all users we requested presence for
		Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryFriendSessionDetails>(Subsystem, LiveContext, XUIDsVectorView, ManagerTask);

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend presence details, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return false;
}

void FOnlineAsyncTaskLiveQueryFriendPresenceDetails::Finalize()
{
	if (bWasSuccessful)
	{
		ManagerTask.PresenceDetailsStatus = EOnlineAsyncTaskState::Done;
	}
	else
	{
		ManagerTask.PresenceDetailsStatus = EOnlineAsyncTaskState::Failed;
		ManagerTask.SessionDetailsStatus = EOnlineAsyncTaskState::Failed;
	}
}

FOnlineAsyncTaskLiveQueryFriendSessionDetails::FOnlineAsyncTaskLiveQueryFriendSessionDetails(FOnlineSubsystemLive* const InLiveInterface,
																							 XboxLiveContext^ InLiveContext,
																							 IVectorView<Platform::String^>^ InXUIDsVectorView,
																							 FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, XUIDsVectorView(InXUIDsVectorView)
	, ManagerTask(InManagerTask)
{
	check(XUIDsVectorView);
}

IAsyncOperation<IVectorView<MultiplayerActivityDetails^>^>^ FOnlineAsyncTaskLiveQueryFriendSessionDetails::CreateOperation()
{
	try
	{
		// TODO: subscribe to session changes

		// @ATG_CHANGE : BEGIN - UWP support
		return LiveContext->MultiplayerService->GetActivitiesForUsersAsync(Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->ServiceConfigurationId, XUIDsVectorView);
		// @ATG_CHANGE : END - UWP support
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend session details for friends list, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryFriendSessionDetails::ProcessResult(const Concurrency::task<IVectorView<MultiplayerActivityDetails^>^>& CompletedTask)
{
	try
	{
		IVectorView<MultiplayerActivityDetails^>^ UserActivityVector = CompletedTask.get();
		const int32 FoundActivityCount = UserActivityVector->Size;
		for (int32 Index = 0; Index < FoundActivityCount; ++Index)
		{
			MultiplayerActivityDetails^ UserActivity = UserActivityVector->GetAt(Index);
			check(UserActivity);

			TSharedRef<FOnlineFriendLive>& FoundFriend = ManagerTask.FriendsListMap.FindChecked(FUniqueNetIdLive(UserActivity->OwnerXboxUserId));

			const bool bHasEmptySlot = UserActivity->MembersCount < UserActivity->MaxMembersCount;
			const bool bSessionOpen = !UserActivity->Closed;

			const bool bIsJoinable = bHasEmptySlot && bSessionOpen;

			FoundFriend->Presence.SessionId = MakeShared<FUniqueNetIdString>(UserActivity->SessionReference->ToUriPath()->Data());
			FoundFriend->Presence.bIsJoinable = bIsJoinable;
		}

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying friend session details, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return false;
}

void FOnlineAsyncTaskLiveQueryFriendSessionDetails::Finalize()
{
	ManagerTask.SessionDetailsStatus = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
}
// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#endif
// @ATG_CHANGE :  END
