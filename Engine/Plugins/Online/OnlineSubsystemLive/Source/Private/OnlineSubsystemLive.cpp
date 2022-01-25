// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSubsystemLive.h"
#include "Modules/ModuleManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

#include "OnlineFriendsInterfaceLive.h"
#include "MessageSanitizerLive.h"
// #include "OnlineUserCloudInterfaceLive.h"
#include "OnlineLeaderboardInterfaceLive.h"
#include "OnlineExternalUIInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineStoreInterfaceLive.h"
#include "OnlinePurchaseInterfaceLive.h"
#include "OnlineAchievementsInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineVoiceInterfaceLive.h"
#include "OnlineUserInterfaceLive.h"
#include "SessionMessageRouter.h"
#include "OnlineMatchmakingInterfaceLive.h"
// @ATG_CHANGE : BEGIN - needed for pathing to cpprest dll
#include "Interfaces/IPluginManager.h"
// @ATG_CHANGE : END
#include "Framework/Application/SlateApplication.h"

// @ATG_CHANGE : BEGIN - Xim Support 
#if USE_XIM
#include "Xim/OnlineSessionInterfaceXim.h"
#include "Xim/SocketSubsystemXim.h"
#include "Xim/OnlineVoiceInterfaceXim.h"
#include "Xim/XimMessageRouter.h"
typedef FOnlineSessionXim FOnlineSessionImpl;
typedef FOnlineSessionXimPtr FOnlineSessionImplPtr;
typedef FOnlineVoiceXim FOnlineVoiceImpl;
typedef FOnlineVoiceXimPtr FOnlineVoiceImplPtr;
#else
typedef FOnlineSessionLive FOnlineSessionImpl;
typedef FOnlineSessionLivePtr FOnlineSessionImplPtr;
typedef FOnlineVoiceLive FOnlineVoiceImpl;
typedef FOnlineVoiceLivePtr FOnlineVoiceImplPtr;
#endif
// @ATG_CHANGE : END

using namespace Microsoft::Xbox::Services;
using namespace Windows::Networking::Connectivity;
using Microsoft::Xbox::Services::Social::SocialRelationshipChangeEventArgs;
using Microsoft::Xbox::Services::Presence::DevicePresenceChangeEventArgs;
using Microsoft::Xbox::Services::Presence::TitlePresenceChangeEventArgs;
using Microsoft::Xbox::Services::UserStatistics::StatisticChangeEventArgs;

// FOnlineSubsystemLiveModule

IMPLEMENT_MODULE(FOnlineSubsystemLiveModule, OnlineSubsystemLive);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryLive : public IOnlineFactory
{
private:
	/** Single instantiation of the LIVE interface */
	FOnlineSubsystemLivePtr& GetSingleton() const
	{
		static FOnlineSubsystemLivePtr LiveSingleton;
		return LiveSingleton;
	}

	virtual void DestroySubsystem()
	{
		FOnlineSubsystemLivePtr& LiveSingleton = GetSingleton();
		if (LiveSingleton.IsValid())
		{
			LiveSingleton->Shutdown();
			LiveSingleton.Reset();
		}
	}

public:
	FOnlineFactoryLive() {}
	virtual ~FOnlineFactoryLive()
	{
		DestroySubsystem();
	}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		FOnlineSubsystemLivePtr& LiveSingleton = GetSingleton();
		if (LiveSingleton.IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Can't create more than one instance of Live online subsystem!"));
			return nullptr;
		}

		LiveSingleton = MakeShared<FOnlineSubsystemLive, ESPMode::ThreadSafe>();
		if (LiveSingleton->IsEnabled())
		{
			if (!LiveSingleton->Init())
			{
				UE_LOG_ONLINE(Warning, TEXT("Live API failed to initialize!"));
				DestroySubsystem();
				return nullptr;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Live API disabled!"));
			DestroySubsystem();
			return nullptr;
		}

		return LiveSingleton;
	}
};

/**
 * Called right after the module DLL has been loaded and the module object has been created
 * Registers the actual implementation of the Live online subsystem with the engine
 */
void FOnlineSubsystemLiveModule::StartupModule()
{
	LiveFactory = MakeUnique<FOnlineFactoryLive>();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(LIVE_SUBSYSTEM, LiveFactory.Get());
}

/**
 * Called before the module is unloaded, right before the module object is destroyed.
 * Overloaded to shut down all loaded online subsystems
 */
void FOnlineSubsystemLiveModule::ShutdownModule()
{
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(LIVE_SUBSYSTEM);

	LiveFactory.Reset();
}

IOnlineSessionPtr FOnlineSubsystemLive::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemLive::GetFriendsInterface() const
{
	return FriendInterface;
}

IMessageSanitizerPtr FOnlineSubsystemLive::GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const
{
	return MessageSanitizer;
}

