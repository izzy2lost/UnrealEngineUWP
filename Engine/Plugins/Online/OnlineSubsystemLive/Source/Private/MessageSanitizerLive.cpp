// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MessageSanitizerLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineError.h"

#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnlineSanitizeLive, Display, All);

FMessageSanitizerLive::FMessageSanitizerLive(FOnlineSubsystemLive* InLiveSubsystem) :
	LiveSubsystem(InLiveSubsystem)
{
	check(LiveSubsystem);
	AppResumeDelegateHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FMessageSanitizerLive::HandleAppResume);
	AppReactivatedDelegateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FMessageSanitizerLive::HandleAppReactivated);
}

FMessageSanitizerLive::~FMessageSanitizerLive()
{
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(AppResumeDelegateHandle);
}

void FMessageSanitizerLive::HandleAppResume()
{
	UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("FMessageSanitizerLive::HandleAppResume"));
	ResetBlockedUserCache();
}

void FMessageSanitizerLive::HandleAppReactivated()
{
	UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("FMessageSanitizerLive::HandleAppReactivated"));
	ResetBlockedUserCache();
}

void FMessageSanitizerLive::SanitizeDisplayName(const FString& DisplayName, const FOnMessageProcessed& CompletionDelegate)
{
	CompletionDelegate.ExecuteIfBound(true, DisplayName);
}

void FMessageSanitizerLive::SanitizeDisplayNames(const TArray<FString>& DisplayNames, const FOnMessageArrayProcessed& CompletionDelegate)
{
	CompletionDelegate.ExecuteIfBound(true, DisplayNames);
}

void FMessageSanitizerLive::QueryBlockedUser(int32 LocalUserNum, const FString& FromUserIdStr, const FOnQueryUserBlockedResponse& CompletionDelegate)
{
	IOnlineIdentityPtr IdentityInt = LiveSubsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
		if (LocalUserId.IsValid())
		{
			const FString LocalUserIdStr = LocalUserId->ToString();
			FBlockedUserData& UserBlockQueries = UserBlockData.FindOrAdd(LocalUserIdStr);
			UserBlockQueries.LocalUserId = LocalUserIdStr;

			FRemoteUserBlockInfo& RemoteUserData = UserBlockQueries.RemoteUserData.FindOrAdd(FromUserIdStr);
			if (RemoteUserData.State == EOnlineAsyncTaskState::NotStarted ||
				RemoteUserData.State == EOnlineAsyncTaskState::Failed)
			{
				// Fail catchall, set to InProgress if successfully setup
				RemoteUserData.State = EOnlineAsyncTaskState::Failed;
				RemoteUserData.RemoteUserId = FromUserIdStr;

				FOnlineUserLivePtr UserInt = LiveSubsystem->GetUsersLive();
				if (UserInt.IsValid())
				{
					TSharedPtr<const FUniqueNetId> FromUserId = IdentityInt->CreateUniquePlayerId(FromUserIdStr);
					if (FromUserId.IsValid())
					{
						RemoteUserData.State = EOnlineAsyncTaskState::InProgress;
						TArray<TSharedRef<const FUniqueNetId>> UsersToQuery;
						UsersToQuery.Add(FromUserId.ToSharedRef());
						UserInt->QueryUserCommunicationPermissions(*LocalUserId, UsersToQuery, FOnLiveCommunicationPermissionsQueryComplete::CreateThreadSafeSP(this, &FMessageSanitizerLive::OnQueryBlockedUserComplete, CompletionDelegate));
					}
				}
			}

			if (RemoteUserData.State == EOnlineAsyncTaskState::InProgress)
			{
				// Just wait for the existing query to finish
				RemoteUserData.Delegates.Add(CompletionDelegate);
			}
			else 
			{
				// Query previously complete or failed, return a response
				ensure(RemoteUserData.State == EOnlineAsyncTaskState::Done || RemoteUserData.State == EOnlineAsyncTaskState::Failed);
				ensure(RemoteUserData.Delegates.Num() == 0);
				
				UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("QueryBlockedUser - cached result local(%s) remote(%s) blocked:%d"), *LocalUserIdStr, *FromUserIdStr, RemoteUserData.bIsBlocked);

				FBlockedQueryResult Result;
				Result.UserId = FromUserIdStr;
				Result.bIsBlocked = (RemoteUserData.State != EOnlineAsyncTaskState::Failed) ? RemoteUserData.bIsBlocked : true;
				CompletionDelegate.ExecuteIfBound(Result);
			}
		}
	}
}

void FMessageSanitizerLive::OnQueryBlockedUserComplete(const FOnlineError& RequestStatus, const TSharedRef<const FUniqueNetId>& RequestingUser, const FCommunicationPermissionResultsMap& Results, FOnQueryUserBlockedResponse CompletionDelegate)
{
	if (RequestingUser->IsValid())
	{
		const FString LocalUserId = RequestingUser->ToString();

		FBlockedUserData* UserBlockQueries = UserBlockData.Find(LocalUserId);
		if (UserBlockQueries)
		{
			for (const TPair<TSharedRef<const FUniqueNetId>, bool>& Result : Results)
			{
				FString RemoteUserId = Result.Key->ToString();
				bool bHasPermission = Result.Value;
				FRemoteUserBlockInfo* RemoteUserData = UserBlockQueries->RemoteUserData.Find(RemoteUserId);
				if (RemoteUserData)
				{
					UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("OnQueryBlockedUserComplete - user(%s) blocked:%d"), *RemoteUserId, !bHasPermission);

					RemoteUserData->bIsBlocked = !bHasPermission;
					RemoteUserData->State = RequestStatus.bSucceeded ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;

					TArray<FOnQueryUserBlockedResponse> DelegatesCopy = RemoteUserData->Delegates;
					RemoteUserData->Delegates.Empty();

					FBlockedQueryResult Result;
					Result.UserId = MoveTemp(RemoteUserId);
					Result.bIsBlocked = RemoteUserData->bIsBlocked;

					// fire delegates for any/all QueryBlockedUser calls made while this one was in flight
					for (const FOnQueryUserBlockedResponse& Delegate : DelegatesCopy)
					{
						Delegate.ExecuteIfBound(Result);
					}
				}
				else
				{
					UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("OnQueryBlockedUserComplete: No remote data found for user %s"), *RemoteUserId);
				}
			}
		}
		else
		{
			UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("OnQueryBlockedUserComplete: No local data found for user %s"), *LocalUserId);
		}
	}
	else
	{
		UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("OnQueryBlockedUserComplete: Invalid requesting user"));
	}
}

void FMessageSanitizerLive::ResetBlockedUserCache()
{
	UE_LOG(LogOnlineSanitizeLive, Verbose, TEXT("ResetBlockedUserCache"));
	for (TPair<FString, FBlockedUserData>& LocalUserBlockData : UserBlockData)
	{
		for (TPair<FString, FRemoteUserBlockInfo>& RemoteUserData : LocalUserBlockData.Value.RemoteUserData)
		{
			if (RemoteUserData.Value.State != EOnlineAsyncTaskState::InProgress)
			{
				RemoteUserData.Value.State = EOnlineAsyncTaskState::NotStarted;
			}
		}
	}
}
