// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineUserInterface.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLivePackage.h"

class FOnlineSubsystemLive;
class FOnlineUserInfoLive;

using FOnlineUserListLiveMap = TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserInfoLive>>;

/**
 * Implements the XBox Live specific interface for friends
 */
class FOnlineUserLive :
	public IOnlineUser
{
	/** The async task classes require friendship */
	friend class FOnlineAsyncTaskLiveQueryUsers;

public:
	// IOnlineUser
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId>>& UserIds) override;
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<class FOnlineUser>>& OutUsers) override;
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId) override;
	virtual bool QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate = FOnQueryUserMappingComplete()) override;
	virtual bool QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate = FOnQueryExternalIdMappingsComplete()) override;
	virtual void GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds) override;
	virtual TSharedPtr<const FUniqueNetId> GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId) override;

	// FOnlineFriendsLive
	explicit FOnlineUserLive(FOnlineSubsystemLive* const InLiveSubsystem)
		: LiveSubsystem(InLiveSubsystem)
	{
		check(LiveSubsystem);
	}

	virtual ~FOnlineUserLive() = default;

private:
	/** Reference to the main Live subsystem */
	FOnlineSubsystemLive* const LiveSubsystem;

	/** Map of known user information */
	FOnlineUserListLiveMap UsersMap;
};

typedef TSharedPtr<FOnlineUserLive, ESPMode::ThreadSafe> FOnlineUserLivePtr;