IOnlinePartyPtr FOnlineSubsystemLive::GetPartyInterface() const
{
	// Xbox live used to support this, but no longer available
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemLive::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemLive::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemLive::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemLive::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemLive::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemLive::GetVoiceInterface() const
{
	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemLive::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemLive::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemLive::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemLive::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemLive::GetStoreInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemLive::GetStoreV2Interface() const
{
// @ATG_CHANGE : UWP Live Support - BEGIN
#if WITH_MARKETPLACE
	return StoreInterface;
#else
	return nullptr;
#endif
// @ATG_CHANGE : END
}

IOnlinePurchasePtr FOnlineSubsystemLive::GetPurchaseInterface() const
{
// @ATG_CHANGE : UWP Live Support - BEGIN
#if WITH_MARKETPLACE
	return PurchaseInterface;
#else
	return nullptr;
#endif
// @ATG_CHANGE : END
}

IOnlineEventsPtr FOnlineSubsystemLive::GetEventsInterface() const
{
	return EventsInterface;
}

IOnlineAchievementsPtr FOnlineSubsystemLive::GetAchievementsInterface() const
{
	return AchievementInterface;
}

IOnlineSharingPtr FOnlineSubsystemLive::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemLive::GetUserInterface() const
{
	return UserInterface;
}

IOnlineMessagePtr FOnlineSubsystemLive::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemLive::GetPresenceInterface() const
{
	return PresenceInterface;
}

IOnlineChatPtr FOnlineSubsystemLive::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemLive::GetTurnBasedInterface() const
{
	return nullptr;
}

IOnlineTournamentPtr FOnlineSubsystemLive::GetTournamentInterface() const
{
	return nullptr;
}

FOnlineMatchmakingInterfaceLivePtr FOnlineSubsystemLive::GetMatchmakingInterface() const
{
	return MatchmakingInterfaceLive;
}

/**
 *	Give the online subsystem a chance to tick its tasks
 */
bool FOnlineSubsystemLive::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

	// @ATG_CHANGE : BEGIN - Xim Support
	FOnlineSessionImplPtr SessionImpl = StaticCastSharedPtr<FOnlineSessionImpl>(SessionInterface);
 	if (SessionImpl.IsValid())
	{
		SessionImpl->Tick(DeltaTime);
	}
	// @ATG_CHANGE : END

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Tick(DeltaTime);
	}

	if (ExternalUIInterface.IsValid())
	{
		ExternalUIInterface->Tick(DeltaTime);
	}

	// @ATG_CHANGE : BEGIN Adding social features
	if (FriendInterface.IsValid())
	{
		FriendInterface->Tick(DeltaTime);
	}
	// @ATG_CHANGE : END

	// @ATG_CHANGE : BEGIN - Stats 2017 implementation
#if USE_STATS_2017
	if (LeaderboardsInterface.IsValid())
	{
		LeaderboardsInterface->Tick(DeltaTime);
	}
#endif
	// @ATG_CHANGE : END

	return true;
}

class FAsyncEventConnectionStatusChanged : public FOnlineAsyncEvent<FOnlineSubsystemLive>
{
private:
	Windows::Networking::Connectivity::NetworkConnectivityLevel ConnectivityLevel;

public:
	FAsyncEventConnectionStatusChanged(FOnlineSubsystemLive* InLiveSubsystem, Windows::Networking::Connectivity::NetworkConnectivityLevel InConnectivityLevel) :
		FOnlineAsyncEvent( InLiveSubsystem ),
		ConnectivityLevel( InConnectivityLevel )
	{
	}

	virtual void Finalize() override
	{

	}

	virtual FString ToString() const override
	{
		return FString( "FAsyncEventConnectionStatusChanged" );
	}

	virtual void TriggerDelegates() override
	{
		switch ( ConnectivityLevel )
		{
			case NetworkConnectivityLevel::None: UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: None") ); break;
			case NetworkConnectivityLevel::ConstrainedInternetAccess: UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: ConstrainedInternetAccess") ); break;
			case NetworkConnectivityLevel::InternetAccess: UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: InternetAccess") ); break;
			case NetworkConnectivityLevel::LocalAccess: UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: LocalAccess") ); break;
// @ATG_CHANGE : UWP Live Support - BEGIN
#if PLATFORM_XBOXONE
			case NetworkConnectivityLevel::XboxLiveAccess: UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: XboxLiveAccess") ); break;
#endif
			default: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: Invalid") ); break;
		}

		EOnlineServerConnectionStatus::Type	ConvertedNetworkConnectivityLevelOnStack = EOnlineServerConnectionStatus::ServiceUnavailable;

#if PLATFORM_XBOXONE
		if ( ConnectivityLevel == NetworkConnectivityLevel::XboxLiveAccess )
		{
			ConvertedNetworkConnectivityLevelOnStack = EOnlineServerConnectionStatus::Connected;
		}
#elif PLATFORM_UWP
		if (ConnectivityLevel == NetworkConnectivityLevel::InternetAccess)
		{
			ConvertedNetworkConnectivityLevelOnStack = EOnlineServerConnectionStatus::Connected;
		}
#endif
// @ATG_CHANGE : UWP Live Support - END

		UE_LOG_ONLINE(Log, TEXT("NetworkStatusChangedEvent: OldConverted: %s, Converted: %s"), EOnlineServerConnectionStatus::ToString( Subsystem->ConvertedNetworkConnectivityLevel ), EOnlineServerConnectionStatus::ToString( ConvertedNetworkConnectivityLevelOnStack ) );

		if ( !Subsystem->bHasCalledNetworkStatusChangedAtLeastOnce || ConvertedNetworkConnectivityLevelOnStack != Subsystem->ConvertedNetworkConnectivityLevel )
		{
			Subsystem->bHasCalledNetworkStatusChangedAtLeastOnce = true;
			Subsystem->ConvertedNetworkConnectivityLevel = ConvertedNetworkConnectivityLevelOnStack;

			Subsystem->TriggerOnConnectionStatusChangedDelegates(Subsystem->GetSubsystemName().ToString(), EOnlineServerConnectionStatus::Normal, Subsystem->ConvertedNetworkConnectivityLevel);
		}
	}
};

