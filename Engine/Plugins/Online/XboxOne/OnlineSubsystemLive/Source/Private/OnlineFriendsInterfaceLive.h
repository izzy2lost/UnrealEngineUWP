//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include "OnlineSubsystemLivePackage.h"
#include "OnlineFriendsInterface.h"
#include "OnlineSubsystemLiveTypes.h"

namespace FriendsListsNames
{
	extern const FString Default;
	extern const FString OnlinePlayers;
	extern const FString InGamePlayers;
	extern const FString Custom;
}

namespace FriendUserAttributes
{
	extern const FString DisplayPicUrlRaw;
}

class FOnlineFriendLive : public FOnlineFriend
{
public:
	FOnlineFriendLive(class FOnlineSubsystemLive* Subsystem, Microsoft::Xbox::Services::Social::Manager::XboxSocialUser^ User);

	/**
	* @return Id associated with the user account provided by the online service during registration
	*/
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override { return UserId; }
	/**
	* @return the real name for the user if known
	*/
	virtual FString GetRealName() const override { return RealName; }
	/**
	* @return the nickname of the user if known
	*/
	virtual FString GetDisplayName(const FString& Platform = FString()) const override { return DisplayName; }
	/**
	* @return Any additional user data associated with a registered user
	*/
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		if (AttrName.Equals(FriendUserAttributes::DisplayPicUrlRaw, ESearchCase::IgnoreCase))
		{
			OutAttrValue = DisplayPicUrlRaw;
			return true;
		}
		return false;
	}

	/**
	* @return the current invite status of a friend wrt to user that queried
	*/
	virtual EInviteStatus::Type GetInviteStatus() const override { return InviteStatus; }

	/**
	* @return presence info for an online friend
	*/
	virtual const FOnlineUserPresence& GetPresence() const override { return *Presence; }

private:
	TSharedRef<const FUniqueNetIdLive> UserId;
	FString RealName;
	FString DisplayName;
	FString DisplayPicUrlRaw;
	EInviteStatus::Type InviteStatus;
	TSharedRef<FOnlineUserPresence> Presence;
};

DECLARE_MULTICAST_DELEGATE_FourParams(FOnReadFriendsListCompleteMulticast, int32, bool, const FString&, const FString&);

const FString CUSTOM_USER_LIST_NAME = TEXT("Custom");

class FOnlineFriendsLive : public IOnlineFriends
{
public:

