// Copyright 1998-2016 Epic Games, Inc. All R//ights Reserved.

#pragma once

#include "OnlineFriendsInterface.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlinePresenceInterfaceLive.h"
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

PACKAGE_SCOPE:
	TSharedRef<const FUniqueNetIdLive> UserId;
	FString RealName;
	FString DisplayName;
	FString DisplayPicUrlRaw;
	EInviteStatus::Type InviteStatus;
	TSharedRef<FOnlineUserPresence> Presence;
};

class FOnlineBlockedPlayerLive :
	public FOnlineBlockedPlayer
{
public:
	// FOnlineFriendLive
	FOnlineBlockedPlayerLive(Platform::String^ InXUID);
	virtual ~FOnlineBlockedPlayerLive() = default;

	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

PACKAGE_SCOPE:
	TSharedRef<const FUniqueNetIdLive> UniqueNetIdLive;
};


DECLARE_MULTICAST_DELEGATE_FourParams(FOnReadFriendsListCompleteMulticast, int32, bool, const FString&, const FString&);

/**
 * Implements the XBox Live specific interface for friends
 */
class FOnlineFriendsLive :
	public IOnlineFriends
{
	/** The async task classes require friendship */
	friend class FOnlineAsyncTaskLiveQueryFriends;
	friend class FOnlineAsyncTaskLiveQueryFriendManagerTask;
	friend class FOnlineAsyncTaskLiveQueryAvoidList;

public:
	// IOnlineFriends
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName,  const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
 	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
 	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;

	// FOnlineFriendsLive
	explicit FOnlineFriendsLive(class FOnlineSubsystemLive* const InLiveSubsystem);

	/**
	* Friends tick (pumps social manager)
	*/
	void Tick(float DeltaTime);

	bool ReadUserListInternal(int32 LocalUserNum, const FString& ListName, const TArray<TSharedRef<const FUniqueNetId> >* UserIds, const FOnReadFriendsListComplete& Delegate);

	/** Reference to the owning subsystem */
	virtual ~FOnlineFriendsLive()
	{

	}

private:
	/** Reference to the main Live subsystem */
	class FOnlineSubsystemLive* const LiveSubsystem;

#if USE_SOCIAL_MANAGER
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
#else
	/** Map of local users to map of their friends */
	FOnlineUserFriendsListLiveMap FriendsMap;
#endif
	/** These are users we have asked not to play with (similar to a blocklist) */
	TMap<FUniqueNetIdLive, TArray<TSharedRef<FOnlineBlockedPlayerLive>>> AvoidListMap;
};

typedef TSharedPtr<FOnlineFriendsLive, ESPMode::ThreadSafe> FOnlineFriendsLivePtr;