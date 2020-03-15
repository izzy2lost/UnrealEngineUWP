// Copyright 1998-2018 Epic Games, Inc. All R//ights Reserved.

#pragma once

#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineSubsystemLiveTypes.h"

// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
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
// @ATG_CHANGE : END

class FOnlineSubsystemLive;

using FOnlineFriendsListLiveMap = TMap<FUniqueNetIdLive, TSharedRef<class FOnlineFriendLive>>;
using FOnlineUserFriendsListLiveMap = TMap<FUniqueNetIdLive, FOnlineFriendsListLiveMap>;

/**
 * Info associated with an online friend on the XBox Live service
 */
class FOnlineFriendLive :
	public FOnlineFriend
{
public:
	// FOnlineFriendLive
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 	
#if USE_SOCIAL_MANAGER
	FOnlineFriendLive(class FOnlineSubsystemLive* Subsystem, Microsoft::Xbox::Services::Social::Manager::XboxSocialUser^ User);
#else
	FOnlineFriendLive(Microsoft::Xbox::Services::Social::XboxSocialRelationship^ InSocialRelationship);
#endif
// @ATG_CHANGE : END
	virtual ~FOnlineFriendLive() = default;

	// FOnlineFriend
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

	// FOnlineUser
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	/** Helper to tell if this person has been been Favourited by the user; Favourite users should come first in a friendslist */
	bool IsFavorite() const;

PACKAGE_SCOPE:
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
#if USE_SOCIAL_MANAGER
	TSharedRef<const FUniqueNetIdLive> UniqueNetIdLive;
	FString RealName;
	EInviteStatus::Type InviteStatus;
	TSharedRef<FOnlineUserPresence> Presence;
	bool bIsFavorite;
#else // USE_SOCIAL_MANAGER
// @ATG_CHANGE : END
	/** The friend profile data */
	Microsoft::Xbox::Services::Social::XboxSocialRelationship^ SocialRelationship;

	/** Unique Live Id for the friend */
	TSharedRef<const FUniqueNetIdLive> UniqueNetIdLive;

	/** Presence info  */
	FOnlineUserPresenceLive Presence;
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 	
#endif // USE_SOCIAL_MANAGER
// @ATG_CHANGE : END

	/** The Name XBox Live tells us to call this user (May be GamerTag, may be Real Name)*/
	FString DisplayName;

	/** Custom attributes store on this user */
	TMap<FString, FString> UserAttributes;
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

// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
DECLARE_MULTICAST_DELEGATE_FourParams(FOnReadFriendsListCompleteMulticast, int32, bool, const FString&, const FString&);
// @ATG_CHANGE : END

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
	// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
	explicit FOnlineFriendsLive(class FOnlineSubsystemLive* const InLiveSubsystem);

	/**
	* Friends tick (pumps social manager)
	*/
	void Tick(float DeltaTime);

	bool ReadUserListInternal(int32 LocalUserNum, const FString& ListName, const TArray<TSharedRef<const FUniqueNetId> >* UserIds, const FOnReadFriendsListComplete& Delegate);
	// @ATG_CHANGE : END

	virtual ~FOnlineFriendsLive() = default;

PACKAGE_SCOPE:
	void OnUserPresenceUpdate(const FUniqueNetIdLive& FriendId, const TSharedRef<FOnlineUserPresenceLive>& UpdatedPresence);
	void OnUserSessionPresenceUpdate(const FUniqueNetIdLive& FriendId, const TSharedPtr<const FUniqueNetId>& NewSessionId, const bool bNewIsJoinable);

private:
	/** Reference to the main Live subsystem */
	class FOnlineSubsystemLive* const LiveSubsystem;

// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
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
// @ATG_CHANGE : END
	/** These are users we have asked not to play with (similar to a blocklist) */
	TMap<FUniqueNetIdLive, TArray<TSharedRef<FOnlineBlockedPlayerLive>>> AvoidListMap;

	/** Map of local players to their friends list change subscription token */
	TMap<FUniqueNetIdLive, Microsoft::Xbox::Services::Social::SocialRelationshipChangeSubscription^> FriendChangeSubscriptionMap;
};

typedef TSharedPtr<FOnlineFriendsLive, ESPMode::ThreadSafe> FOnlineFriendsLivePtr;