	//IOnlineFriends
	/**
	* Starts an async task that reads the named friends list for the player
	*
	* @param LocalUserNum the user to read the friends list of
	* @param ListName name of the friends list to read
	*
	* @return true if the read request was started successfully, false otherwise
	*/
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate);

	/**
	* Starts an async task that deletes the named friends list for the player
	*
	* @param LocalUserNum the user to delete the friends list for
	* @param ListName name of the friends list to delete
	*
	* @return true if the delete request was started successfully, false otherwise
	*/
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate) { return false; }

	/**
	* Starts an async task that sends an invite to another player.
	*
	* @param LocalUserNum the user that is sending the invite
	* @param FriendId player that is receiving the invite
	* @param ListName name of the friends list to invite to
	*
	* @return true if the request was started successfully, false otherwise
	*/
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate) { return false; }

	/**
	* Starts an async task that accepts an invite from another player.
	*
	* @param LocalUserNum the user that is accepting the invite
	* @param FriendId player that had sent the pending invite
	* @param ListName name of the friends list to operate on
	*
	* @return true if the request was started successfully, false otherwise
	*/
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate) { return false; }

	/**
	* Starts an async task that rejects an invite from another player.
	*
	* @param LocalUserNum the user that is rejecting the invite
	* @param FriendId player that had sent the pending invite
	* @param ListName name of the friends list to operate on
	*
	* @return true if the request was started successfully, false otherwise
	*/
	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) { return false; }

	/**
	* Starts an async task that deletes a friend from the named friends list
	*
	* @param LocalUserNum the user that is making the request
	* @param FriendId player that will be deleted
	* @param ListName name of the friends list to operate on
	*
	* @return true if the request was started successfully, false otherwise
	*/
	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) { return false; }

	/**
	* Copies the list of friends for the player previously retrieved from the online service
	*
	* @param LocalUserNum the user to read the friends list of
	* @param ListName name of the friends list to read
	* @param OutFriends [out] array that receives the copied data
	*
	* @return true if friends list was found
	*/
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends);

	/**
	* Get the cached friend entry if found
	*
	* @param LocalUserNum the user to read the friends list of
	* @param ListName name of the friends list to read
	* @param OutFriends [out] array that receives the copied data
	*
	* @return null ptr if not found
	*/
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName);

	/**
	* Checks that a unique player id is part of the specified user's friends list
	*
	* @param LocalUserNum the controller number of the associated user that made the request
	* @param FriendId the id of the player being checked for friendship
	* @param ListName name of the friends list to read
	*
	* @return true if friends list was found and the friend was valid
	*/
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName);

	/**
	* Query for recent players of the current user
	*
	* @param UserId user to query recent players for
	* @param Namespace the recent players namespace to retrieve
	*
	* @return true if query was started
	*/
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) { return false; }

	/**
	* Copies the cached list of recent players for a given user
	*
	* @param UserId user to retrieve recent players for
	* @param Namespace the recent players namespace to retrieve (if empty retrieve all namespaces)
	* @param OutRecentPlayers [out] array that receives the copied data
	*
	* @return true if recent players list was found for the given user
	*/
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) { return false; }

	/**
	* Block a player
	*
	* @param LocalUserNum The user to check for
	* @param PlayerId The player to block
	*
	* @return true if query was started
	*/
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) { return false; }

	/**
	* Unblock a player
	*
	* @param LocalUserNum The user to check for
	* @param PlayerId The player to unblock
	*
	* @return true if query was started
	*/
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) { return false; }

	/**
	* Query for blocked players
	*
	* @param UserId user to query blocked players for
	*
	* @return true if query was started
	*/
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) { return false; }

	/**
	* Get the list of blocked players
	*
	* @param UserId user to retrieve blocked players for
	* @param OuBlockedPlayers [out] array that receives the copied data
	*
	* @return true if blocked players list was found for the given user
	*/
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) { return false; }

	/**
	* Dump state information about blocked players
	*/
	virtual void DumpBlockedPlayers() const { }


PACKAGE_SCOPE:
	FOnlineFriendsLive(class FOnlineSubsystemLive* InSubsystem);

	/**
	* Friends tick (pumps social manager)
	*/
	void Tick(float DeltaTime);

	bool ReadUserListInternal(int32 LocalUserNum, const FString& ListName, const TArray<TSharedRef<const FUniqueNetId> >* UserIds, const FOnReadFriendsListComplete& Delegate);

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

	struct FUserListFromXboxSocialGroup
	{
		Microsoft::Xbox::Services::Social::Manager::XboxSocialUserGroup^ SocialGroup;
		TArray< TSharedRef<FOnlineFriend> > Users;
		FOnReadFriendsListCompleteMulticast ListReadyDelegate;

		void UpdateFromSocialGroup(class FOnlineSubsystemLive* LiveSubsystem);
		void TriggerDelegate(int32 LocalUserNum, const FString& ListName, const FString& ErrorMessage);
	};

	struct FLiveFriendsLists
	{
		static const uint32 FRIENDS_LIST_COUNT = 3;
		static const uint32 CUSTOM_LIST_INDEX = 3;

		FLiveFriendsLists() : SocialGraphLoaded(false), CustomListLoaded(false) {}

		FUserListFromXboxSocialGroup DefaultFriendsList;
		FUserListFromXboxSocialGroup OnlineFriendsList;
		FUserListFromXboxSocialGroup InGameFriendsList;
		FUserListFromXboxSocialGroup CustomList;

		//FAsyncFriendsList ListsByType[FRIENDS_LIST_COUNT + 1];
		bool SocialGraphLoaded;
		bool CustomListLoaded;

		FUserListFromXboxSocialGroup *UserListFromListName(const FString &ListName);
	};

	void UpdateFromSocialEvent(Microsoft::Xbox::Services::Social::Manager::SocialEvent ^Event);
	void TriggerReadUserListDelegates(FUserListFromXboxSocialGroup& UserList, const FString& ListName, const FString& ErrorMessage);

	Microsoft::Xbox::Services::Social::Manager::SocialManager^ XblSocialManager;
	TMap<FUniqueNetIdLive, FLiveFriendsLists> FriendsByUser;
};