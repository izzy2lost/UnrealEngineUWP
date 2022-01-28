// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveQueryUsers.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineUserInterfaceLive.h"

#include <collection.h>

using Microsoft::Xbox::Services::Social::XboxUserProfile;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::IAsyncOperation;

FOnlineAsyncTaskLiveQueryUsers::FOnlineAsyncTaskLiveQueryUsers(FOnlineSubsystemLive* InLiveSubsystem,
															   XboxLiveContext^ InLiveContext,
															   const TArray<TSharedRef<const FUniqueNetId>>& InUsersToQuery,
															   int32 InLocalUserNum,
															   const FOnQueryUserInfoComplete& InDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, UsersToQuery(InUsersToQuery)
	, LocalUserNum(InLocalUserNum)
	, Delegate(InDelegate)
	, OnlineError(false)
{
}

IAsyncOperation<IVectorView<XboxUserProfile^>^>^ FOnlineAsyncTaskLiveQueryUsers::CreateOperation()
{
	// Convert our TArray of UniqueNetIds into a IVectorView of XUID strings
	Platform::Collections::Vector<Platform::String^>^ XUIDVector = ref new Platform::Collections::Vector<Platform::String^>();
	for (const TSharedRef<const FUniqueNetId>& UserRef : UsersToQuery)
	{
		XUIDVector->Append(ref new Platform::String(*UserRef->ToString()));
	}
	IVectorView<Platform::String^>^ XUIDsVectorView = XUIDVector->GetView();

	try
	{
		IAsyncOperation<IVectorView<XboxUserProfile^>^>^ AsyncTask = LiveContext->ProfileService->GetUserProfilesAsync(XUIDsVectorView);
		return AsyncTask;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error starting user account details query, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryUsers::ProcessResult(const Concurrency::task<IVectorView<XboxUserProfile^>^>& CompletedTask)
{
	try
	{
		IVectorView<XboxUserProfile^>^ UserProfilesVector = CompletedTask.get();
		int32 FoundProfileCount = UserProfilesVector->Size;
		UserInfoMap.Empty(FoundProfileCount);
		for (int32 Index = 0; Index < FoundProfileCount; ++Index)
		{
			XboxUserProfile^ Profile = UserProfilesVector->GetAt(Index);
			check(Profile);

			TSharedRef<FOnlineUserInfoLive> NewUser = MakeShared<FOnlineUserInfoLive>(Profile);
			FUniqueNetIdLive UserId(*StaticCastSharedRef<const FUniqueNetIdLive>(NewUser->GetUserId()));

			UserInfoMap.Add(MoveTemp(UserId), NewUser);
		}

		OnlineError.bSucceeded = true;
	}
	catch (Platform::Exception^ Ex)
	{
		OnlineError.bSucceeded = false;
		OnlineError.SetFromErrorCode(TEXT("Unable to query users"));
		UE_LOG_ONLINE(Error, TEXT("Error querying user account details, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
	}

	return OnlineError.bSucceeded;
}

void FOnlineAsyncTaskLiveQueryUsers::Finalize()
{
	FOnlineUserLivePtr UserIntPtr = Subsystem->GetUsersLive();
	if (UserIntPtr.IsValid())
	{
		for (TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserInfoLive>>::ElementType& Pair : UserInfoMap)
		{
			UserIntPtr->UsersMap.Add(MoveTemp(Pair.Key), Pair.Value);
		}
	}
}

void FOnlineAsyncTaskLiveQueryUsers::TriggerDelegates()
{
	Delegate.Broadcast(LocalUserNum, OnlineError.bSucceeded, UsersToQuery, OnlineError.ErrorCode);
}
