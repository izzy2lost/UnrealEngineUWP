#include "OnlineSubsystemLivePrivatePCH.h"

#include "OnlineSubsystemLive.h"
#include "OnlineUserInterfaceLive.h"
#include "OnlineFriendsInterfaceLive.h"

FOnlineUserLive::FOnlineUserLive(FOnlineSubsystemLive* InSubsystem) :
	LiveSubsystem(InSubsystem)
{

}

bool FOnlineUserLive::QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId> >& UserIds)
{
	if (!LiveSubsystem)
	{
		return false;
	}

	auto FriendsInterface = LiveSubsystem->GetFriendsLive();

	auto ListReadyDelegate = FOnReadFriendsListComplete::CreateLambda(
		[this,UserIds](int32 UserNum, bool Result, const FString&, const FString& ErrorMessage)
	{
		TriggerOnQueryUserInfoCompleteDelegates(UserNum, Result, UserIds, ErrorMessage);
	});

	return FriendsInterface->ReadUserListInternal(LocalUserNum, TEXT("custom"), &UserIds, ListReadyDelegate);
}

bool FOnlineUserLive::GetAllUserInfo(int32 LocalUserNum, TArray< TSharedRef<class FOnlineUser> >& OutUsers)
{
	if (!LiveSubsystem)
	{
		return false;
	}

	auto FriendsInterface = LiveSubsystem->GetFriendsLive();
	return FriendsInterface->GetFriendsList(LocalUserNum, TEXT("custom"), reinterpret_cast<TArray<TSharedRef<FOnlineFriend>>&>(OutUsers));
}

TSharedPtr<FOnlineUser> FOnlineUserLive::GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId)
{
	if (!LiveSubsystem)
	{
		return false;
	}

	auto FriendsInterface = LiveSubsystem->GetFriendsLive();
	return FriendsInterface->GetFriend(LocalUserNum, UserId, TEXT("custom"));
}