//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "OnlineSubsystemLivePrivatePCH.h"

#include "OnlineSubsystemLive.h"
#include "OnlineFriendsInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"

using namespace Microsoft::Xbox::Services::Social;
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

FOnlineFriendsLive::FOnlineFriendsLive(FOnlineSubsystemLive* InSubsystem) :
	LiveSubsystem(InSubsystem)
{
	// Cache this because calling WinRT statics is non-trivial
	XblSocialManager = SocialManager::SingletonInstance;
}


bool FOnlineFriendsLive::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	return ReadUserListInternal(LocalUserNum, ListName, nullptr, Delegate);
}

bool FOnlineFriendsLive::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUserNum);

	if (!UserId.IsValid())
	{
		UE_LOG(LogOnlineSubsystemLive, Error, TEXT("Invalid user index %d."), LocalUserNum);
		return false;
	}

	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*UserId));
	if (ListCollection == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Unknown friends list %s."), *ListName);
		return false;
	}
	OutFriends = UserList->Users;

	return true;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsLive::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUserNum);

	if (!UserId.IsValid())
	{
		UE_LOG(LogOnlineSubsystemLive, Error, TEXT("Invalid user index %d."), LocalUserNum);
		return false;
	}

	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*UserId));
	if (ListCollection == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Unknown friends list %s."), *ListName);
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
}

bool FOnlineFriendsLive::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(LocalUserNum);

	if (!UserId.IsValid())
	{
		UE_LOG(LogOnlineSubsystemLive, Error, TEXT("Invalid user index %d."), LocalUserNum);
		return false;
	}

	FLiveFriendsLists* ListCollection = FriendsByUser.Find(FUniqueNetIdLive(*UserId));
	if (ListCollection == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Friends list has not been read for user %d."), LocalUserNum);
		return false;
	}

	FUserListFromXboxSocialGroup* UserList = ListCollection->UserListFromListName(ListName);
	if (UserList == nullptr)
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Unknown friends list %s."), *ListName);
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
}

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
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Received social event for unknown user %s"), *UniqueId.ToString());
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
			UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Error updating friends for %s: %s"), *UniqueId.ToString(), *ErrorMessage);
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
		UE_LOG(LogOnlineSubsystemLive, Error, TEXT("Invalid user index %d."), LocalUserNum);
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
				ListToUpdate->ListReadyDelegate.Broadcast(LocalUserNum, false, CUSTOM_USER_LIST_NAME, TEXT("User list changed"));
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
				UE_LOG(LogOnlineSubsystemLive, Error, TEXT("List of UserIds is required when querying the custom list."), *ListName);
				return false;
			}
			else
			{
				UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Unknown friends list %s."), *ListName);
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
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Getting friends list failed. Exception: %s."), Ex->ToString()->Data());
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