//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "OnlineSubsystemLivePrivatePCH.h"

#include "OnlineSubsystemLive.h"
#include "OnlineFriendsInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveQueryFriends.h"
#include "AsyncTasks/OnlineAsyncTaskLiveQueryAvoidList.h"

using namespace Microsoft::Xbox::Services::Social::Manager;

namespace FriendsListsNames
{
	const FString Default = EFriendsLists::ToString(EFriendsLists::Default);
	const FString OnlinePlayers = EFriendsLists::ToString(EFriendsLists::OnlinePlayers);
	const FString InGamePlayers = EFriendsLists::ToString(EFriendsLists::InGamePlayers);
	const FString Custom = TEXT("custom");
}

namespace FriendUserAttributes
{
	const FString DisplayPicUrlRaw = TEXT("DisplayPicUrlRaw");
}


FOnlineFriendLive::FOnlineFriendLive(FOnlineSubsystemLive* Subsystem, XboxSocialUser^ User) :
	UserId(MakeShareable(new FUniqueNetIdLive(User->XboxUserId))),
	Presence(Subsystem->GetPresenceLive()->CachePresenceFromLive(FUniqueNetIdLive(User->XboxUserId), User->PresenceRecord))
{
	RealName = User->RealName->Data();
	DisplayName = User->DisplayName->Data();
	DisplayPicUrlRaw = User->DisplayPicUrlRaw->Data();

	if (User->IsFollowedByCaller && User->IsFollowingUser)
	{
		InviteStatus = EInviteStatus::Accepted;
	}
	else if (User->IsFollowedByCaller)
	{
		InviteStatus = EInviteStatus::PendingOutbound;
	}
	else if (User->IsFollowingUser)
	{
		InviteStatus = EInviteStatus::PendingInbound;
	}
	else
	{
		InviteStatus = EInviteStatus::Unknown;
	}
}

FOnlineBlockedPlayerLive::FOnlineBlockedPlayerLive(Platform::String^ InXUID)
	: UniqueNetIdLive(MakeShareable(new FUniqueNetIdLive(InXUID->Data())))
{
}

TSharedRef<const FUniqueNetId> FOnlineBlockedPlayerLive::GetUserId() const
{
	return UniqueNetIdLive;
}

FString FOnlineBlockedPlayerLive::GetRealName() const
{
	// We don't currently query DisplayNames for avoided players
	return TEXT("BlockedPlayer");
}

FString FOnlineBlockedPlayerLive::GetDisplayName(const FString& Platform /*= FString()*/) const
{
	// We don't currently query DisplayNames for avoided players
	return TEXT("BlockedPlayer");
}

bool FOnlineBlockedPlayerLive::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	OutAttrValue.Empty();
	return false;
}

FOnlineFriendsLive::FOnlineFriendsLive(FOnlineSubsystemLive* InSubsystem) :
	LiveSubsystem(InSubsystem)
{
	// Cache this because calling WinRT statics is non-trivial
	XblSocialManager = SocialManager::SingletonInstance;
}



bool FOnlineFriendsLive::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate /*= FOnReadFriendsListComplete()*/)
{
#if USE_SOCIAL_MANAGER
	return ReadUserListInternal(LocalUserNum, ListName, nullptr, Delegate);
#else
	Microsoft::Xbox::Services::XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if (!UserLiveContext)
	{
		constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, FString::Printf(TEXT("Could not find user at index %d"), LocalUserNum));
		return false;
	}

	FOnlineAsyncTaskManagerLive* MyTaskManager = LiveSubsystem->GetAsyncTaskManager();
	check(MyTaskManager);

	FOnlineAsyncTaskLiveQueryFriends* NewTask = new FOnlineAsyncTaskLiveQueryFriends(LiveSubsystem, UserLiveContext, LocalUserNum, ListName, Delegate);
	MyTaskManager->AddToParallelTasks(NewTask);

	return true;
#endif
}

bool FOnlineFriendsLive::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate /*= FOnDeleteFriendsListComplete()*/)
{
	// Not Implemented
	LiveSubsystem->ExecuteNextTick([LocalUserNum, ListName, Delegate]()
	{
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("FOnlineFriendsLive::DeleteFriendsList is not currently supported"));
	});

	return false;
}