bool FOnlineSubsystemLive::Init()
{
	const bool bLiveInit = true;

	bIgnoreNetworkStatusChanged = false;

	if (bLiveInit)
	{
		// @todo - still need to define Live socket subsystem
		//CreateLiveSocketSubsystem();
// @ATG_CHANGE : BEGIN - Adding XIM
#if PLATFORM_UWP
		TSharedPtr<IPlugin> LivePlugin = IPluginManager::Get().FindPlugin(TEXT("OnlineSubsystemLive"));
		if (!LivePlugin.IsValid())
		{
			UE_LOG_ONLINE(Error, TEXT("Failed to locate FPlugin instance describing Live plugin."), OnlineAsyncTaskThread->GetThreadID());
			return false;
		}

#if USE_XIM
		CreateXimSocketSubsystem();
#ifdef XIM_DLL
		// Must manually load XIM since staging puts in a location where it won't
		// be naturally picked up.
		void *XimDll = FPlatformProcess::GetDllHandle(*(LivePlugin->GetBaseDir() / XIM_DLL));
		if (!XimDll)
		{
			UE_LOG_ONLINE(Error, TEXT("Failed to load XIM dll from %s."), *(LivePlugin->GetBaseDir() / XIM_DLL));
			return false;
		}
#endif
#endif
// @ATG_CHANGE : END

// @ATG_CHANGE : BEGIN UWP LIVE support
#ifdef CPP_REST_DLL
		// Must manually load cpprest since staging puts in a location where it won't
		// be naturally picked up.
		void* CppRestDll = FPlatformProcess::GetDllHandle(*(LivePlugin->GetBaseDir() / CPP_REST_DLL));
		if (!CppRestDll)
		{
			UE_LOG(LogOnline, Error, TEXT("Failed to load cpprest dll from %s."), *(LivePlugin->GetBaseDir() / CPP_REST_DLL));
			return false;
		}
#endif
#endif
// @ATG_CHANGE : END

		// Create the online async task thread
		OnlineAsyncTaskThreadRunnable = MakeUnique<FOnlineAsyncTaskManagerLive>(this);

		OnlineAsyncTaskThread.Reset(FRunnableThread::Create(OnlineAsyncTaskThreadRunnable.Get(), *FString::Printf(TEXT("OnlineAsyncTaskThread %s"), *InstanceName.ToString())));
		check(OnlineAsyncTaskThread.IsValid());

		UE_LOG_ONLINE(Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID());

// @ATG_CHANGE : BEGIN - Adding XIM
#if PLATFORM_UWP
#if USE_XIM
		XimMessageRouter = MakeShared<FXimMessageRouter, ESPMode::ThreadSafe>(this);
#else
		SessionMessageRouterInterface = MakeShared<FSessionMessageRouter, ESPMode::ThreadSafe>(this);
		MatchmakingInterfaceLive = MakeShared<FOnlineMatchmakingInterfaceLive, ESPMode::ThreadSafe>(this);
#endif
		IdentityInterface = MakeShared<FOnlineIdentityLive, ESPMode::ThreadSafe>(this);
		IdentityInterface->RefreshGamepadsAndUsers();
#if WITH_MARKETPLACE
		StoreInterface = MakeShared<FOnlineStoreLive, ESPMode::ThreadSafe>(this);
		PurchaseInterface = MakeShared<FOnlinePurchaseLive, ESPMode::ThreadSafe>(this);
		PurchaseInterface->RegisterLivePurchaseHooks();
#endif
#if USE_XIM
		SessionInterface = MakeShared<FOnlineSessionXim, ESPMode::ThreadSafe>(this);
#else
		SessionInterface = MakeShared<FOnlineSessionLive, ESPMode::ThreadSafe>(this);
#endif
		FriendInterface = MakeShared<FOnlineFriendsLive, ESPMode::ThreadSafe>(this);
		MessageSanitizer = MakeShared<FMessageSanitizerLive, ESPMode::ThreadSafe>(this);
// 		UserCloudInterface = MakeShared<FOnlineUserCloudLive, ESPMode::ThreadSafe>(this);
		LeaderboardsInterface = MakeShared<FOnlineLeaderboardsLive, ESPMode::ThreadSafe>(this);
#if WITH_GAME_CHAT
#if USE_XIM
		FOnlineVoiceImplPtr VoiceImpl = MakeShared<FOnlineVoiceXim, ESPMode:ThreadSafe>(this);
#else
		FOnlineVoiceImplPtr VoiceImpl = MakeShared<FOnlineVoiceLive, ESPMode::ThreadSafe>(this);
#endif
		if (VoiceImpl->Init())
		{
			VoiceInterface = VoiceImpl;
		}
#endif //WITH_GAME_CHAT
#endif //PLATFORM_UWP
// @ATG_CHANGE : END
		ExternalUIInterface = MakeShared<FOnlineExternalUILive, ESPMode::ThreadSafe>(this);
		EventsInterface = MakeShared<FOnlineEventsLive, ESPMode::ThreadSafe>(this);
		AchievementInterface = MakeShared<FOnlineAchievementsLive, ESPMode::ThreadSafe>(this);
		PresenceInterface = MakeShared<FOnlinePresenceLive, ESPMode::ThreadSafe>(this);
		UserInterface = MakeShared<FOnlineUserLive, ESPMode::ThreadSafe>(this);

		bHasCalledNetworkStatusChangedAtLeastOnce = false;

		TWeakPtr<FOnlineSubsystemLive, ESPMode::ThreadSafe> WeakThis = AsShared();

		Windows::ApplicationModel::Core::CoreApplication::Resuming += ref new Windows::Foundation::EventHandler< Platform::Object^>(
			[WeakThis](Platform::Object^, Platform::Object^)
		{
			FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
			if (!StrongThis.IsValid())
			{
				UE_LOG_ONLINE(Verbose, TEXT("Ignoring ResumeEvent as we went away"));
				return;
			}

			// Handle multiplayer subscriptions and contexts next tick so that if this happens on the same frame as
			// the SessionMessageRouter's MultiplayerSubscriptionLost event, they will happen in the correct order
			// relative to each other.
			StrongThis->ExecuteNextTick([WeakThis]()
			{
				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring MultiplayerEventsSubscription as we went away"));
					return;
				}

				// On resume, multiplayer subscriptions are invalidated, so we need to re-subscribe to them.
				// Unsubscribe here, re-subscription will happen after the old contexts are cleared.
				StrongThis->SessionMessageRouterInterface->UnsubscribeAllUsersFromMultiplayerEvents();

				{
					// After resuming from suspend, the cached XboxLiveContexts are invalid. Clear them,
					// and they will be re-created on demand in GetLiveContext().
					FScopeLock Lock(&StrongThis->LiveContextsLock);

					StrongThis->CachedXboxLiveContexts.Reset();
				}

				StrongThis->SessionMessageRouterInterface->SubscribeAllUsersToMultiplayerEvents();
			});

			// After resuming, we may get a series of conflicting NetworkStatusChanged events
			// which should be ignored for a few seconds before polling the status
			// https://forums.xboxlive.com/questions/57371/networkconnectivitylevel-when-resuming-from-suspen.html
			StrongThis->bIgnoreNetworkStatusChanged = true;

			concurrency::task<void> delayedTask = concurrency::create_task([]()
			{
				concurrency::wait(5000);

			}).then([WeakThis]()
			{
				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring NetworkStatusChanged as we went away"));
					return;
				}

				StrongThis->bIgnoreNetworkStatusChanged = false;
				StrongThis->RefreshNetworkConnectivityLevel();
			});
		});

		Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged += ref new Windows::Networking::Connectivity::NetworkStatusChangedEventHandler( 
			[WeakThis] (Platform::Object^)
		{
			FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
			if (!StrongThis.IsValid())
			{
				UE_LOG_ONLINE(Verbose, TEXT("Ignoring NetworkStatusChanged as we went away"));
				return;
			}

			if (!StrongThis->bIgnoreNetworkStatusChanged)
			{
				StrongThis->RefreshNetworkConnectivityLevel();
			}
			else
			{
				UE_LOG_ONLINE(Verbose, TEXT("Ignoring NetworkStatusChanged"));
			}
		});

		// Clear cached XboxLiveContext when user is removed
		UserRemovedToken = Windows::Xbox::System::User::UserRemoved += ref new Windows::Foundation::EventHandler<Windows::Xbox::System::UserRemovedEventArgs^>(
			[WeakThis](Platform::Object^, Windows::Xbox::System::UserRemovedEventArgs^ Args)
		{
			FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
			if (!StrongThis.IsValid())
			{
				UE_LOG_ONLINE(Verbose, TEXT("Ignoring UserRemovedEventArgs as we went away"));
				return;
			}

				FScopeLock ScopeLock(&StrongThis->LiveContextsLock);

			// @ATG_CHANGE : BEGIN UWP support
			XboxLiveContext^ RemoveContext = StrongThis->CachedXboxLiveContexts.FindChecked(Args->User->XboxUserId->Data());
			RemoveContext->RealTimeActivityService->Deactivate();
					StrongThis->CachedXboxLiveContexts.Remove(Args->User->XboxUserId->Data());
			// @ATG_CHANGE : END UWP support 
			});

		// @ATG_CHANGE : BEGIN UWP support - delay setting this until after the above manual load steps for dependant libs
