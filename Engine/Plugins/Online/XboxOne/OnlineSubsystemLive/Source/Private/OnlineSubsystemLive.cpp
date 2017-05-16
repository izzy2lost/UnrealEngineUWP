// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSubsystemLive.h"
#include "ModuleManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

// @ATG_CHANGE : BEGIN Adding social features
#include "OnlineUserInterfaceLive.h"
// @ATG_CHANGE : END
#include "OnlineFriendsInterfaceLive.h"
// #include "OnlineUserCloudInterfaceLive.h"
#include "OnlineLeaderboardInterfaceLive.h"
#include "OnlineExternalUIInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAchievementsInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineVoiceInterfaceLive.h"
#include "SessionMessageRouter.h"
#include "OnlineMatchmakingInterfaceLive.h"
// @ATG_CHANGE : BEGIN - needed for pathing to cpprest dll
#include "IPluginManager.h"
// @ATG_CHANGE : END

// @ATG_CHANGE :  BEGIN 
typedef FOnlineSessionLive FOnlineSessionImpl;
typedef FOnlineSessionLivePtr FOnlineSessionImplPtr;
typedef FOnlineVoiceLive FOnlineVoiceImpl;
typedef FOnlineVoiceLivePtr FOnlineVoiceImplPtr;
// @ATG_CHANGE : END

using namespace Microsoft::Xbox::Services;
using namespace Windows::Networking::Connectivity;

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

		LiveSingleton = MakeShareable(new FOnlineSubsystemLive());
		if (LiveSingleton->IsEnabled())
		{
			if(!LiveSingleton->Init())
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
	return nullptr;
// @ATG_CHANGE : UWP Live Support - END
}

IOnlinePurchasePtr FOnlineSubsystemLive::GetPurchaseInterface() const
{
// @ATG_CHANGE : UWP Live Support - BEGIN
	return nullptr;
// @ATG_CHANGE : UWP Live Support - END
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
	// @ATG_CHANGE : BEGIN Adding social features
	return UserInterface;
	// @ATG_CHANGE : END
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

	// @ATG_CHANGE : BEGIN
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

	// @ATG_CHANGE : BEGIN Adding social features
	if (FriendInterface.IsValid())
	{
		FriendInterface->Tick(DeltaTime);
	}
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
			case NetworkConnectivityLevel::None: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: None") ); break;
			case NetworkConnectivityLevel::ConstrainedInternetAccess: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: ConstrainedInternetAccess") ); break;
			case NetworkConnectivityLevel::InternetAccess: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: InternetAccess") ); break;
			case NetworkConnectivityLevel::LocalAccess: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: LocalAccess") ); break;
// @ATG_CHANGE : UWP Live Support - BEGIN
#if PLATFORM_XBOXONE
			case NetworkConnectivityLevel::XboxLiveAccess: UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: XboxLiveAccess") ); break;
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

		UE_LOG_ONLINE(Warning, TEXT("NetworkStatusChangedEvent: OldConverted: %s, Converted: %s"), EOnlineServerConnectionStatus::ToString( Subsystem->ConvertedNetworkConnectivityLevel ), EOnlineServerConnectionStatus::ToString( ConvertedNetworkConnectivityLevelOnStack ) );

		if ( !Subsystem->bHasCalledNetworkStatusChangedAtLeastOnce || ConvertedNetworkConnectivityLevelOnStack != Subsystem->ConvertedNetworkConnectivityLevel )
		{
			Subsystem->bHasCalledNetworkStatusChangedAtLeastOnce = true;
			Subsystem->ConvertedNetworkConnectivityLevel = ConvertedNetworkConnectivityLevelOnStack;

			Subsystem->TriggerOnConnectionStatusChangedDelegates(EOnlineServerConnectionStatus::Normal, Subsystem->ConvertedNetworkConnectivityLevel);
		}
	}
};