bool FOnlineFriendsLive::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate /*= FOnSendInviteComplete()*/)
{
	// Not Implemented
	const FUniqueNetIdLive& LiveFriendId = static_cast<const FUniqueNetIdLive&>(FriendId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LiveFriendId, ListName, Delegate]()
	{
		Delegate.ExecuteIfBound(LocalUserNum, false, LiveFriendId, ListName, TEXT("FOnlineFriendsLive::SendInvite is currently not implemented"));
	});

	return false;
}

bool FOnlineFriendsLive::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate /*= FOnAcceptInviteComplete()*/)
{
	// Not Implemented
	const FUniqueNetIdLive& LiveFriendId = static_cast<const FUniqueNetIdLive&>(FriendId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LiveFriendId, ListName, Delegate]()
	{
		Delegate.ExecuteIfBound(LocalUserNum, false, LiveFriendId, ListName, TEXT("FOnlineFriendsLive::AcceptInvite is currently not implemented"));
	});

	return false;
}

bool FOnlineFriendsLive::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	// Not Implemented
	const FUniqueNetIdLive& LiveFriendId = static_cast<const FUniqueNetIdLive&>(FriendId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LiveFriendId, ListName, this]()
	{
		TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, LiveFriendId, ListName, TEXT("FOnlineFriendsLive::RejectInvite is currently not implemented"));
	});

	return false;
}

bool FOnlineFriendsLive::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	// Not supported by Live
	const FUniqueNetIdLive& LiveFriendId = static_cast<const FUniqueNetIdLive&>(FriendId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LiveFriendId, ListName, this]()
	{
		TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, LiveFriendId, ListName, TEXT("FOnlineFriendsLive::DeleteFriend is not currently supported"));
	});

	return false;
}

bool FOnlineFriendsLive::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
#if USE_SOCIAL_MANAGER
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUserNum);

	if (!UserId.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("Invalid user index %d."), LocalUserNum);
		return false;
	}

	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*UserId));
	if (ListCollection == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unknown friends list %s."), *ListName);
		return false;
	}
	OutFriends = UserList->Users;

	return true;
#else
	TSharedPtr<const FUniqueNetIdLive> LiveUserId = StaticCastSharedPtr<const FUniqueNetIdLive>(LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
	if (!LiveUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not find a user for LocalUserNum %d"), LocalUserNum);
		OutFriends.Empty();
		return false;
	}

	FOnlineFriendsListLiveMap* UserFriendsList = FriendsMap.Find(*LiveUserId);
	if (UserFriendsList != nullptr)
	{
		OutFriends.Empty(UserFriendsList->Num());
		for (const FOnlineFriendsListLiveMap::ElementType& NetIdFriendPair : *UserFriendsList)
		{
			OutFriends.Emplace(NetIdFriendPair.Value);
		}
	}
	else
	{
		OutFriends.Empty();
		UE_LOG_ONLINE(Warning, TEXT("Could not find a friendslist for user %s; was this user's friendslist queried?"), *LiveUserId->ToString())
	}

	return true;
#endif
}

TSharedPtr<FOnlineFriend> FOnlineFriendsLive::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<const FUniqueNetIdLive> LiveUserId = StaticCastSharedPtr<const FUniqueNetIdLive>(LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
	if (!LiveUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not find a user for LocalUserNum %d"), LocalUserNum);
		return nullptr;
	}

#if USE_SOCIAL_MANAGER
	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*LiveUserId));
	if (ListCollection == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unknown friends list %s."), *ListName);
		return false;
	}

	for (auto Friend : UserList->Users)
	{
		if (*Friend->GetUserId() == FriendId)
		{
			return Friend;
		}
	}

	return nullptr;
#else
	FOnlineFriendsListLiveMap* UserFriendsList = FriendsMap.Find(*LiveUserId);
	if (UserFriendsList == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not find a friendslist for user %s; was this user's friendslist queried?"), *LiveUserId->ToString())
		return nullptr;
	}

	TSharedRef<FOnlineFriendLive>* FoundFriendPtr = UserFriendsList->Find(static_cast<const FUniqueNetIdLive&>(FriendId));
	if (FoundFriendPtr == nullptr)
	{
		return nullptr;
	}

	return *FoundFriendPtr;
#endif
}

bool FOnlineFriendsLive::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<const FUniqueNetIdLive> LiveUserId = StaticCastSharedPtr<const FUniqueNetIdLive>(LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum));
	if (!LiveUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not find a user for LocalUserNum %d"), LocalUserNum);
		return false;
	}

#if USE_SOCIAL_MANAGER
	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*LiveUserId));
	if (ListCollection == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unknown friends list %s."), *ListName);
		return false;
	}

	for (auto Friend : UserList->Users)
	{
		if (*Friend->GetUserId() == FriendId)
		{
			return true;
		}
	}

	return false;