#if PLATFORM_UWP
		TitleId = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->TitleId;
#endif
		// @ATG_CHANGE : END
	}
	else
	{
		Shutdown();
	}

	return bLiveInit;
}

bool FOnlineSubsystemLive::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemLive::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	if (OnlineAsyncTaskThread.IsValid())
	{
		// Destroy the online async task thread
		OnlineAsyncTaskThread->Kill(true);
		OnlineAsyncTaskThread.Reset();
	}

	if (OnlineAsyncTaskThreadRunnable.IsValid())
	{
		OnlineAsyncTaskThreadRunnable.Reset();
	}

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	// Destruct the interfaces (in opposite order they were created)
	DESTRUCT_INTERFACE(UserInterface);
	DESTRUCT_INTERFACE(PresenceInterface);
	DESTRUCT_INTERFACE(AchievementInterface);
	DESTRUCT_INTERFACE(EventsInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(FriendInterface);
	DESTRUCT_INTERFACE(MessageSanitizer);
	DESTRUCT_INTERFACE(SessionInterface);
// @ATG_CHANGE : UWP Live Support - BEGIN
#if WITH_MARKETPLACE
	DESTRUCT_INTERFACE(PurchaseInterface);
	DESTRUCT_INTERFACE(StoreInterface);
#endif
// @ATG_CHANGE : UWP Live Support - END
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(MatchmakingInterfaceLive);
	DESTRUCT_INTERFACE(SessionMessageRouterInterface);

#undef DESTRUCT_INTERFACE
	// Clear cached XboxLiveContext when user is removed
	if (UserRemovedToken.Value != 0)
	{
		Windows::Xbox::System::User::UserRemoved -= UserRemovedToken;
	}

	return true;
}

