
#pragma once

#include "OnlineSubsystemLivePackage.h"
#include "OnlineUserInterface.h"

/**
 *	Interface class for obtaining online User info
 */
class FOnlineUserInterfaceLive : public IOnlineUser
{

public:

	/**
	 * Starts an async task that queries/reads the info for a list of users
	 *
	 * @param LocalUserNum the user requesting the query
	 * @param UserIds list of users to read info about
	 *
	 * @return true if the read request was started successfully, false otherwise
	 */
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId> >& UserIds) override;

	/**
	 * Obtains the cached list of online user info 
	 *
	 * @param LocalUserNum the local user that queried for online user data
	 * @param OutUsers [out] array that receives the copied data
	 *
	 * @return true if user info was found
	 */
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray< TSharedRef<class FOnlineUser> >& OutUsers) override;

	/**
	 * Get the cached user entry for a specific user id if found
	 *
	 * @param LocalUserNum the local user that queried for online user data
	 * @param UserId id to use for finding the cached online user
	 *
	 * @return user info or null ptr if not found
	 */
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId) override;

	/**
	 * Contacts server to obtain a user id from an arbitrary user-entered name string, eg. display name
	 *
	 * @param UserId id of the user that is requesting the name string lookup
	 * @param DisplayNameOrEmail a string of a display name or email to attempt to map to a user id
	 *
	 * @return true if the operation was started successfully
	 */
	virtual bool QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate = FOnQueryUserMappingComplete()) override { return false; }

	/**
	 * Contacts server to obtain user ids from external ids
	 *
	 * @param UserId id of the user that is requesting the name string lookup
	 * @param AuthType auth type that the external ids represent
	 * @param ExternalIds array of external ids to attempt to map to user ids
	 *
	 * @return true if the operation was started successfully
	 */
	virtual bool QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate = FOnQueryExternalIdMappingsComplete()) override { return false; }

	/**
	 * Get the cached user ids for the specified external ids
	 *
	 * @param AuthType auth type that the external ids represent
	 * @param ExternalIds array of external ids to map to user ids
	 * @param OutIds array of user ids that map to the specified external ids (can contain null entries)
	 */
	virtual void GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds) override {}

	/**
	 * Get the cached user id for the specified external id
	 *
	 * @param AuthType auth type that the external ids represent
	 * @param ExternalId external id to obtain user id for

	 * @return user info or null ptr if not found
	 */
	virtual TSharedPtr<const FUniqueNetId> GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId) override { return nullptr; }

PACKAGE_SCOPE:
	FOnlineUserInterfaceLive(class FOnlineSubsystemLive* InSubsystem);

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;
};