#else
	FOnlineFriendsListLiveMap* UserFriendsList = FriendsMap.Find(*LiveUserId);
	if (UserFriendsList == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not find a friendslist for user %s; was this user's friendslist queried?"), *LiveUserId->ToString())
		return false;
	}

	return UserFriendsList->Contains(static_cast<const FUniqueNetIdLive&>(FriendId));
#endif
}

bool FOnlineFriendsLive::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	// Not Implemented
	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);
	LiveSubsystem->ExecuteNextTick([LiveUserId, Namespace, this]()
	{
		TriggerOnQueryRecentPlayersCompleteDelegates(LiveUserId, Namespace, false, TEXT("FOnlineFriendsLive::QueryRecentPlayers is currently not implemented"));
	});

	return false;
}

bool FOnlineFriendsLive::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	// Not Implemented
	OutRecentPlayers.Empty();
	return false;
}

bool FOnlineFriendsLive::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	// Not supported by Live
	const FUniqueNetIdLive& LivePlayerId = static_cast<const FUniqueNetIdLive&>(PlayerId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LivePlayerId, this]()
	{
		TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, false, LivePlayerId, TEXT(""), TEXT("FOnlineFriendsLive::BlockPlayer is not supported"));
	});

	return false;
}

bool FOnlineFriendsLive::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	// Not supported by Live
	const FUniqueNetIdLive& LivePlayerId = static_cast<const FUniqueNetIdLive&>(PlayerId);
	LiveSubsystem->ExecuteNextTick([LocalUserNum, LivePlayerId, this]()
	{
		TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, LivePlayerId, TEXT(""), TEXT("FOnlineFriendsLive::UnblockPlayer is not supported"));
	});

	return false;
}

bool FOnlineFriendsLive::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	Microsoft::Xbox::Services::XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(LiveUserId);
	if (!UserLiveContext)
	{
		LiveSubsystem->ExecuteNextTick([this, LiveUserId]()
		{
			constexpr bool bWasSuccessful = false;
			TriggerOnQueryBlockedPlayersCompleteDelegates(LiveUserId, bWasSuccessful, FString::Printf(TEXT("Could not find user %s"), *LiveUserId.ToString()));
		});

		return false;
	}

	if (FOnlineAsyncTaskManagerLive* MyAsyncTaskManager = LiveSubsystem->GetAsyncTaskManager())
	{
		FOnlineAsyncTaskLiveQueryAvoidList* AvoidListTask = new FOnlineAsyncTaskLiveQueryAvoidList(LiveSubsystem, UserLiveContext, LiveUserId);
		MyAsyncTaskManager->AddToParallelTasks(AvoidListTask);

		return true;
	}

	return false;
}

bool FOnlineFriendsLive::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	TArray<TSharedRef<FOnlineBlockedPlayerLive>>* UserAvoidList = AvoidListMap.Find(static_cast<const FUniqueNetIdLive&>(UserId));
	if (UserAvoidList == nullptr)
	{
		OutBlockedPlayers.Empty();
		return false;
	}

	OutBlockedPlayers.Empty(UserAvoidList->Num());
	for (const TSharedRef<FOnlineBlockedPlayerLive>& BlockedPlayerRef : *UserAvoidList)
	{
		OutBlockedPlayers.Emplace(BlockedPlayerRef);
	}

	return true;
}

void FOnlineFriendsLive::DumpBlockedPlayers() const
{
	for (const auto& LiveUserIdBlockedPlayerPtrPair : AvoidListMap)
	{
		for (const TSharedRef<FOnlineBlockedPlayerLive>& BlockedPlayer : LiveUserIdBlockedPlayerPtrPair.Value)
		{
			UE_LOG_ONLINE(Log, TEXT("User %s has player %s blocked"), *LiveUserIdBlockedPlayerPtrPair.Key.ToString(), *BlockedPlayer->UniqueNetIdLive->ToString());
		}
	}
}