FString FOnlineSubsystemLive::GetAppId() const
{
	// @ATG_CHANGE : UWP Live Support - BEGIN
	static const FString TitleId = FString::Printf(TEXT("%d"), Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->TitleId);
	// @ATG_CHANGE : UWP Live Support - END
	return TitleId;
}

bool FOnlineSubsystemLive::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}

	return false;
}

FText FOnlineSubsystemLive::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemLive", "OnlineServiceName", "Xbox Live");
}

// @ATG_CHANGE : BEGIN
bool FOnlineSubsystemLive::IsEnabled()
{
	// Check the ini for disabling Live
	bool bEnableLive = false;
	GConfig->GetBool(TEXT("OnlineSubsystemLive"), TEXT("bEnabled"), bEnableLive, GEngineIni);
#if !UE_BUILD_SHIPPING
	// Check the commandline for disabling Mcp, but not in shipping
	bEnableLive = bEnableLive && !FParse::Param(FCommandLine::Get(),TEXT("NOLIVE"));
#endif
	return bEnableLive;
}
// @ATG_CHANGE : EDN

const FString& FOnlineSubsystemLive::GetTitleProductId() const
{
	static FString TitleProductId;
	if (TitleProductId.IsEmpty())
	{
		GConfig->GetString(TEXT("/Script/XboxOnePlatformEditor.XboxOneTargetSettings"), TEXT("ProductId"), TitleProductId, GEngineIni);
	}

	return TitleProductId;
}

FOnlineAsyncTaskManagerLive* FOnlineSubsystemLive::GetAsyncTaskManager()
{
	check(OnlineAsyncTaskThreadRunnable != nullptr);

	return OnlineAsyncTaskThreadRunnable.Get();
}

void FOnlineSubsystemLive::QueueAsyncTask(FOnlineAsyncTask* const AsyncTask, const bool bCanRunInParallel)
{
	check(OnlineAsyncTaskThreadRunnable);

	if ( bCanRunInParallel )
	{
		OnlineAsyncTaskThreadRunnable->AddToParallelTasks(AsyncTask);
	}
	else
	{
		OnlineAsyncTaskThreadRunnable->AddToInQueue(AsyncTask);
	}
}

void FOnlineSubsystemLive::QueueAsyncEvent(FOnlineAsyncEvent<FOnlineSubsystemLive>* const AsyncEvent)
{
	check(OnlineAsyncTaskThreadRunnable);

	OnlineAsyncTaskThreadRunnable->AddToOutQueue(AsyncEvent);
}

Platform::String^ FOnlineSubsystemLive::RemoveBracesFromGuidString( __in Platform::String^ guid )
{
	std::wstring strGuid = guid->ToString()->Data();

	if(strGuid.length() > 0 && strGuid[0] == L'{')
	{
		// Remove the {
		strGuid.erase(0, 1);
	}
	if(strGuid.length() > 0 && strGuid[strGuid.length() - 1] == L'}')
	{
		// Remove the }
		strGuid.erase(strGuid.end() - 1, strGuid.end());
	}

	return ref new Platform::String(strGuid.c_str());
}

Microsoft::Xbox::Services::XboxLiveContext^ FOnlineSubsystemLive::GetLiveContext(int32 LocalUserNum)
{
	if(!IdentityInterface.IsValid())
	{
		return nullptr;
	}

	auto LiveUser = IdentityInterface->GetUserForPlatformUserId(LocalUserNum);
	if(!LiveUser)
	{
		return nullptr;
	}

	return GetLiveContext(LiveUser);
}

