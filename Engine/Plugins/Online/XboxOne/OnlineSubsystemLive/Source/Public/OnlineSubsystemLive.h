// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemLivePackage.h"

// @ATG_CHANGE : BEGIN UWP LIVE support
#if PLATFORM_XBOXONE
#include "XboxOneAllowPlatformTypes.h"
#define _UITHREADCTXT_SUPPORT   0
#include <ppltasks.h>
#include "XboxOneHidePlatformTypes.h"
#elif PLATFORM_UWP
#include "AllowWindowsPlatformTypes.h"
#include <ppltasks.h>
#include "HideWindowsPlatformTypes.h"

namespace Windows
{
	namespace Xbox
	{
		namespace System
		{
			using User = EraAdapter::Windows::Xbox::System::User;
			using IUser = EraAdapter::Windows::Xbox::System::User;
			using GetTokenAndSignatureResult = Microsoft::Xbox::Services::System::GetTokenAndSignatureResult;
		}
	}
}
#endif
// @ATG_CHANGE : END

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionLive, ESPMode::ThreadSafe> FOnlineSessionLivePtr;
typedef TSharedPtr<class FOnlineProfileLive, ESPMode::ThreadSafe> FOnlineProfileLivePtr;
typedef TSharedPtr<class FOnlineFriendsLive, ESPMode::ThreadSafe> FOnlineFriendsLivePtr;
typedef TSharedPtr<class FOnlineUserCloudLive, ESPMode::ThreadSafe> FOnlineUserCloudLivePtr;
typedef TSharedPtr<class FOnlineLeaderboardsLive, ESPMode::ThreadSafe> FOnlineLeaderboardsLivePtr;
typedef TSharedPtr<class FOnlineVoiceLive, ESPMode::ThreadSafe> FOnlineVoiceLivePtr;
typedef TSharedPtr<class FOnlineExternalUILive, ESPMode::ThreadSafe> FOnlineExternalUILivePtr;
typedef TSharedPtr<class FOnlineIdentityLive, ESPMode::ThreadSafe> FOnlineIdentityLivePtr;
typedef TSharedPtr<class FOnlineAchievementsLive, ESPMode::ThreadSafe> FOnlineAchievementsLivePtr;
typedef TSharedPtr<class FOnlineEventsLive, ESPMode::ThreadSafe> FOnlineEventsLivePtr;
typedef TSharedPtr<class FOnlinePresenceLive, ESPMode::ThreadSafe> FOnlinePresenceLivePtr;
typedef TSharedPtr<class FOnlineMatchmakingInterfaceLive, ESPMode::ThreadSafe> FOnlineMatchmakingInterfaceLivePtr;
typedef TSharedPtr<class FSessionMessageRouter, ESPMode::ThreadSafe> FSessionMessageRouterPtr;
// @ATG_CHANGE : BEGIN Adding social features
typedef TSharedPtr<class FOnlineUserInterfaceLive, ESPMode::ThreadSafe> FOnlineUserLivePtr;
// @ATG_CHANGE : END

class FOnlineAsyncTask;
class FOnlineAsyncTaskManagerLive;
class FRunnableThread;
template<class FOnlineSubsystemClass> class FOnlineAsyncEvent;

/**
 *	OnlineSubsystemLive - Implementation of the online subsystem for Live services
 */