#if USE_SOCIAL_MANAGER
void FOnlineFriendsLive::Tick(float DeltaTime)
{
	auto EventList = XblSocialManager->DoWork();
	for (auto Event : EventList)
	{
		UpdateFromSocialEvent(Event);
	}

	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	for (TMap<FUniqueNetIdLive, FLiveFriendsLists>::TIterator It(FriendsByUser); It; ++It)
	{
		auto LocalUserNum = Identity->GetControllerIndexForId(It->Key);
		FLiveFriendsLists& FriendsListCollection = It->Value;
		if (FriendsListCollection.SocialGraphLoaded)
		{
			FriendsListCollection.DefaultFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::Default, TEXT(""));
			FriendsListCollection.OnlineFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::OnlinePlayers, TEXT(""));
			FriendsListCollection.InGameFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::InGamePlayers, TEXT(""));
		}

		if (FriendsListCollection.CustomListLoaded)
		{
			FriendsListCollection.CustomList.TriggerDelegate(LocalUserNum, FriendsListsNames::Custom, TEXT(""));
		}
	}
}

void FOnlineFriendsLive::UpdateFromSocialEvent(SocialEvent^ Event)
{
	FUniqueNetIdLive UniqueId(Event->User->XboxUserId);
	FLiveFriendsLists* FriendsListCollection = FriendsByUser.Find(UniqueId);
	if (!FriendsListCollection)
	{
		UE_LOG_ONLINE(Warning, TEXT("Received social event for unknown user %s"), *UniqueId.ToString());
		return;
	}
	const auto Identity = LiveSubsystem->GetIdentityLive();
	auto LocalUserNum = Identity->GetControllerIndexForId(UniqueId);

	switch (Event->EventType)
	{
	case SocialEventType::LocalUserAdded:
		if (Event->ErrorCode == 0)
		{
			// We should have UserAdded events immediately following this to populate
			// the friends lists in time to trigger delegates correctly.
			FriendsListCollection->SocialGraphLoaded = true;
		}
		else
		{
			// Something went wrong.  Throw the user graph out so we can try again later.
			FString ErrorMessage = Event->ErrorMessage->Data();
			UE_LOG_ONLINE(Warning, TEXT("Error updating friends for %s: %s"), *UniqueId.ToString(), *ErrorMessage);
			FriendsListCollection->DefaultFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::Default, ErrorMessage);
			FriendsListCollection->OnlineFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::OnlinePlayers, ErrorMessage);
			FriendsListCollection->InGameFriendsList.TriggerDelegate(LocalUserNum, FriendsListsNames::InGamePlayers, ErrorMessage);
			FriendsListCollection->CustomList.TriggerDelegate(LocalUserNum, FriendsListsNames::Custom, ErrorMessage);
			FriendsByUser.Remove(UniqueId);
			SocialManager::SingletonInstance->RemoveLocalUser(Event->User);
		}
		break;

	case SocialEventType::SocialUserGroupLoaded:
		// Custom list
		FriendsListCollection->CustomList.UpdateFromSocialGroup(LiveSubsystem);
		FriendsListCollection->CustomListLoaded = true;
		break;

	default:
		// Something changed.  For now we just rebuild all the lists.  In the future
		// consider inspecting the event in detail and making more surgical changes.
		FriendsListCollection->DefaultFriendsList.UpdateFromSocialGroup(LiveSubsystem);
		FriendsListCollection->OnlineFriendsList.UpdateFromSocialGroup(LiveSubsystem);
		FriendsListCollection->InGameFriendsList.UpdateFromSocialGroup(LiveSubsystem);
		FriendsListCollection->CustomList.UpdateFromSocialGroup(LiveSubsystem);
		TriggerOnFriendsChangeDelegates(LocalUserNum);
		break;
	}
}

void FOnlineFriendsLive::FUserListFromXboxSocialGroup::TriggerDelegate(int32 LocalUserNum, const FString& ListName, const FString& ErrorMessage)
{
	ListReadyDelegate.Broadcast(LocalUserNum, ErrorMessage.IsEmpty(), ListName, ErrorMessage);
	ListReadyDelegate.Clear();
}

void FOnlineFriendsLive::FUserListFromXboxSocialGroup::UpdateFromSocialGroup(FOnlineSubsystemLive* LiveSubsystem)
{
	if (SocialGroup != nullptr)
	{
		Users.Empty();
		for (auto SocialUser : SocialGroup->Users)
		{
			Users.Add(MakeShareable(new FOnlineFriendLive(LiveSubsystem, SocialUser)));
		}
	}
}