bool FOnlineSubsystemLive::Init()
{
	const bool bLiveInit = true;
	
	if (bLiveInit)
	{
		// @todo - still need to define Live socket subsystem
		//CreateLiveSocketSubsystem();
// @ATG_CHANGE : BEGIN UWP LIVE support
		TSharedPtr<IPlugin> LivePlugin = IPluginManager::Get().FindPlugin(TEXT("OnlineSubsystemLive"));
		if (!LivePlugin.IsValid())
		{
			UE_LOG(LogOnline, Error, TEXT("Failed to locate FPlugin instance describing Live plugin."), OnlineAsyncTaskThread->GetThreadID());
			return false;
		}

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
// @ATG_CHANGE : END

		// Create the online async task thread
		OnlineAsyncTaskThreadRunnable = new FOnlineAsyncTaskManagerLive(this);
		check(OnlineAsyncTaskThreadRunnable);
		OnlineAsyncTaskThread = FRunnableThread::Create(OnlineAsyncTaskThreadRunnable, *FString::Printf(TEXT("OnlineAsyncTaskThread %s"), *InstanceName.ToString()));
		check(OnlineAsyncTaskThread);
		UE_LOG(LogOnline, Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID() );

		// @ATG_CHANGE : BEGIN 
		ApplicationConfig = XboxLiveAppConfiguration::SingletonInstance;
		// @ATG_CHANGE : END

		SessionMessageRouterInterface = MakeShareable(new FSessionMessageRouter(this));
		MatchmakingInterfaceLive = MakeShareable(new FOnlineMatchmakingInterfaceLive(this));

		IdentityInterface = MakeShareable(new FOnlineIdentityLive(this));

 		SessionInterface = MakeShareable(new FOnlineSessionLive(this));

 		// @ATG_CHANGE : BEGIN Adding social features
		FriendInterface = MakeShareable(new FOnlineFriendsLive(this));
		// @ATG_CHANGE : END		
// 		UserCloudInterface = MakeShareable(new FOnlineUserCloudLive(this));
		LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsLive(this));

// @ATG_CHANGE : BEGIN 
#if WITH_GAME_CHAT
		FOnlineVoiceImplPtr VoiceImpl = MakeShareable(new FOnlineVoiceLive(this));

		if (VoiceImpl->Init())
		{
			VoiceInterface = VoiceImpl;
		}
#endif
// @ATG_CHANGE : END

 		ExternalUIInterface = MakeShareable(new FOnlineExternalUILive(this));
		EventsInterface = MakeShareable(new FOnlineEventsLive(this));
		AchievementInterface = MakeShareable(new FOnlineAchievementsLive(this));
		PresenceInterface = MakeShareable(new FOnlinePresenceLive(this));
 		// @ATG_CHANGE : BEGIN Adding social features
		UserInterface = MakeShareable(new FOnlineUserInterfaceLive(this));
		// @ATG_CHANGE : END		
		
		bHasCalledNetworkStatusChangedAtLeastOnce = false;

		Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged += ref new Windows::Networking::Connectivity::NetworkStatusChangedEventHandler( 
			[this] (Platform::Object^)
		{
			Windows::Networking::Connectivity::NetworkConnectivityLevel NetworkConnectivityLevelOnStack = Windows::Networking::Connectivity::NetworkConnectivityLevel::None;

			ConnectionProfile^ InternetConnectionProfile = NetworkInformation::GetInternetConnectionProfile();

			if ( InternetConnectionProfile != nullptr )
			{
				NetworkConnectivityLevelOnStack = InternetConnectionProfile->GetNetworkConnectivityLevel();
			}

			auto NewEvent = new FAsyncEventConnectionStatusChanged(this, NetworkConnectivityLevelOnStack);
			GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		});

		// Clear cached XboxLiveContext when user is removed
		UserRemovedToken = Windows::Xbox::System::User::UserRemoved += ref new Windows::Foundation::EventHandler<Windows::Xbox::System::UserRemovedEventArgs^>(
		[this] (Platform::Object^, Windows::Xbox::System::UserRemovedEventArgs^ Args)
		{
			FScopeLock ScopeLock(&LiveContextsLock);

			// @ATG_CHANGE : UWP Live Support - BEGIN
			XboxLiveContext^* RemoveContext = CachedXboxLiveContexts.Find(Args->User->XboxUserId->Data());
			(*RemoveContext)->RealTimeActivityService->Deactivate();
			CachedXboxLiveContexts.Remove(Args->User->XboxUserId->Data());
			// @ATG_CHANGE : UWP Live Support - END
		});
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

	if (OnlineAsyncTaskThread)
	{
		// Destroy the online async task thread
		delete OnlineAsyncTaskThread;
		OnlineAsyncTaskThread = nullptr;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		delete OnlineAsyncTaskThreadRunnable;
		OnlineAsyncTaskThreadRunnable = nullptr;
	}

	#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	// Destruct the interfaces (in opposite order they were created)
	DESTRUCT_INTERFACE(PresenceInterface);
	DESTRUCT_INTERFACE(AchievementInterface);
	DESTRUCT_INTERFACE(EventsInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(FriendInterface);
	DESTRUCT_INTERFACE(SessionInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(MatchmakingInterfaceLive);
	DESTRUCT_INTERFACE(SessionMessageRouterInterface);
	// @ATG_CHANGE : BEGIN Adding social features
	DESTRUCT_INTERFACE(FriendInterface);
	DESTRUCT_INTERFACE(UserInterface);
	// @ATG_CHANGE : END


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
	// @ATG_CHANGE : BEGIN Report scid as appid for use by other components
	if (ApplicationConfig != nullptr)
	{
		return ApplicationConfig->ServiceConfigurationId->Data();
	}
	// @ATG_CHANGE : END
	return TEXT("");
}

bool FOnlineSubsystemLive::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}

	return false;
}

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

