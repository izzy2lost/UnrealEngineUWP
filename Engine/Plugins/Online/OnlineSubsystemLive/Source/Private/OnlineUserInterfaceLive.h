// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineUserInterface.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLivePackage.h"

class FOnlineSubsystemLive;
class FOnlineUserInfoLive;
struct FOnlineError;

using FOnlineUserListLiveMap = TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserInfoLive>>;

/** Map of User's permission to communicate to other users */
using FCommunicationPermissionResultsMap = TMap<TSharedRef<const FUniqueNetId>, bool>;

/**
 * Delegate used when the user's communication permissions has completed
 *
 * @param RequestStatus If this request was successful or not
 * @param RequestingUser The user who generated this request
 * @param Results A Map of Users to communication results
 */
DECLARE_DELEGATE_ThreeParams(FOnLiveCommunicationPermissionsQueryComplete, const FOnlineError& /*RequestStatus*/, const TSharedRef<const FUniqueNetId>& /*RequestingUser*/, const FCommunicationPermissionResultsMap& /*Results*/);

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

PACKAGE_SCOPE:
	void QueryUserCommunicationPermissions(const FUniqueNetId& UserId, const TArray<TSharedRef<const FUniqueNetId> >& InUsersToQuery, const FOnLiveCommunicationPermissionsQueryComplete& CompletionDelegate);

private:
	void OnQueryUserCommunicationPermissionsComplete(const FOnlineError& RequestStatus, const TSharedRef<const FUniqueNetId>& RequestingUser, const TMap<TSharedRef<const FUniqueNetId>, TMap<FString, bool> >& Results, FOnLiveCommunicationPermissionsQueryComplete CompletionDelegate);

private:
	/** Reference to the main Live subsystem */
	FOnlineSubsystemLive* const LiveSubsystem;

	/** Map of known user information */
	FOnlineUserListLiveMap UsersMap;
};

typedef TSharedPtr<FOnlineUserLive, ESPMode::ThreadSafe> FOnlineUserLivePtr;