bool FOnlineFriendsLive::ReadUserListInternal(int32 LocalUserNum, const FString& ListName, const TArray<TSharedRef<const FUniqueNetId> >* UserIds, const FOnReadFriendsListComplete& Delegate)
{
	if (!LiveSubsystem)
	{
		return false;
	}

	const auto Identity = LiveSubsystem->GetIdentityLive();
	if (!Identity.IsValid())
	{
		return false;
	}

	Windows::Xbox::System::User^ RequestingUser = Identity->GetUserForControllerIndex(LocalUserNum);
	if (!RequestingUser)
	{
		return false;
	}

	TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUserNum);

	if (!UserId.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("Invalid user index %d."), LocalUserNum);
		return false;
	}

	try
	{
		FLiveFriendsLists* CachedFriendsLists = FriendsByUser.Find(FUniqueNetIdLive(*UserId));
		if (CachedFriendsLists == nullptr)
		{
			// No previous requests for this user.  Register with the social manager to get their social graph tracked.
			XblSocialManager->AddLocalUser(XSAPIUserFromSystemUser(RequestingUser), SocialManagerExtraDetailLevel::NoExtraDetail);
			CachedFriendsLists = &FriendsByUser.Add(FUniqueNetIdLive(*UserId));
		}

		FUserListFromXboxSocialGroup* ListToUpdate = nullptr;
		if (UserIds)
		{
			check(ListName.Equals(FriendsListsNames::Custom, ESearchCase::IgnoreCase));

			ListToUpdate = &CachedFriendsLists->CustomList;
			if (ListToUpdate->SocialGroup != nullptr)
			{
				XblSocialManager->DestroySocialUserGroup(ListToUpdate->SocialGroup);
				ListToUpdate->ListReadyDelegate.Broadcast(LocalUserNum, false, FriendsListsNames::Custom, TEXT("User list changed"));
			}
			Platform::Collections::Vector<Platform::String^> ^XuidList = ref new Platform::Collections::Vector<Platform::String^>(UserIds->Num());
			for (uint32 i = 0; i < XuidList->Size; ++i)
			{
				XuidList->SetAt(i, ref new Platform::String(*(*UserIds)[i]->ToString()));
			}

			ListToUpdate->SocialGroup = XblSocialManager->CreateSocialUserGroupFromList(XSAPIUserFromSystemUser(RequestingUser), XuidList->GetView());
		}
		else
		{
			PresenceFilter SocialGroupFilter;
			if (ListName.Equals(FriendsListsNames::Default, ESearchCase::IgnoreCase))
			{
				SocialGroupFilter = PresenceFilter::All;
				ListToUpdate = &CachedFriendsLists->DefaultFriendsList;
			}
			else if (ListName.Equals(FriendsListsNames::OnlinePlayers, ESearchCase::IgnoreCase))
			{
				SocialGroupFilter = PresenceFilter::AllOnline;
				ListToUpdate = &CachedFriendsLists->OnlineFriendsList;
			}
			else if (ListName.Equals(FriendsListsNames::InGamePlayers, ESearchCase::IgnoreCase))
			{
				SocialGroupFilter = PresenceFilter::TitleOnline;
				ListToUpdate = &CachedFriendsLists->InGameFriendsList;
			}
			else if (ListName.Equals(FriendsListsNames::Custom, ESearchCase::IgnoreCase))
			{
				UE_LOG_ONLINE(Error, TEXT("List of UserIds is required when querying the custom list."), *ListName);
				return false;
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Unknown friends list %s."), *ListName);
				return false;
			}

			if (ListToUpdate->SocialGroup == nullptr)
			{
				ListToUpdate->SocialGroup = XblSocialManager->CreateSocialUserGroupFromFilters(XSAPIUserFromSystemUser(RequestingUser), SocialGroupFilter, RelationshipFilter::Friends);
			}
		}

		ListToUpdate->ListReadyDelegate.Add(Delegate);

		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Getting friends list failed. Exception: %s."), Ex->ToString()->Data());
		return false;
	}

}

FOnlineFriendsLive::FUserListFromXboxSocialGroup* FOnlineFriendsLive::FLiveFriendsLists::UserListFromListName(const FString &ListName)
{
	if (ListName.Equals(FriendsListsNames::Default, ESearchCase::IgnoreCase))
	{
		return &DefaultFriendsList;
	}
	else if (ListName.Equals(FriendsListsNames::OnlinePlayers, ESearchCase::IgnoreCase))
	{
		return &OnlineFriendsList;
	}
	else if (ListName.Equals(FriendsListsNames::InGamePlayers, ESearchCase::IgnoreCase))
	{
		return &InGameFriendsList;
	}
	else if (ListName.Equals(FriendsListsNames::Custom, ESearchCase::IgnoreCase))
	{
		return &CustomList;
	}
	else
	{
		return false;
	}
}
#endif