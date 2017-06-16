// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#if !USE_SOCIAL_MANAGER
// @ATG_CHANGE :  END

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlineFriendsInterfaceLive.h"

class FOnlineSubsystemLive;

/**
 * Async task to query our friends list
 */
class FOnlineAsyncTaskLiveQueryFriends
	: public FOnlineAsyncTaskConcurrencyLive<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult^>
{
public:
	FOnlineAsyncTaskLiveQueryFriends(FOnlineSubsystemLive* const InLiveInterface, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const int32 InLocalUserNum, const FString& InListName, const FOnReadFriendsListComplete& InCompletionDelegate);
	virtual ~FOnlineAsyncTaskLiveQueryFriends() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryFriends"); }

	// Start in Game Thread
	virtual Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult^>^ CreateOperation() override;
	// Process in Online Thread
	virtual bool ProcessResult(const Concurrency::task<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult^>& CompletedTask);

	// Move results and trigger delegates in Game Thread
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

protected:
	int32 LocalUserNum;
	FString ListName;
	FOnReadFriendsListComplete Delegate;

	int32 FoundItemCount;
	FString OutError;

	FOnlineFriendsListLiveMap FriendsListMap;
};

/**
 * Async task to manage calling delegates and updating the local cache after the other friends-list tasks are completed
 */
class FOnlineAsyncTaskLiveQueryFriendManagerTask
	: public FOnlineAsyncTaskLive
{
	friend class FOnlineAsyncTaskLiveQueryFriendAccountDetails;
	friend class FOnlineAsyncTaskLiveQueryFriendPresenceDetails;

public:
	FOnlineAsyncTaskLiveQueryFriendManagerTask(FOnlineSubsystemLive* const InSubsystem, FOnlineFriendsListLiveMap&& InFriendsListMap, const int32 InLocalUserNum, FString&& InListName, FOnReadFriendsListComplete&& InDelegate)
		: FOnlineAsyncTaskLive(InSubsystem, InLocalUserNum)
		, FriendsListMap(MoveTemp(InFriendsListMap))
		, LocalUserNum(InLocalUserNum)
		, ListName(MoveTemp(InListName))
		, Delegate(MoveTemp(InDelegate))
		, AccountDetailsStatus(EOnlineAsyncTaskState::NotStarted)
		, PresenceDetailsStatus(EOnlineAsyncTaskState::NotStarted)
		, SessionDetailsStatus(EOnlineAsyncTaskState::NotStarted)
	{
	}

	virtual ~FOnlineAsyncTaskLiveQueryFriendManagerTask() = default;

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryFriendManagerTask"); }

	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

PACKAGE_SCOPE:
	FOnlineFriendsListLiveMap FriendsListMap;
	int32 LocalUserNum;
	FString ListName;
	FOnReadFriendsListComplete Delegate;

	volatile EOnlineAsyncTaskState::Type AccountDetailsStatus;
	volatile EOnlineAsyncTaskState::Type PresenceDetailsStatus;
	volatile EOnlineAsyncTaskState::Type SessionDetailsStatus;
};

/**
 * Async Task to query the account details of friends
 */
class FOnlineAsyncTaskLiveQueryFriendAccountDetails
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>
{
public:
	FOnlineAsyncTaskLiveQueryFriendAccountDetails(FOnlineSubsystemLive* const InLiveInterface,
												  Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
												  Windows::Foundation::Collections::IVectorView<Platform::String^>^ InXUIDsVectorView,
												  FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask);
	virtual ~FOnlineAsyncTaskLiveQueryFriendAccountDetails() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryFriendAccountDetails"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>& CompletedTask) override;

	virtual void Finalize();

protected:
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ XUIDsVectorView;
	FOnlineAsyncTaskLiveQueryFriendManagerTask& ManagerTask;
};

/**
 * Async Task to query the presence details of friends
 */
class FOnlineAsyncTaskLiveQueryFriendPresenceDetails
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord^>^>
{
public:
	FOnlineAsyncTaskLiveQueryFriendPresenceDetails(FOnlineSubsystemLive* const InLiveInterface,
												   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
												   Windows::Foundation::Collections::IVectorView<Platform::String^>^ InXUIDsVectorView,
												   FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask);
	virtual ~FOnlineAsyncTaskLiveQueryFriendPresenceDetails() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryFriendPresenceDetails"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord^>^>& CompletedTask);

	virtual void Finalize();

protected:
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ XUIDsVectorView;
	FOnlineAsyncTaskLiveQueryFriendManagerTask& ManagerTask;
};

/**
 * Async Task to query the session details of friends
 */
class FOnlineAsyncTaskLiveQueryFriendSessionDetails
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^>
{
public:
	FOnlineAsyncTaskLiveQueryFriendSessionDetails(FOnlineSubsystemLive* const InLiveInterface,
												  Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
												  Windows::Foundation::Collections::IVectorView<Platform::String^>^ InXUIDsVectorView,
												  FOnlineAsyncTaskLiveQueryFriendManagerTask& InManagerTask);
	virtual ~FOnlineAsyncTaskLiveQueryFriendSessionDetails() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryFriendSessionDetails"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^>& CompletedTask);

	virtual void Finalize();

protected:
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ XUIDsVectorView;
	FOnlineAsyncTaskLiveQueryFriendManagerTask& ManagerTask;
};
// @ATG_CHANGE :  BEGIN - Alternative Social implementation using Manager 
#endif // USE_SOCIAL_MANAGER
// @ATG_CHANGE :  END