// @ATG_CHANGE : BEGIN Adding social features
XboxLiveContext^ FOnlineSubsystemLive::GetDefaultLiveContext() const
{
	FScopeLock ScopeLock(&LiveContextsLock);

	TMap<FString, Microsoft::Xbox::Services::XboxLiveContext^>::TConstIterator It(CachedXboxLiveContexts);
	return It ? It.Value() : nullptr;
}
// @ATG_CHANGE : END

Microsoft::Xbox::Services::XboxLiveContext^ FOnlineSubsystemLive::GetLiveContext(int32 LocalUserNum)
{
	if(!IdentityInterface.IsValid())
	{
		return nullptr;
	}

	auto LiveUser = IdentityInterface->GetUserForControllerIndex(LocalUserNum);
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

	if(LiveContextPtr == nullptr || *LiveContextPtr == nullptr)
	{
		try
		{
			// @ATG_CHANGE : BEGIN UWP LIVE support
			auto LiveContext = ref new XboxLiveContext(XSAPIUserFromSystemUser(LiveUser));
			LiveContext->RealTimeActivityService->Activate();
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
		auto LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			LiveInfo->RefreshLiveInfo(LatestSession);
		}
	}

	FOnlineMatchTicketInfoPtr MatchTicket;
	MatchmakingInterfaceLive->GetMatchmakingTicket(SessionName, MatchTicket);
	if (MatchTicket.IsValid())
	{
		MatchTicket->RefreshLiveInfo(LatestSession);
	}
}

void FOnlineSubsystemLive::SetLastDiffedSession(const FName& SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
{
	// Access interfaces directly through this object
	if (SessionInterface.IsValid())
	{
		if (FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(SessionName))
		{
			auto LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
			if (LiveInfo.IsValid())
			{
				LiveInfo->SetLastDiffedMultiplayerSession(LatestSession);
			}
		}
	}

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
			auto LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
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


// @ATG_CHANGE : BEGIN 
FOnlineSessionLivePtr FOnlineSubsystemLive::GetSessionInterfaceLive()
{
	return StaticCastSharedPtr<FOnlineSessionLive>(SessionInterface);
}
// @ATG_CHANGE :  END


