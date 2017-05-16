// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#if !USE_SOCIAL_MANAGER	
// @ATG_CHANGE :  END

#include "OnlineAsyncTaskLiveQueryFriends.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineFriendsInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"

#include <collection.h>

using Microsoft::Xbox::Services::Presence::PresenceRecord;
using Microsoft::Xbox::Services::Social::XboxSocialRelationship;
using Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult;
using Microsoft::Xbox::Services::Social::XboxUserProfile;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::IAsyncOperation;

FOnlineAsyncTaskLiveQueryFriends::FOnlineAsyncTaskLiveQueryFriends(FOnlineSubsystemLive* InLiveInterface, XboxLiveContext^ InLiveContext, const int32 InLocalUserNum, const FString& InListName, const FOnReadFriendsListComplete& InCompletionDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, LocalUserNum(InLocalUserNum)
	, ListName(InListName)
	, Delegate(InCompletionDelegate)
	, FoundItemCount(0)
{
}

// @ATG_CHANGE : BEGIN - UWP support
PACK_WINRT()
IAsyncOperation<XboxSocialRelationshipResult^>^ FOnlineAsyncTaskLiveQueryFriends::CreateOperation()
{
	try
	{
		auto AsyncOp = LiveContext->SocialService->GetSocialRelationshipsAsync();
		return AsyncOp;
	}
	catch (Platform::COMException^ Ex)
	{
		OutError = FString::Printf(TEXT("Error querying friends, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
		UE_LOG_ONLINE(Error, *OutError);
	}

	return nullptr;
}
PACK_WINRT_REVERT()
// @ATG_CHANGE : END - UWP support

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
		TSharedRef<FOnlineFriendLive> LiveFriend = MakeShareable(new FOnlineFriendLive(FriendItem));
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
			// Build list of XUIDs to send to Live for more information
			// @ATG_CHANGE : BEGIN - bug fix
			Platform::Collections::Vector<Platform::String^>^ XUIDVector = ref new Platform::Collections::Vector<Platform::String^>();
			// @ATG_CHANGE : END -  bug fix
			for (const FOnlineFriendsListLiveMap::ElementType& FriendPair : FriendsListMap)
			{
				XUIDVector->Append(ref new Platform::String(*FriendPair.Key.UniqueNetIdStr));
			}

			IVectorView<Platform::String^>^ XUIDVectorView = XUIDVector->GetView();

			// Task to manage calling delegates and updating the local cache after the below tasks are finished
			FOnlineAsyncTaskLiveQueryFriendManagerTask* ManagerTask = new FOnlineAsyncTaskLiveQueryFriendManagerTask(Subsystem, MoveTemp(FriendsListMap), LocalUserNum, MoveTemp(ListName), MoveTemp(Delegate));
			{
				// Request Account Information (DisplayName / Potentially Icon information in the future)
				FOnlineAsyncTaskLiveQueryFriendAccountDetails* AccountDetailsTask = new FOnlineAsyncTaskLiveQueryFriendAccountDetails(Subsystem, LiveContext, XUIDVectorView, *ManagerTask);
				MyTaskManager->AddToParallelTasks(AccountDetailsTask);

				// Request Presence Information
				FOnlineAsyncTaskLiveQueryFriendPresenceDetails* PresenceDetailsTask = new FOnlineAsyncTaskLiveQueryFriendPresenceDetails(Subsystem, LiveContext, XUIDVectorView, *ManagerTask);
				MyTaskManager->AddToParallelTasks(PresenceDetailsTask);
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
	if (!bWasSuccessful || FoundItemCount == 0)
	{
		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, OutError);
	}
}

void FOnlineAsyncTaskLiveQueryFriendManagerTask::Tick()
{
	if ((AccountDetailsStatus == EOnlineAsyncTaskState::Done || AccountDetailsStatus == EOnlineAsyncTaskState::Failed)
		&& (PresenceDetailsStatus == EOnlineAsyncTaskState::Done || PresenceDetailsStatus == EOnlineAsyncTaskState::Failed))
	{
		bIsComplete = true;
		bWasSuccessful = (AccountDetailsStatus == EOnlineAsyncTaskState::Done && PresenceDetailsStatus == EOnlineAsyncTaskState::Done);
	}
}


void FOnlineAsyncTaskLiveQueryFriendManagerTask::Finalize()
{
	if (bWasSuccessful)
	{
		TSharedPtr<const FUniqueNetIdLive> UserId = StaticCastSharedPtr<const FUniqueNetIdLive>(Subsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
		if (UserId.IsValid())
		{
			Subsystem->GetFriendsLive()->FriendsMap.Emplace(*UserId, MoveTemp(FriendsListMap));
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
		if (AccountDetailsStatus == EOnlineAsyncTaskState::Failed && PresenceDetailsStatus == EOnlineAsyncTaskState::Failed)
		{
			ErrorString = TEXT("Failed to query account and presence details.");
		}
		else if (AccountDetailsStatus == EOnlineAsyncTaskState::Failed)
		{
			ErrorString = TEXT("Failed to query account details.");
		}
		else if (PresenceDetailsStatus == EOnlineAsyncTaskState::Failed)
		{
			ErrorString = TEXT("Failed to query presence details.");
		}
		else
		{
			ErrorString = TEXT("An unknown error has occured");
		}
	}


	Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorString);
}

FOnlineAsyncTaskLiveQueryFriendAccountDetails::FOnlineAsyncTaskLiveQueryFriendAccountDetails(FOnlineSubsystemLive* InLiveInterface, XboxLiveContext^ InLiveContext, IVectorView<Platform::String^>^ InXUIDsVectorView, FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask)
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

			FoundFriend->DisplayName = FOnlineUserLive::FilterPlayerName(Profile->GameDisplayName);
			FoundFriend->UserAttributes.Emplace(FString(TEXT("Gamerscore")), FString(Profile->Gamerscore->Data()));
			// This is a the URI to a resizeable display image for the user.  For example, &format=png&w=64&h=64
			// Valid Format: png
			// Valid Width/Height: 64/64, 208/208, or 424/424
			FoundFriend->UserAttributes.Emplace(FString(TEXT("DisplayPictureUri")), FString(Profile->GameDisplayPictureResizeUri->AbsoluteCanonicalUri->Data()));
		}

		ManagerTask.AccountDetailsStatus = EOnlineAsyncTaskState::Done;

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		ManagerTask.AccountDetailsStatus = EOnlineAsyncTaskState::Failed;
		UE_LOG_ONLINE(Error, TEXT("Error querying friend details, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return false;
}

FOnlineAsyncTaskLiveQueryFriendPresenceDetails::FOnlineAsyncTaskLiveQueryFriendPresenceDetails(FOnlineSubsystemLive* InLiveInterface, XboxLiveContext^ InLiveContext, IVectorView<Platform::String^>^ InXUIDsVectorView, FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask)
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
		UE_LOG_ONLINE(Error, TEXT("Error querying friend account details for friends list, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryFriendPresenceDetails::ProcessResult(const Concurrency::task<IVectorView<PresenceRecord^>^>& CompletedTask)
{
	try
	{
		IVectorView<PresenceRecord^>^ UserPresenceVector = CompletedTask.get();
		int32 FoundPresenceCount = UserPresenceVector->Size;
		for (int32 Index = 0; Index < FoundPresenceCount; ++Index)
		{
			PresenceRecord^ XboxPresence = UserPresenceVector->GetAt(Index);
			check(XboxPresence);

			TSharedRef<FOnlineFriendLive>& FoundFriend = ManagerTask.FriendsListMap.FindChecked(FUniqueNetIdLive(XboxPresence->XboxUserId));

			FoundFriend->Presence = FOnlineUserPresenceLive(XboxPresence);
		}

		ManagerTask.AccountDetailsStatus = EOnlineAsyncTaskState::Done;

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		ManagerTask.AccountDetailsStatus = EOnlineAsyncTaskState::Failed;
		UE_LOG_ONLINE(Error, TEXT("Error querying friend details, error: (%d) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return false;
}

// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#endif
// @ATG_CHANGE :  END