Microsoft::Xbox::Services::XboxLiveContext^ FOnlineSubsystemLive::GetLiveContext(const FUniqueNetId& UserId)
{
	if (!IdentityInterface.IsValid())
	{
		return nullptr;
	}

	auto LiveUser = IdentityInterface->GetUserForUniqueNetId(static_cast<const FUniqueNetIdLive&>(UserId));
	if (!LiveUser)
	{
		return nullptr;
	}

	return GetLiveContext(LiveUser);
}

Microsoft::Xbox::Services::XboxLiveContext^ FOnlineSubsystemLive::GetLiveContext(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session)
{
	if (!Session || !Session->CurrentUser || !Session->CurrentUser->XboxUserId)
	{
		return nullptr;
	}

	Platform::String^ SessionMemberXuid = Session->CurrentUser->XboxUserId;

	return GetLiveContext(FUniqueNetIdLive(SessionMemberXuid->Data()));
}

Microsoft::Xbox::Services::XboxLiveContext^ FOnlineSubsystemLive::GetLiveContext(Windows::Xbox::System::User^ LiveUser)
{
	FScopeLock ScopeLock(&LiveContextsLock);

	// @ATG_CHANGE : BEGIN UWP LIVE support
	XboxLiveContext^* LiveContextPtr = CachedXboxLiveContexts.Find(LiveUser->XboxUserId->Data());
	// @ATG_CHANGE : END

	if (LiveContextPtr == nullptr || *LiveContextPtr == nullptr)
	{
		TWeakPtr<FOnlineSubsystemLive, ESPMode::ThreadSafe> WeakThis = AsShared();

		try
		{
			// @ATG_CHANGE : BEGIN UWP LIVE support
			XboxLiveContext^ LiveContext = ref new XboxLiveContext(XSAPIUserFromSystemUser(LiveUser));
			// @ATG_CHANGE : END UWP LIVE support
			LiveContext->RealTimeActivityService->Activate();

			// Register for friends updates
			LiveContext->SocialService->SocialRelationshipChanged += ref new Windows::Foundation::EventHandler<SocialRelationshipChangeEventArgs^>([WeakThis](Platform::Object^, SocialRelationshipChangeEventArgs^ EventArgs)
			{
				const FUniqueNetIdLive LiveNetId(EventArgs->CallerXboxUserId);
				UE_LOG_ONLINE(Verbose, TEXT("Received SocialRelationshipChange event for player %s"), *LiveNetId.ToString());

				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring SocialRelationshipChange as we went away"));
					return;
				}

				// Call on the game thread
				StrongThis->ExecuteNextTick([WeakThis, LiveNetId]()
				{
					FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
					if (!StrongThis.IsValid())
					{
						UE_LOG_ONLINE(Verbose, TEXT("Ignoring SocialRelationshipChange as we went away"));
						return;
					}

					const FOnlineIdentityLivePtr IdentityPtr = StrongThis->GetIdentityLive();
					if (!IdentityPtr.IsValid())
					{
						UE_LOG_ONLINE(Warning, TEXT("Received unhandleable SocialRelationshipChange event for player %s"), *LiveNetId.ToString());
						return;
					}

					const int32 LocalUserNum = IdentityPtr->GetPlatformUserIdFromUniqueNetId(LiveNetId);
					if (LocalUserNum == -1)
					{
						UE_LOG_ONLINE(Warning, TEXT("Received SocialRelationshipChange event for unknown player %s"), *LiveNetId.ToString());
						return;
					}

					// Requery our friends list
					const FOnlineFriendsLivePtr FriendsPtr = StrongThis->GetFriendsLive();
					if (FriendsPtr.IsValid())
					{
						if (!FriendsPtr->ReadFriendsList(LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default), FOnReadFriendsListComplete()))
						{
							UE_LOG_ONLINE(Warning, TEXT("Failed to requery friends list for player %s after SocialRelationshipChange"), *LiveNetId.ToString());
						}
					}
				});
			});

			// Register for presence updates
			LiveContext->PresenceService->DevicePresenceChanged += ref new Windows::Foundation::EventHandler<DevicePresenceChangeEventArgs^>([WeakThis](Platform::Object^, DevicePresenceChangeEventArgs^ EventArgs)
			{
				const FUniqueNetIdLive LiveNetId(EventArgs->XboxUserId);
				UE_LOG_ONLINE(Verbose, TEXT("Received DevicePresenceChanged Event Player:%s DeviceType:%s IsUserLoggedIn:%d"), *LiveNetId.ToString(), EventArgs->DeviceType.ToString()->Data(), EventArgs->IsUserLoggedOnDevice);

				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring DevicePresenceChanged as we went away"));
					return;
				}

				StrongThis->ExecuteNextTick([WeakThis, EventArgs]()
				{
					FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
					if (!StrongThis.IsValid())
					{
						UE_LOG_ONLINE(Verbose, TEXT("Ignoring DevicePresenceChanged as we went away"));
						return;
					}

					const FOnlinePresenceLivePtr PresencePtr = StrongThis->GetPresenceLive();
					if (PresencePtr.IsValid())
					{
						PresencePtr->OnPresenceDeviceChanged(EventArgs);
					}
				});
			});
			LiveContext->PresenceService->TitlePresenceChanged += ref new Windows::Foundation::EventHandler<TitlePresenceChangeEventArgs^>([WeakThis](Platform::Object^, TitlePresenceChangeEventArgs^ EventArgs)
			{
				const FUniqueNetIdLive LiveNetId(EventArgs->XboxUserId);
				UE_LOG_ONLINE(Verbose, TEXT("Received TitlePresenceChanged Event Player: %s TitleId: %u TitleState:%s"), *LiveNetId.ToString(), EventArgs->TitleId, EventArgs->TitleState.ToString()->Data());

				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring TitlePresenceChanged as we went away"));
					return;
				}

				StrongThis->ExecuteNextTick([WeakThis, EventArgs]()
				{
					FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
					if (!StrongThis.IsValid())
					{
						UE_LOG_ONLINE(Verbose, TEXT("Ignoring TitlePresenceChanged as we went away"));
						return;
					}

					const FOnlinePresenceLivePtr PresencePtr = StrongThis->GetPresenceLive();
					if (PresencePtr.IsValid())
					{
						PresencePtr->OnPresenceTitleChanged(EventArgs);
					}
				});
			});
			const FUniqueNetIdLive SourceNetId(LiveUser->XboxUserId);
			LiveContext->UserStatisticsService->StatisticChanged += ref new Windows::Foundation::EventHandler<StatisticChangeEventArgs^>([WeakThis, SourceNetId](Platform::Object^, StatisticChangeEventArgs^ EventArgs)
			{
				const FUniqueNetIdLive LiveNetId(EventArgs->XboxUserId);
				UE_LOG_ONLINE(Verbose, TEXT("Received StatisticChanged Event Player:%s StatName: %s NewValue: %s"), *LiveNetId.ToString(), EventArgs->LatestStatistic->StatisticName->Data(), EventArgs->LatestStatistic->Value->Data());

				FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
				if (!StrongThis.IsValid())
				{
					UE_LOG_ONLINE(Verbose, TEXT("Ignoring StatisticChanged as we went away"));
					return;
				}

				StrongThis->ExecuteNextTick([WeakThis, SourceNetId, EventArgs, LiveNetId]()
				{
					FOnlineSubsystemLivePtr StrongThis = WeakThis.Pin();
					if (!StrongThis.IsValid())
					{
						UE_LOG_ONLINE(Verbose, TEXT("Ignoring StatisticChanged as we went away"));
						return;
					}

					FOnlineSessionLivePtr SessionInt(StrongThis->GetSessionInterfaceLive());
					if (SessionInt.IsValid())
					{
						FString StatName(EventArgs->LatestStatistic->StatisticName->Data());
						if (StatName == SessionInt->SessionUpdateStatName)
						{
							FOnlinePresenceLivePtr PresencePtr(StrongThis->GetPresenceLive());
							if (PresencePtr.IsValid())
							{
								PresencePtr->OnSessionUpdatedStatChange(SourceNetId, LiveNetId);
							}
						}
					}
				});
			});

			// @ATG_CHANGE : BEGIN UWP LIVE support
			CachedXboxLiveContexts.Add(LiveUser->XboxUserId->Data(), LiveContext);
			// @ATG_CHANGE : END

			return LiveContext;
		}
		catch(Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("XboxLiveContext creation failed. Exception: %s."), Ex->ToString()->Data());
			return nullptr;
		}
	}
	else
	{
		return *LiveContextPtr;
	}
}