class ONLINESUBSYSTEMLIVE_API FOnlineSubsystemLive
	: public FOnlineSubsystemImpl
{

public:
	/**
	 * Forwards the invite check to the session interface. This is here because this is already
	 * a public header and I don't want to make OnlineSessionInterfaceLive a public header.
	 */
	void CheckPendingSessionInvite();

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual FOnlineMatchmakingInterfaceLivePtr GetMatchmakingInterface() const;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

	/**
	 * Is the Live API available for use
	 * @return true if Live functionality is available, false otherwise
	 */
	bool IsEnabled();

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemLive()
		: ConvertedNetworkConnectivityLevel(EOnlineServerConnectionStatus::Normal)
		, bHasCalledNetworkStatusChangedAtLeastOnce(false)
		, OnlineAsyncTaskThreadRunnable(nullptr)
		, OnlineAsyncTaskThread(nullptr)
	{
	}

	virtual ~FOnlineSubsystemLive() = default;
	/** Helpers to get typed Interface shared pointers */
	FOnlineSessionLivePtr GetSessionInterfaceLive();
	FOnlineIdentityLivePtr GetIdentityLive() const { return IdentityInterface; }
	FOnlinePresenceLivePtr GetPresenceLive() const { return PresenceInterface; }
	FOnlineLeaderboardsLivePtr GetLeaderboardsInterfaceLive() const { return LeaderboardsInterface; }
	FOnlineMatchmakingInterfaceLivePtr GetMatchmakingInterfaceLive() const { return MatchmakingInterfaceLive; }
	FSessionMessageRouterPtr GetSessionMessageRouter() const { return SessionMessageRouterInterface; }
	FOnlineFriendsLivePtr GetFriendsLive() const { return FriendInterface; }

	FOnlineAsyncTaskManagerLive* GetAsyncTaskManager() { check(OnlineAsyncTaskThreadRunnable != nullptr); return OnlineAsyncTaskThreadRunnable; }

	/** Helpers to manage queuing already created FOnlineAsyncItem */
	void QueueAsyncTask(FOnlineAsyncTask* const AsyncTask, const bool bCanRunInParallel = false);
	void QueueAsyncEvent(FOnlineAsyncEvent<FOnlineSubsystemLive>* const AsyncEvent);

	/** Create a new async task with the provided arguments and queue it to be processed in parallel with other tasks */
	template <typename TOnlineAsyncTask, typename... TArguments>
	FORCEINLINE void CreateAndDispatchAsyncTaskParallel(TArguments&&... Arguments)
	{
		check(OnlineAsyncTaskThreadRunnable);

		TOnlineAsyncTask* NewTask = new TOnlineAsyncTask(Forward<TArguments>(Arguments)...);
		OnlineAsyncTaskThreadRunnable->AddToParallelTasks(NewTask);
	}

	/** Create a new async task with the provided arguments and add it to the serial processing queue*/
	template <typename TOnlineAsyncTask, typename... TArguments>
	FORCEINLINE void CreateAndDispatchAsyncTaskSerial(TArguments&&... Arguments)
	{
		check(OnlineAsyncTaskThreadRunnable);

		TOnlineAsyncTask* NewTask = new TOnlineAsyncTask(Forward<TArguments>(Arguments)...);
		OnlineAsyncTaskThreadRunnable->AddToInQueue(NewTask);
	}

	/** Create a new async event with the provided arguments and queue it to be processed in the OutQueue */
	template <typename TOnlineAsyncEvent, typename... TArguments>
	FORCEINLINE void CreateAndDispatchAsyncEvent(TArguments&&... Arguments)
	{
		check(OnlineAsyncTaskThreadRunnable);

		TOnlineAsyncEvent* NewEvent = new TOnlineAsyncEvent(Forward<TArguments>(Arguments)...);
		OnlineAsyncTaskThreadRunnable->AddToOutQueue(NewEvent);
	}

	/** Returns the Live context for the given user, or null if the input could not be found/was invalid. */
	Microsoft::Xbox::Services::XboxLiveContext^ GetLiveContext(int32 LocalUserNum);
	Microsoft::Xbox::Services::XboxLiveContext^ GetLiveContext(const FUniqueNetId& UserId);
	Microsoft::Xbox::Services::XboxLiveContext^ GetLiveContext(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

	/** Returns the cached Live context for the given user. Creates and caches a new one if necessary. Session subscriptions require that you preserve the context. */
	Microsoft::Xbox::Services::XboxLiveContext^ GetLiveContext(Windows::Xbox::System::User^ LiveUser);

	/** Removes one leading and one trailing curly brace from the input string and returns a new string */
	Platform::String^ RemoveBracesFromGuidString( __in Platform::String^ guid );

	/** Updates our local caches with updated session info*/
	void RefreshLiveInfo(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession);

	/** Updates our local caches to specify this session was the latest seen */
	void SetLastDiffedSession(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession);

	/** Return the last seen session by a provided Name */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLastDiffedSession(const FName& SessionName);

	/** Helper to compare two sessions to see if they're the same underlying session */
	static bool AreSessionReferencesEqual(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ First, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ Second);

PACKAGE_SCOPE:
	EOnlineServerConnectionStatus::Type ConvertedNetworkConnectivityLevel;

	bool bHasCalledNetworkStatusChangedAtLeastOnce;

private:

	// @ATG_CHANGE : BEGIN 
	/** Interface to the session services */
	IOnlineSessionPtr SessionInterface;
	// @ATG_CHANGE : END

	/** Interface to the external UI services */
	FOnlineExternalUILivePtr ExternalUIInterface;

	/** Interface to the identity registration/auth services */
	FOnlineIdentityLivePtr IdentityInterface;

	// @ATG_CHANGE : BEGIN
	/** Interface to the voice chat services */
	IOnlineVoicePtr VoiceInterface;
	// @ATG_CHANGE : END

	/** Interface to the events services */
	FOnlineEventsLivePtr EventsInterface;

	/** Interface to the achievement services */
	FOnlineAchievementsLivePtr AchievementInterface;

	/** Interface to the rich presence services */
	FOnlinePresenceLivePtr PresenceInterface;

	/** Interface to the leaderboard services */
	FOnlineLeaderboardsLivePtr LeaderboardsInterface;

	/** Interface to the matchmaking services */
	FOnlineMatchmakingInterfaceLivePtr MatchmakingInterfaceLive;

	/** Interface to the mpsd shouldertap services */
	FSessionMessageRouterPtr SessionMessageRouterInterface;

	// @ATG_CHANGE : BEGIN Adding social features
	/** Interface to the user info service */
	FOnlineUserLivePtr UserInterface;

	/** Interface to the social service */
	FOnlineFriendsLivePtr FriendInterface;
	// @ATG_CHANGE : END

	/** Online async task runnable */
	FOnlineAsyncTaskManagerLive* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

PACKAGE_SCOPE:
	FOnlineIdentityLivePtr GetIdentityLive() { return IdentityInterface; }
	FOnlinePresenceLivePtr GetPresenceLive() { return PresenceInterface; }

	// @ATG_CHANGE : BEGIN Adding social features
	/** Returns the first available Live context, or null if none available.  Useful when user context is not available. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetDefaultLiveContext() const;
	// @ATG_CHANGE : END

	// @ATG_CHANGE : BEGIN 
	Microsoft::Xbox::Services::XboxLiveAppConfiguration^ GetApplicationConfig() { return ApplicationConfig; }
	// @ATG_CHANGE : END

	/** Utility function which makes it easy to run task continuations on the game thread. */
	template<class ResultType, class ContinuationType>
	void GameThreadContinuation(Windows::Foundation::IAsyncOperation<ResultType>^ Op, ContinuationType Continuation)
	{
		create_task(Op).then( [this,Continuation](concurrency::task<ResultType> Task )
		{
			GetAsyncTaskManager()->AddGenericToOutQueue( [Task,Continuation]() { Continuation(Task); } );
		});
	}

	// @ATG_CHANGE : BEGIN UWP LIVE support
	// Store single XboxLiveContext per user
	TMap<FString, Microsoft::Xbox::Services::XboxLiveContext^> CachedXboxLiveContexts;

	void HandleAppResume();

	// Store the singleton application config object because calling the static WinRT property every time is expensive.
	Microsoft::Xbox::Services::XboxLiveAppConfiguration^ ApplicationConfig;
	// @ATG_CHANGE : END
	mutable FCriticalSection LiveContextsLock;
	Windows::Foundation::EventRegistrationToken UserRemovedToken;
	FCriticalSection RefreshLock;
};

typedef TSharedPtr<FOnlineSubsystemLive, ESPMode::ThreadSafe> FOnlineSubsystemLivePtr;

