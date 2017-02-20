// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
typedef TSharedPtr<class FOnlineUserLive, ESPMode::ThreadSafe> FOnlineUserLivePtr;
// @ATG_CHANGE : END

class FOnlineAsyncTask;

/** Log category for Live */
DECLARE_LOG_CATEGORY_EXTERN(LogOnlineSubsystemLive, Log, All);

/**
 *	OnlineSubsystemLive - Implementation of the online subsystem for Live services
 */
class ONLINESUBSYSTEMLIVE_API FOnlineSubsystemLive : 
	public FOnlineSubsystemImpl
{

public:

	virtual ~FOnlineSubsystemLive()
	{
	}

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
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
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

	// FOnlineSubsystemLive

	/**
	 * Is the Live API available for use
	 * @return true if Live functionality is available, false otherwise
	 */
	bool IsEnabled();

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemLive() :
		OnlineAsyncTaskThread(NULL)
	{}

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
	class FOnlineAsyncTaskManagerLive* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

PACKAGE_SCOPE:
	FOnlineIdentityLivePtr GetIdentityLive() { return IdentityInterface; }
	FOnlinePresenceLivePtr GetPresenceLive() { return PresenceInterface; }
	// @ATG_CHANGE : BEGIN 
	FOnlineSessionLivePtr GetSessionInterfaceLive();
	// @ATG_CHANGE : END

	FOnlineLeaderboardsLivePtr GetLeaderboardsInterfaceLive() { return LeaderboardsInterface; }
	FSessionMessageRouterPtr GetSessionMessageRouter() { return SessionMessageRouterInterface; }

	FOnlineMatchmakingInterfaceLivePtr GetMatchmakingInterfaceLive() { return MatchmakingInterfaceLive; }
	// @ATG_CHANGE : BEGIN Adding social features
	FOnlineFriendsLivePtr GetFriendsLive() { return FriendInterface; }
	// @ATG_CHANGE : END

	class FOnlineAsyncTaskManagerLive* GetAsyncTaskManager() { return OnlineAsyncTaskThreadRunnable; }

	void QueueAsyncTask(FOnlineAsyncTask* AsyncTask, bool bCanRunInParallel = false);

	Platform::String^		RemoveBracesFromGuidString( __in Platform::String^ guid );

	// @ATG_CHANGE : BEGIN Adding social features
	/** Returns the first available Live context, or null if none available.  Useful when user context is not available. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetDefaultLiveContext() const;
	// @ATG_CHANGE : END

	/** Returns the Live context for the given user, or null if the user could not be found. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetLiveContext(int32 LocalUserNum);

	/** Returns the Live context for the given user, or null if the user could not be found. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetLiveContext(const FUniqueNetId& UserId);
		
	/** Returns the Live context for the current user of the given session, or null if the session is invalid. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetLiveContext(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

	/** Returns the cached Live context for the given user. Creates and caches a new one if necessary. Session subscriptions require that you preserve the context. */
	Microsoft::Xbox::Services::XboxLiveContext^		GetLiveContext(Windows::Xbox::System::User^ LiveUser);

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

	EOnlineServerConnectionStatus::Type	ConvertedNetworkConnectivityLevel;
	bool bHasCalledNetworkStatusChangedAtLeastOnce;

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
	void RefreshLiveInfo(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession);
	void SetLastDiffedSession(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession);
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLastDiffedSession(const FName& SessionName);
	static bool AreSessionReferencesEqual(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ First, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ Second);
};

typedef TSharedPtr<FOnlineSubsystemLive, ESPMode::ThreadSafe> FOnlineSubsystemLivePtr;