void FOnlineSubsystemLive::CheckPendingSessionInvite()
{
	// @ATG_CHANGE : BEGIN Adding XIM
	FOnlineSessionImplPtr SessionImpl = StaticCastSharedPtr<FOnlineSessionImpl>(SessionInterface);
	if (SessionImpl.IsValid() )
	{
		SessionImpl->CheckPendingSessionInvite();
	}
	// @ATG_CHANGE : END
}

void FOnlineSubsystemLive::RefreshLiveInfo(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
{
	FScopeLock ScopedRefreshLock(&RefreshLock);
	if (FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(SessionName))
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			LiveInfo->RefreshLiveInfo(LatestSession);
		}
	}

	if (MatchmakingInterfaceLive.IsValid())
	{
		FOnlineMatchTicketInfoPtr MatchTicket;
		MatchmakingInterfaceLive->GetMatchmakingTicket(SessionName, MatchTicket);
		if (MatchTicket.IsValid())
		{
			MatchTicket->RefreshLiveInfo(LatestSession);
		}
	}
}

void FOnlineSubsystemLive::SetLastDiffedSession(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
{
	// Access interfaces directly through this object
	// @ATG_CHANGE : BEGIN - Adding XIM
	FOnlineSessionLivePtr SessionInterfaceTyped = GetSessionInterfaceLive();
	if (SessionInterfaceTyped.IsValid())
	{
		FScopeLock ScopeLock(&SessionInterfaceTyped->SessionLock);
		if (FNamedOnlineSession* NamedSession = SessionInterfaceTyped->GetNamedSession(SessionName))
		{
			FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
			if (LiveInfo.IsValid())
			{
				LiveInfo->SetLastDiffedMultiplayerSession(LatestSession);
			}
		}
	}
	// @ATG_CHANGE : END

	if (MatchmakingInterfaceLive.IsValid())
	{
		FOnlineMatchTicketInfoPtr MatchTicket;
		MatchmakingInterfaceLive->GetMatchmakingTicket(SessionName, MatchTicket);
		if (MatchTicket.IsValid())
		{
			MatchTicket->SetLastDiffedSession(LatestSession);
		}
	}
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ FOnlineSubsystemLive::GetLastDiffedSession(const FName& SessionName)
{
	if (SessionInterface.IsValid())
	{
		if (FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(SessionName))
		{
			FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
			if (LiveInfo.IsValid())
			{
				return LiveInfo->GetLastDiffedMultiplayerSession();
			}
		}
	}

	if (MatchmakingInterfaceLive.IsValid())
	{
		FOnlineMatchTicketInfoPtr MatchTicket;
		MatchmakingInterfaceLive->GetMatchmakingTicket(SessionName, MatchTicket);
		if (MatchTicket.IsValid())
		{
			return MatchTicket->GetLastDiffedSession();
		}
	}

	return nullptr;
}

bool FOnlineSubsystemLive::AreSessionReferencesEqual(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ First, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ Second)
{
	return (FPlatformString::Stricmp(First->ServiceConfigurationId->Data(), Second->ServiceConfigurationId->Data()) == 0 &&
		FPlatformString::Stricmp(First->SessionTemplateName->Data(), Second->SessionTemplateName->Data()) == 0 &&
		FPlatformString::Stricmp(First->SessionName->Data(), Second->SessionName->Data()) == 0);
}

// @ATG_CHANGE : BEGIN - fix problems joining MP games after resume
void FOnlineSubsystemLive::HandleAppResume()
{
	// RTA automatically deactivates during suspension, but does not automatically wake again.
	// We need RTA active for MP operations, since it's required for monitoring service status,
	// so wake it manually.
	FScopeLock ScopeLock(&LiveContextsLock);
	for (TMap<FString, XboxLiveContext^>::TIterator It(CachedXboxLiveContexts); It; ++It)
	{
		It.Value()->RealTimeActivityService->Activate();
	}
}
// @ATG_CHANGE : END

FOnlineSessionXimPtr FOnlineSubsystemLive::GetSessionInterfaceXim()
{
// @ATG_CHANGE : BEGIN - Adding XIM
#if USE_XIM
	return StaticCastSharedPtr<FOnlineSessionXim>(SessionInterface);
#else
	return nullptr;
#endif
// @ATG_CHANGE : END
}

FOnlineSessionLivePtr FOnlineSubsystemLive::GetSessionInterfaceLive()
{
// @ATG_CHANGE : BEGIN - Adding XIM
#if USE_XIM
	return nullptr;
#else
	return StaticCastSharedPtr<FOnlineSessionLive>(SessionInterface);
#endif
// @ATG_CHANGE : END
}


void FOnlineSubsystemLive::RefreshNetworkConnectivityLevel()
{
	UE_LOG_ONLINE(Verbose, TEXT("RefreshNetworkConnectivityLevel"));
	Windows::Networking::Connectivity::NetworkConnectivityLevel NetworkConnectivityLevelOnStack = Windows::Networking::Connectivity::NetworkConnectivityLevel::None;

	ConnectionProfile^ InternetConnectionProfile = NetworkInformation::GetInternetConnectionProfile();

	if (InternetConnectionProfile != nullptr)
	{
		NetworkConnectivityLevelOnStack = InternetConnectionProfile->GetNetworkConnectivityLevel();
	}

	CreateAndDispatchAsyncEvent<FAsyncEventConnectionStatusChanged>(this, NetworkConnectivityLevelOnStack);
}

EOnlineEnvironment::Type FOnlineSubsystemLive::GetOnlineEnvironment() const
{
	// @ATG_CHANGE : BEGIN - UWP support
	FString SandboxId = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->Sandbox->Data();
	// @ATG_CHANGE : END

	if (SandboxId.Equals(TEXT("RETAIL")))
	{
		return EOnlineEnvironment::Production;
	}
	else if (SandboxId.StartsWith(TEXT("CERT")) || SandboxId.EndsWith(TEXT(".99")))
	{
		return EOnlineEnvironment::Certification;
	}
	else
	{
		return EOnlineEnvironment::Development;
	}
}

TSharedPtr<FPlatformInputInterface> GetInputInterface()
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

#if PLATFORM_XBOXONE
	FXboxOneApplication* PlatformApp = FXboxOneApplication::GetXboxOneApplication();
#elif PLATFORM_UWP
	FUWPApplication* PlatformApp = FUWPApplication::GetUWPApplication();
#endif
	if (PlatformApp == nullptr)
	{
		return nullptr;
	}
#if PLATFORM_XBOXONE
	return PlatformApp->GetXboxInputInterface();
#elif PLATFORM_UWP
	return PlatformApp->GetUWPInputInterface();
#endif
}
