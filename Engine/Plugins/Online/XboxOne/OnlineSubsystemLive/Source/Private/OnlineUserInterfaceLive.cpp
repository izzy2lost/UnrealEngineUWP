// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineUserInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveQueryUsers.h"
#include "OnlineFriendsInterfaceLive.h"

bool FOnlineUserLive::QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId>>& UserIds)
{
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
// Social manager provides a method for querying an arbitrary user list that will also keep it updated
// while respecting rate limiting.
#if USE_SOCIAL_MANAGER
	auto FriendsInterface = LiveSubsystem->GetFriendsLive();

	auto ListReadyDelegate = FOnReadFriendsListComplete::CreateLambda(
		[this,UserIds](int32 UserNum, bool Result, const FString&, const FString& ErrorMessage)
	{
		TriggerOnQueryUserInfoCompleteDelegates(UserNum, Result, UserIds, ErrorMessage);
	});

	return FriendsInterface->ReadUserListInternal(LocalUserNum, TEXT("custom"), &UserIds, ListReadyDelegate);
#else
// @ATG_CHANGE : END	
	Microsoft::Xbox::Services::XboxLiveContext^ UserContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if (UserContext == nullptr)
	{
		LiveSubsystem->ExecuteNextTick([this, Localgit adggiUserNum, UserIds]()
		{
			const constexpr bool bWasSuccessful = false;
			TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bWasSuccessful, UserIds, TEXT("Could not find user context for user"));
		});
		return false;
	}

	if (UserIds.Num() < 1)
	{
		LiveSubsystem->ExecuteNextTick([this, LocalUserNum, UserIds]()
		{
			const constexpr bool bWasSuccessful = false;
			TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bWasSuccessful, UserIds, TEXT("No users provided to QueryUserInfo"));
		});
		return false;
	}

	// Shouldn't happen if UserContext is not nullptr
	check(LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS);

	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryUsers>(LiveSubsystem, UserContext, UserIds, LocalUserNum, OnQueryUserInfoCompleteDelegates[LocalUserNum]);
	return true;
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
#endif
// @ATG_CHANGE : END	
}

bool FOnlineUserLive::GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser>>& OutUsers)
{
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
// Social manager provides a method for querying an arbitrary user list that will also keep it updated
// while respecting rate limiting.
#if USE_SOCIAL_MANAGER
	auto FriendsInterface = LiveSubsystem->GetFriendsLive();
	return FriendsInterface->GetFriendsList(LocalUserNum, TEXT("custom"), reinterpret_cast<TArray<TSharedRef<FOnlineFriend>>&>(OutUsers));
#else
// @ATG_CHANGE : END
	UNREFERENCED_PARAMETER(LocalUserNum);

	OutUsers.Empty(UsersMap.Num());
	for (const FOnlineUserListLiveMap::ElementType& Pair : UsersMap)
	{
		OutUsers.Emplace(Pair.Value);
	}

	return true;
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
#endif
// @ATG_CHANGE : END	
}

TSharedPtr<FOnlineUser> FOnlineUserLive::GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId)
{
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
// Social manager provides a method for querying an arbitrary user list that will also keep it updated
// while respecting rate limiting.
#if USE_SOCIAL_MANAGER
	auto FriendsInterface = LiveSubsystem->GetFriendsLive();
	return FriendsInterface->GetFriend(LocalUserNum, UserId, TEXT("custom"));
#else
// @ATG_CHANGE : END
	UNREFERENCED_PARAMETER(LocalUserNum);

	TSharedRef<FOnlineUserInfoLive>* FoundUserPtr = UsersMap.Find(static_cast<const FUniqueNetIdLive&>(UserId));
	if (FoundUserPtr != nullptr)
	{
		return *FoundUserPtr;
	}

	return nullptr;
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
#endif
// @ATG_CHANGE : END	
}

bool FOnlineUserLive::QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate /*= FOnQueryUserMappingComplete()*/)
{
	const FUniqueNetIdLive& UserIdLive = static_cast<FUniqueNetIdLive>(UserId);
	LiveSubsystem->ExecuteNextTick([Delegate, UserIdLive, DisplayNameOrEmail]()
	{
		const constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(bWasSuccessful, UserIdLive, DisplayNameOrEmail, FUniqueNetIdLive(), TEXT("Querying user id mapping is not supported"));
	});

	return false;
}

bool FOnlineUserLive::QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate /*= FOnQueryExternalIdMappingsComplete()*/)
{
	const FUniqueNetIdLive& UserIdLive = static_cast<FUniqueNetIdLive>(UserId);
	LiveSubsystem->ExecuteNextTick([Delegate, QueryOptions, UserIdLive]()
	{
		const constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(bWasSuccessful, UserIdLive, QueryOptions, TArray<FString>(), TEXT("Querying external user id mapping is not supported"));
	});

	return false;
}

void FOnlineUserLive::GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds)
{
	OutIds.Empty(ExternalIds.Num());
	OutIds.AddDefaulted(ExternalIds.Num());
}

TSharedPtr<const FUniqueNetId> FOnlineUserLive::GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId)
{
	return nullptr;
}
