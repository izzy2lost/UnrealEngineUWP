// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveQueryPrivacyPermissions.h"
#include "OnlineSubsystemLive.h"

#include <collection.h>

using Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult;
using Microsoft::Xbox::Services::Privacy::PermissionCheckResult;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::IAsyncOperation;

FOnlineAsyncTaskLiveQueryPrivacyPermissions::FOnlineAsyncTaskLiveQueryPrivacyPermissions(FOnlineSubsystemLive* InLiveSubsystem,
																						 XboxLiveContext^ InLiveContext,
																						 const TArray<TSharedRef<const FUniqueNetId>>& InUsersToQuery,
																						 IVectorView<Platform::String^>^ InPermissionsToQueryVector,
																						 const FOnLivePrivacyPermissionsQueryComplete& InDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, UsersToQuery(InUsersToQuery)
	, PermissionsToQueryVector(InPermissionsToQueryVector)
	, Delegate(InDelegate)
	, OnlineError(false)
{
}

IAsyncOperation<IVectorView<MultiplePermissionsCheckResult^>^>^ FOnlineAsyncTaskLiveQueryPrivacyPermissions::CreateOperation()
{
	// Ensure we have permissions
	if (PermissionsToQueryVector->Size < 1)
	{
		OnlineError.SetFromErrorCode(TEXT("Error starting user privacy permissions query, no permissions were requested"));
		UE_LOG_ONLINE(Warning, TEXT("%s"), *OnlineError.ErrorCode);
		return nullptr;
	}

	// Convert our TArray of UniqueNetIds into a IVectorView of XUID strings
	Platform::Collections::Vector<Platform::String^>^ XUIDVector = ref new Platform::Collections::Vector<Platform::String^>();
	for (const TSharedRef<const FUniqueNetId>& UserRef : UsersToQuery)
	{
		if (UserRef->IsValid())
		{
			XUIDVector->Append(ref new Platform::String(*UserRef->ToString()));
		}
	}

	// Ensure we have some players
	if (XUIDVector->Size < 1)
	{
		OnlineError.SetFromErrorCode(TEXT("Error starting user privacy permissions query, no permissions were requested"));
		UE_LOG_ONLINE(Warning, TEXT("%s"), *OnlineError.ErrorCode);
		return nullptr;
	}

	// Ensure we don't have TOO many players
	// TODO: figure out actual maximum and replace the 50 here
	if (XUIDVector->Size > 50)
	{
		OnlineError.SetFromErrorCode(TEXT("Error starting user privacy permissions query, more than 50 users were requested"));
		UE_LOG_ONLINE(Warning, TEXT("%s"), *OnlineError.ErrorCode);
		return nullptr;
	}

	// Create our task
	try
	{
		IAsyncOperation<IVectorView<MultiplePermissionsCheckResult^>^>^ AsyncTask = LiveContext->PrivacyService->CheckMultiplePermissionsWithMultipleTargetUsersAsync(PermissionsToQueryVector, XUIDVector->GetView());
		return AsyncTask;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error starting user privacy permissions query, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryPrivacyPermissions::ProcessResult(const Concurrency::task<IVectorView<MultiplePermissionsCheckResult^>^>& CompletedTask)
{
	try
	{
		IVectorView<MultiplePermissionsCheckResult^>^ UserPermissionsResultsVector = CompletedTask.get();
	
		// Read our users
		const int32 UserResultCount = UserPermissionsResultsVector->Size;
		UserPermissionsMap.Empty(UserResultCount);
		for (int32 UserIndex = 0; UserIndex < UserResultCount; ++UserIndex)
		{
			MultiplePermissionsCheckResult^ UserPermissionsResult = UserPermissionsResultsVector->GetAt(UserIndex);
			check(UserPermissionsResult);

			const TSharedRef<const FUniqueNetId> Player = MakeShared<FUniqueNetIdLive>(UserPermissionsResult->XboxUserId);

			const int32 PermissionsCount = UserPermissionsResult->Items->Size;
			TMap<FString, bool> PermissionsMap;
			PermissionsMap.Empty(PermissionsCount);

			// Read this user's permissions result
			for (int32 PermissionIndex = 0; PermissionIndex < PermissionsCount; ++PermissionIndex)
			{
				PermissionCheckResult^ PermissionResult = UserPermissionsResult->Items->GetAt(PermissionIndex);
				check(PermissionResult);

				PermissionsMap.Emplace(PermissionResult->PermissionRequested->Data(), PermissionResult->IsAllowed);
			}

			UserPermissionsMap.Emplace(Player, MoveTemp(PermissionsMap));
		}

		OnlineError.bSucceeded = true;
	}
	catch (Platform::Exception^ Ex)
	{
		OnlineError.SetFromErrorCode(FString::Printf(TEXT("Error querying user privacy permissions, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data()));
		UE_LOG_ONLINE(Error, TEXT("%s"), *OnlineError.ErrorCode);
	}

	return OnlineError.bSucceeded;
}

void FOnlineAsyncTaskLiveQueryPrivacyPermissions::Finalize()
{
	// TODO: cache our results?
}

void FOnlineAsyncTaskLiveQueryPrivacyPermissions::TriggerDelegates()
{
	// @ATG_CHANGE : BEGIN UWP support
	Windows::Xbox::System::User^ RequestingUser = SystemUserFromXSAPIUser(LiveContext->User);
	// @ATG_CHANGE : END
	TSharedRef<const FUniqueNetId> RequestingNetId = MakeShared<FUniqueNetIdLive>(RequestingUser ? RequestingUser->XboxUserId : nullptr);
	Delegate.ExecuteIfBound(OnlineError, RequestingNetId, UserPermissionsMap);
}
