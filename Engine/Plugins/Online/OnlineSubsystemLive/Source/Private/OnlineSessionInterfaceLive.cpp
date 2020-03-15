// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineEventsInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineMatchmakingInterfaceLive.h"
#include "OnlinePresenceInterfaceLive.h"
#include "Interfaces/VoiceInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// @ATG_CHANGE : UWP Live Support - BEGIN
#if PLATFORM_XBOXONE
#include "XboxOnePostApi.h"
#include "XboxOne/XboxOneMisc.h"
#endif
// @ATG_CHANGE : UWP Live Support - END
#include "SocketSubsystem.h"
#include "IPAddress.h"

#include "AsyncTasks/OnlineAsyncTaskLiveCreateSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveFindSessions.h"
#include "AsyncTasks/OnlineAsyncTaskLiveJoinSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveCreateMatchSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveSubmitMatchTicket.h"
#include "AsyncTasks/OnlineAsyncTaskLiveGameSessionReady.h"
#include "AsyncTasks/OnlineAsyncTaskLiveCancelMatchmaking.h"
#include "AsyncTasks/OnlineAsyncTaskLiveDestroySession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveRegisterLocalUser.h"
#include "AsyncTasks/OnlineAsyncTaskLiveUnregisterLocalUser.h"
#include "AsyncTasks/OnlineAsyncTaskLiveUpdateSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveUpdateSessionMember.h"
#include "AsyncTasks/OnlineAsyncTaskLiveMeasureAndUploadQos.h"
#include "AsyncTasks/OnlineAsyncTaskLiveSendSessionInviteToFriends.h"
#include "AsyncTasks/OnlineAsyncTaskLiveSetSessionActivity.h"
#include "AsyncTasks/OnlineAsyncTaskLiveFindSessionById.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Platform::Collections;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;
using namespace Microsoft::Xbox::Services;
using Microsoft::Xbox::Services::XboxLiveContext;
using namespace Microsoft::Xbox::Services::Matchmaking;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage;
// @ATG_CHANGE: BEGIN
using Microsoft::Xbox::Services::Multiplayer::MultiplayerGetSessionsRequest;
// @ATG_CHANGE: END
using Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSession;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionProperties;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode;
using Microsoft::Xbox::Services::Multiplayer::WriteSessionResult;
using Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus;
using Microsoft::Xbox::Services::Social::XboxUserProfile;

namespace
{
	/** The maximum number of Live sessions to return when searching for orphaned sessions. */
	const int32 MAX_ORPHANED_SESSIONS_RESULTS = 100;
	const int32 MAX_RETRIES = 20;

	/** Gets the current time as a DateTime object */
	DateTime GetCurrentTime()
	{
		ULARGE_INTEGER uInt;
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		uInt.LowPart = ft.dwLowDateTime;
		uInt.HighPart = ft.dwHighDateTime;

		Windows::Foundation::DateTime time;
		time.UniversalTime = uInt.QuadPart;
		return time;
	}

	const TCHAR* GetSessionMemberStatusString(MultiplayerSessionMemberStatus Status)
	{

		switch (Status)
		{
		case MultiplayerSessionMemberStatus::Active:
			return TEXT("Active");
		case MultiplayerSessionMemberStatus::Inactive:
			return TEXT("Inactive");
		case MultiplayerSessionMemberStatus::Ready:
			return TEXT("Ready");
		case MultiplayerSessionMemberStatus::Reserved:
			return TEXT("Reserved");
		}

		return TEXT("Unknown");
	}

	void DebugLogLiveSession(MultiplayerSession^ Session)
	{
		if (Session == nullptr)
		{
			UE_LOG_ONLINE(Verbose, TEXT("DebugLogLiveSession: Session is null."));
			return;
		}

		UE_LOG_ONLINE(Verbose, TEXT("DebugLogLiveSession:"));
		UE_LOG_ONLINE(Verbose, TEXT("  MaxMembersInSession: %d"), Session->SessionConstants->MaxMembersInSession);
		UE_LOG_ONLINE(Verbose, TEXT("  Members->Size: %d. Members:"), Session->Members->Size);

		for (MultiplayerSessionMember^ Member : Session->Members)
		{
			UE_LOG_ONLINE(Verbose, TEXT("    Gamertag: %s, Live ID: %s, status: %s"),
				Member->Gamertag->Data(), Member->XboxUserId->Data(), GetSessionMemberStatusString(Member->Status));
		}

		Platform::String^ PlatformPropertiesJson = Session->SessionProperties->SessionCustomPropertiesJson;
		FString PropertiesJson(PlatformPropertiesJson->Data());
		TSharedPtr< FJsonObject > JsonObject;
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(PropertiesJson);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			UE_LOG_ONLINE(Verbose, TEXT("  Custom Properties Size: %d. Fields:"), JsonObject->Values.Num());
			for (const TMap<FString, TSharedPtr<FJsonValue>>::ElementType& Pair : JsonObject->Values)
			{
				FString OutString;
				if (Pair.Value->TryGetString(OutString))
				{
					UE_LOG_ONLINE(Verbose, TEXT("    %s: %s"), *Pair.Key, *OutString);
				}
				else
				{
					UE_LOG_ONLINE(Verbose, TEXT("    %s: [Non-String Value]"), *Pair.Key, *OutString);
				}
			}
		}
		else
		{
			UE_LOG_ONLINE(Verbose, TEXT("  No Custom Properties Set"));
		}
	}
}

FOnlineSessionLive::FOnlineSessionLive(class FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
	, PeerTemplate(nullptr)
	, bIsDestroyingSessions(false)
	, bOnlyHostUpdateSession(true)
{
	Initialize();
}

FOnlineSessionLive::~FOnlineSessionLive()
{
	if (PeerTemplate)
	{
		PeerTemplate->AssociationIncoming -= TokenSecureAssociationIncoming;
	}

	try
	{
		Windows::Xbox::System::User::SignInCompleted -= SignInCompletedToken;
	}
	catch(Platform::Exception^)
	{
		UE_LOG_ONLINE(Warning, TEXT("User Exception during shutdown"));
	}

	// Replaced old party events with session subscriptions and activation handler for invites
	CoreApplication::GetCurrentView()->Activated -= ActivatedToken;
	LiveSubsystem->GetSessionMessageRouter()->ClearOnSubscriptionLostDelegate_Handle(OnSubscriptionLostDelegateHandle);

	// @ATG_CHANGE : BEGIN - clean up registration for user added event (used for invites with late sign-in)
	User::UserAdded -= UserAddedToken;
	// @ATG_CHANGE : END
}

void FOnlineSessionLive::Initialize()
{
	FString TemplateName;

	// Load our session-updating stats
	GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("SessionUpdateStatName"), SessionUpdateStatName, GEngineIni);
	GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("SessionUpdateEventName"), SessionUpdateEventName, GEngineIni);

	// Load session updating permissions setting
	GConfig->GetBool(TEXT("OnlineSubsystemLive"), TEXT("bOnlyHostUpdateSession"), bOnlyHostUpdateSession, GEngineIni);
	

	// Look up the secure device association template name in the engine ini settings.
	if (GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("SecureDeviceAssociationTemplateName"), TemplateName, GEngineIni))
	{
		try
		{
			PeerTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(ref new Platform::String(*TemplateName));

			// Listen for Secure Association incoming
			auto AssociationIncomingEvent = ref new TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>(
				[] (Platform::Object^, SecureDeviceAssociationIncomingEventArgs^ EventArgs)
			{
				if (EventArgs->Association)
				{
					UE_LOG_ONLINE(Log, TEXT("Received association, state is %d."), (int)EventArgs->Association->State);

					auto StateChangedEvent = ref new TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>(&LogAssociationStateChange);
					EventArgs->Association->StateChanged += StateChangedEvent;
				}
			});

			TokenSecureAssociationIncoming = PeerTemplate->AssociationIncoming += AssociationIncomingEvent;
		}
		catch(Platform::COMException^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("Couldn't find secure device association template named %s. Check the app manifest."), *TemplateName);
		}
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("No SecureDeviceAssociationTemplateName specified in the engine ini file."));
	}

	// Clean up orphaned sessions.
	// If an online game is ended abruptly due to a network disconnect, crash, or just stopping the debugger,
	// we may end up with stale sessions in the MPSD that have no corresponding game running.
	// According to https://forums.xboxlive.com/AnswerPage.aspx?qid=216d3346-19ba-4bb0-af9e-0b2001fb57fe&tgt=1,
	// games should detect sessions with only the local user in them at startup and call Leave() on those sessions.
	// Also enable multiplayer subscriptions to get session updates.

	// Grab the user list, the Identity interface caches this at startup already so we can just use their list instead of
	// spending extra time making another cross-VM call.
	// Note that this forces a dependency on the identity interface being initialized before the session interface.
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ Users = Identity->GetCachedUsers();
	for (User^ CurrentUser : Users)
	{
		CleanUpOrphanedSessions(CurrentUser);
	}

	using namespace Windows::Xbox::System;
	using namespace Windows::Foundation;
	EventHandler<SignInCompletedEventArgs^>^ SignInCompletedEvent = ref new EventHandler<SignInCompletedEventArgs^>(
		[this] (Platform::Object^, SignInCompletedEventArgs^ EventArgs)
	{
		CleanUpOrphanedSessions(EventArgs->User);
	});
	SignInCompletedToken = Windows::Xbox::System::User::SignInCompleted += SignInCompletedEvent;


	OnSubscriptionLostDestroyCompleteDelegate = FOnEndSessionCompleteDelegate::CreateRaw(this, &FOnlineSessionLive::OnSubscriptionLostDestroyComplete);

	// Sign up for Activated events, which we use to detect accepted invites while the game
	// is running.
	ActivatedToken = CoreApplication::GetCurrentView()->Activated += ref new TypedEventHandler< CoreApplicationView^, IActivatedEventArgs^ >(
		[this] (CoreApplicationView^, IActivatedEventArgs^ EventArgs)
	{
		OnActivated(EventArgs);
	});

	// Check for a saved invite protocol URI, indicating that local player accepted an invite.
	CheckPendingSessionInvite();

	// @ATG_CHANGE : BEGIN - on UWP users might not arrive until later.  Re-check invites at that point.
	// Harmless on Xbox since we've already consumed any saved invite.
	UserAddedToken = User::UserAdded += ref new EventHandler<UserAddedEventArgs^>(
		[this](Platform::Object^, UserAddedEventArgs^ Args)
	{
		CheckPendingSessionInvite();
	});
	// @ATG_CHANGE : END

	OnSubscriptionLostDelegateHandle = LiveSubsystem->GetSessionMessageRouter()->OnSubscriptionLostDelegates.AddRaw(this, &FOnlineSessionLive::OnMultiplayerSubscriptionsLost);

	// Initialize session state after create/join
	OnSessionNeedsInitialStateDelegate = FOnSessionNeedsInitialStateDelegate::CreateRaw(this, &FOnlineSessionLive::OnSessionNeedsInitialState);
	LiveSubsystem->GetSessionMessageRouter()->AddOnSessionNeedsInitialStateDelegate_Handle(OnSessionNeedsInitialStateDelegate);

	OnSessionChangedDelegate = FOnSessionChangedDelegate::CreateRaw(this, &FOnlineSessionLive::OnSessionChanged);
}

void FOnlineSessionLive::CleanUpOrphanedSessions(Windows::Xbox::System::User^ User) const
{
	try
	{
		XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(User);
		if (!LiveContext)
		{
			UE_LOG_ONLINE(Warning, TEXT("Unable to retrieve LiveContext for User %s"), (User && User->XboxUserId) ? User->XboxUserId->Data() : L"Unknown User");
			return;
		}

		// @ATG_CHANGE : BEGIN UWP LIVE support
		auto SessionsRequest = ref new MultiplayerGetSessionsRequest(LiveContext->AppConfig->ServiceConfigurationId, MAX_ORPHANED_SESSIONS_RESULTS);
		SessionsRequest->IncludePrivateSessions = true;
		SessionsRequest->IncludeReservations = true;
		SessionsRequest->IncludeInactiveSessions = true;
		SessionsRequest->XboxUserIdFilter = User->XboxUserId;
		SessionsRequest->VisibilityFilter = MultiplayerSessionVisibility::Any;

		auto AsyncGetOp = LiveContext->MultiplayerService->GetSessionsAsync(SessionsRequest);
		// @ATG_CHANGE : END

		Concurrency::create_task(AsyncGetOp).then([User, LiveContext](Concurrency::task<IVectorView<MultiplayerSessionStates^>^> Task)
		{
			try
			{
				auto Results = Task.get();

				UE_LOG_ONLINE(Log, TEXT("Found %d potentially orphaned sessions."), Results->Size);

				for (auto SessionState : Results)
				{
					UE_LOG_ONLINE(Log, TEXT("Potentially orphaned session:"));
					UE_LOG_ONLINE(Log, TEXT("  Template name: %s"), SessionState->SessionReference->SessionTemplateName->Data());
					UE_LOG_ONLINE(Log, TEXT("  Id: %s"), SessionState->SessionReference->SessionName->Data());
					UE_LOG_ONLINE(Log, TEXT("  State: %s"), SessionState->Status.ToString()->Data());

					auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionState->SessionReference);

					Concurrency::create_task(GetSessionOp).then([User, LiveContext, SessionState](Concurrency::task<MultiplayerSession^> SessionTask)
					{
						try
						{
							auto Session = SessionTask.get();

							if (!Session)
							{
								return;
							}

							for (auto Member : Session->Members)
							{
								if (Member->XboxUserId != User->XboxUserId)
									continue;

								// If we're just coming into the title, we can't be active for a session in this title,
								// we can however be inactive from a PLM change, and we should drop our membership in said session as a rejoin doesn't make sense
								// We don't check reserved or ready, as both signify an invite
								if (Member->Status == MultiplayerSessionMemberStatus::Active || Member->Status == MultiplayerSessionMemberStatus::Inactive)
								{
									Session->Leave();

									auto WriteOp = LiveContext->MultiplayerService->WriteSessionAsync(Session, MultiplayerSessionWriteMode::UpdateExisting);
									Concurrency::create_task(WriteOp).then([Session](Concurrency::task<MultiplayerSession^> WriteTask)
									{
										try
										{
											WriteTask.get();
										}
										catch(Platform::Exception^ Ex)
										{
											UE_LOG_ONLINE(Warning, TEXT("Failed to write leave to orphaned session. Id: %s"), Session->SessionReference->SessionName->Data());
										}
									});
									return;
								}
							}
						}
						catch(Platform::Exception^ Ex)
						{
							UE_LOG_ONLINE(Warning, TEXT("Failed to get session from session reference. Id: %s"), SessionState->SessionReference->SessionName->Data());
						}
					});
				}
			}
			catch(Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to get sessions for orphaned session cleanup."));
			}
		});
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not clean up orphaned sessions for local user."));
	}
}

bool FOnlineSessionLive::AreInvitesAndJoinViaPresenceAllowed(const FOnlineSessionSettings& OnlineSessionSettings)
{
	return OnlineSessionSettings.bAllowInvites || OnlineSessionSettings.bAllowJoinViaPresence || OnlineSessionSettings.bAllowJoinViaPresenceFriendsOnly;
}

bool FOnlineSessionLive::CreateSession(int32 HostingPlayerControllerIndex, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(HostingPlayerControllerIndex);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Log, TEXT("Couldn't find unique id for HostingPlayerNum %d"), HostingPlayerControllerIndex);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	return CreateSession(*UniqueId, SessionName, NewSessionSettings);
}

bool FOnlineSessionLive::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	// Check for an existing session
	FNamedOnlineSession* Session = GetNamedSession(SessionName);

	if (Session != nullptr)
	{
		UE_LOG(LogOnline, Warning, TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Create a new session and deep copy the game settings
	Session = AddNamedSession(SessionName, NewSessionSettings);
	check(Session != nullptr);
	Session->SessionState = EOnlineSessionState::Creating;
	Session->bHosting = true;
	Session->LocalOwnerId = MakeShared<FUniqueNetIdLive>(HostingPlayerId);

	FString TemplateNameString;
	const FOnlineSessionSetting* TemplateNameSetting = NewSessionSettings.Settings.Find(SETTING_SESSION_TEMPLATE_NAME);
	if (TemplateNameSetting)
	{
		TemplateNameSetting->Data.GetValue(TemplateNameString);
	}

	Windows::Xbox::System::User^ CreatingUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(FUniqueNetIdLive(HostingPlayerId));

	FString Keyword;
	NewSessionSettings.Get(SEARCH_KEYWORDS, Keyword);

	try
	{
		auto writeSessionOp = CreateSessionOperation(HostingPlayerId, NewSessionSettings, Keyword, TemplateNameString);
		if (!writeSessionOp)
		{
			UE_LOG(LogOnline, Log, TEXT("Failed to create async create session operation"));
			RemoveNamedSession(SessionName);
			TriggerOnCreateSessionCompleteDelegates(SessionName, false);
			return false;
		}

		bool bCreateActivity = AreInvitesAndJoinViaPresenceAllowed(NewSessionSettings);
		// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
		Concurrency::create_task(writeSessionOp).then([this,CreatingUser,SessionName,bCreateActivity,NewSessionSettings](Concurrency::task<MultiplayerSession^> CreateTask)
		// @ATG_CHANGE : END
		{
			MultiplayerSession^ LiveSession = nullptr;
			try
			{
				LiveSession = CreateTask.get(); // if t.get() didn't throw, it succeeded

				LiveSubsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(OnSessionChangedDelegate, LiveSession->SessionReference);
				// Now that the session is created, we can get the device token and set this console as the host.
				MultiplayerSessionMember^ HostMember = nullptr;
				for (MultiplayerSessionMember^ Member : LiveSession->Members)
				{
					if (Member->XboxUserId == CreatingUser->XboxUserId)
					{
						HostMember = Member;
					}
				}

				TSharedRef<const FUniqueNetId> CreatingUserUniqueId(MakeShared<FUniqueNetIdLive>(CreatingUser->XboxUserId));

				XboxLiveContext^ Context = LiveSubsystem->GetLiveContext(CreatingUser);
				check(Context != nullptr);

				if (HostMember == nullptr)
				{
					UE_LOG_ONLINE(Warning, TEXT("Could not find creator in session members. Not setting host."));

					if (bCreateActivity)
					{
						// Set activity now if we're done updating the session
						LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(LiveSubsystem, Context, LiveSession->SessionReference);
					}

					LiveSubsystem->CreateAndDispatchAsyncEvent<FOnlineAsyncTaskLiveCreateSession>(
						this,
						CreatingUserUniqueId,
						SessionName,
						LiveSession);
					return;
				}

				// Simple host selection - the user that creates the session is the host.
				LiveSession->SetHostDeviceToken(HostMember->DeviceToken);

				// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
				// Now that the session is created its constants should be fully initialized - as such
				// it's now safe to rely on constants to help determine joinability.
				WriteSessionPrivacySettingsToLiveJson(NewSessionSettings, LiveSession);
				// @ATG_CHANGE : END
				
				// This will be the session used for invites/join in progress if supported.

				auto WriteSessionOp = Context->MultiplayerService->WriteSessionAsync(
					LiveSession,
					MultiplayerSessionWriteMode::UpdateExisting);

				Concurrency::create_task(WriteSessionOp).then([this, CreatingUserUniqueId, SessionName, LiveSession, Context, bCreateActivity](Concurrency::task<MultiplayerSession^> WriteTask)
				{
					try
					{
						auto NewSession = WriteTask.get(); // if t.get() didn't throw, it succeeded

						if (bCreateActivity)
						{
							LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(LiveSubsystem, Context, NewSession->SessionReference);
						}

						LiveSubsystem->CreateAndDispatchAsyncEvent<FOnlineAsyncTaskLiveCreateSession>(
							this,
							CreatingUserUniqueId,
							SessionName,
							NewSession);
					}
					catch (Platform::COMException^ Ex)
					{
						UE_LOG_ONLINE(Warning, TEXT("WriteSessionAsync failed attempting to write host device token."));

						LiveSubsystem->ExecuteNextTick([this, SessionName, LiveSession]()
						{
							if (LiveSession)
							{
								LiveSession->Leave();
							}
							RemoveNamedSession(SessionName);
							TriggerOnCreateSessionCompleteDelegates(SessionName, false);
						});
					}
				});
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG(LogOnline, Log, TEXT("Create Session Task failed with 0x%0.8X"), Ex->HResult);

				LiveSubsystem->ExecuteNextTick([this, SessionName, LiveSession]()
				{
					if (LiveSession)
					{
						LiveSession->Leave();
					}
					RemoveNamedSession(SessionName);
					TriggerOnCreateSessionCompleteDelegates(SessionName, false);
				});
			}
		});
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG(LogOnline, Log, TEXT("Create Session Task failed with 0x%0.8X"), Ex->HResult);
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	return true;
}

bool FOnlineSessionLive::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionLive::FindSessions(int32 SearchingPlayerControllerIndex, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(SearchingPlayerControllerIndex);
	if (!UniqueId.IsValid())
	{
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	return FindSessions(*UniqueId, SearchSettings);
}

bool FOnlineSessionLive::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	// Don't start another search while one is in progress
	if (!CurrentSessionSearch.IsValid() && SearchSettings->SearchState != EOnlineAsyncTaskState::InProgress)
	{
		// Free up previous results
		SearchSettings->SearchResults.Empty();

		FString		InGameType;
		FString		InUser;
		FString		InKeywords;
		int32		MaxResult = 0;
		int32		ContractVersionFilter = 0;
		bool		IncludePrivateSessions = false;
		bool		IncludeReservations = false;
		bool		IncludeInactiveSessions = false;
		int32		MultiplayerVisibility = int32(MultiplayerSessionVisibility::Open);

		SearchSettings->QuerySettings.Get(SETTING_GAMEMODE, InGameType);
		SearchSettings->QuerySettings.Get(SEARCH_USER, InUser);
		SearchSettings->QuerySettings.Get(SEARCH_KEYWORDS, InKeywords);
		SearchSettings->QuerySettings.Get(SETTING_MAX_RESULT, MaxResult);
		if (MaxResult == 0)
		{
			// Default value is 100, this is arbitrary
			MaxResult = 100;
		}
		SearchSettings->QuerySettings.Get(SETTING_CONTRACT_VERSION_FILTER,	ContractVersionFilter);
		SearchSettings->QuerySettings.Get(SETTING_FIND_PRIVATE_SESSIONS,	IncludePrivateSessions);
		SearchSettings->QuerySettings.Get(SETTING_FIND_RESERVED_SESSIONS,	IncludeReservations);
		SearchSettings->QuerySettings.Get(SETTING_FIND_INACTIVE_SESSIONS,	IncludeInactiveSessions);
		SearchSettings->QuerySettings.Get(SETTING_MULTIPLAYER_VISIBILITY,	MultiplayerVisibility);

		Platform::String^ sessionTemplateNameFilter	= ref new Platform::String(*InGameType);
		Platform::String^ xboxUserIdFilter			= ref new Platform::String(*InUser);
		Platform::String^ keywordFilter				= ref new Platform::String(*InKeywords);

		try
		{
			XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(SearchingPlayerId);
			if (LiveContext == nullptr)
			{
				SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
				TriggerOnFindSessionsCompleteDelegates(false);
				return false;
			}

			// @ATG_CHANGE : BEGIN UWP LIVE support
			auto SessionsRequest = ref new MultiplayerGetSessionsRequest(LiveContext->AppConfig->ServiceConfigurationId, MaxResult);
			SessionsRequest->IncludePrivateSessions = IncludePrivateSessions;
			SessionsRequest->IncludeReservations = IncludeReservations;
			SessionsRequest->IncludeInactiveSessions = IncludeInactiveSessions;
			SessionsRequest->XboxUserIdFilter = xboxUserIdFilter;
			SessionsRequest->SessionTemplateNameFilter = sessionTemplateNameFilter;
			SessionsRequest->KeywordFilter = keywordFilter;
			SessionsRequest->VisibilityFilter = MultiplayerSessionVisibility(MultiplayerVisibility);

			IAsyncOperation<IVectorView<MultiplayerSessionStates^>^>^  SearchOp;
			SearchOp = LiveContext->MultiplayerService->GetSessionsAsync(SessionsRequest);
			// @ATG_CHANGE : END

			Concurrency::create_task(SearchOp)
				.then([this,SearchSettings,LiveContext] (Concurrency::task<IVectorView<MultiplayerSessionStates^>^> SearchTask)
			{
				try
				{
					auto SearchResults = SearchTask.get(); // if t.get() didn't throw, it succeeded
					ExpectedResults = SearchResults->Size;

					if (ExpectedResults == 0)
					{
						// Finish on the Game thread
						LiveSubsystem->ExecuteNextTick([this, SearchSettings]()
						{
							SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
							CurrentSessionSearch = nullptr;
							TriggerOnFindSessionsCompleteDelegates(true);
						});
						return;
					}

					for (auto SessionState : SearchResults)
					{
						if (SessionState->SessionReference != nullptr)
						{
							auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionState->SessionReference);
							Concurrency::create_task(GetSessionOp)
								.then([this, SearchSettings, LiveContext](Concurrency::task<MultiplayerSession^> t)
							{
								// Lock for the entirety of this scope to protect safe access to SearchSettings' SearchResults and ExpectedResults
								FScopeLock Lock(&SessionResultLock);
								try
								{
									MultiplayerSession^ SearchResult = t.get();
									if (SearchResult)
									{
										Platform::String^ HostDisplayName = nullptr;
										auto HostMember = GetLiveSessionHost(SearchResult);
										if (HostMember)
										{
											// XR-46 permits the use of Gamertag here.
											HostDisplayName = HostMember->Gamertag;
										}
										else
										{
											HostDisplayName = ref new Platform::String(TEXT("Unknown Host"));
										}

										auto NewSearchResult = CreateSearchResultFromSession(SearchResult, HostDisplayName, LiveContext);
										SearchSettings->SearchResults.Add(NewSearchResult);
									}
								}
								catch (Platform::Exception^ ex)
								{
									UE_LOG(LogOnline, Log,TEXT("A MultiplayerService::GetCurrentSessionAsync call failed with 0x%0.8X"), ex->HResult);
								}

								--ExpectedResults;

								if (ExpectedResults == 0)
								{
									PingResultsAndTriggerDelegates(SearchSettings);
								}
							});
						}
						else
						{
							//Lock for the entirety of the fail case so ExpectedResults is valid for it.
							FScopeLock Lock(&SessionResultLock);
							--ExpectedResults;

							if (ExpectedResults == 0)
							{
								PingResultsAndTriggerDelegates(SearchSettings);
							}
						}
					}
				}
				catch (Platform::Exception^ ex)
				{
					UE_LOG(LogOnline, Log,TEXT("MultiplayerService::GetSessionsAsync with 0x%0.8X"), ex->HResult);

					LiveSubsystem->ExecuteNextTick([this,SearchSettings]()
					{
						CurrentSessionSearch = nullptr;
						SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
						TriggerOnFindSessionsCompleteDelegates(false);
					});
				}
			});

			// Copy the search pointer so we can keep it around
			CurrentSessionSearch = SearchSettings;
			SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Log,TEXT("MultiplayerService::GetSessionsAsync failed with 0x%0.8X: %s"), ex->HResult, ex->ToString()->Data());
			TriggerOnFindSessionsCompleteDelegates(false);
			return false;
		}
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("Ignoring LIVE Session Search request while one is pending."));
	}

	return true;
}

bool FOnlineSessionLive::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	UNREFERENCED_PARAMETER(FriendId);

	if (!SearchingUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid local-user: %s"), *SearchingUserId.ToString());
		LiveSubsystem->ExecuteNextTick([this, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(0, false, EmptyResult);
		});
		return false;
	}

	FOnlineIdentityLivePtr IdentityPtr = LiveSubsystem->GetIdentityLive();
	if (!IdentityPtr.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Identity Interface invalid"));
		LiveSubsystem->ExecuteNextTick([this, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(0, false, EmptyResult);
		});
		return false;
	}

	const int32 LocalUserNum = IdentityPtr->GetPlatformUserIdFromUniqueNetId(SearchingUserId);
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid local-user: %s"), *SearchingUserId.ToString());
		LiveSubsystem->ExecuteNextTick([this, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(0, false, EmptyResult);
		});
		return false;
	}

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unknown local-user: %s"), *SearchingUserId.ToString());
		LiveSubsystem->ExecuteNextTick([this, LocalUserNum, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(LocalUserNum, false, EmptyResult);
		});
		return false;
	}

	if (!SessionId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid session id: %s"), *SessionId.ToString());
		LiveSubsystem->ExecuteNextTick([this, LocalUserNum, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(LocalUserNum, false, EmptyResult);
		});
		return false;
	}

	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveFindSessionById>(LiveSubsystem, LiveContext, LocalUserNum, SessionId.ToString(), CompletionDelegate);
	return true;
}

bool FOnlineSessionLive::CancelFindSessions()
{
	// Unsupported
	LiveSubsystem->ExecuteNextTick([this]()
	{
		TriggerOnCancelFindSessionsCompleteDelegates(false);
	});

	return false;
}

FOnlineSessionSearchResult* ResultFromSDA(Platform::String^ SDABase64, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	for (FOnlineSessionSearchResult& SearchResult : SearchSettings->SearchResults)
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SearchResult.Session.SessionInfo);
		for (auto Member : LiveInfo->GetLiveMultiplayerSession()->Members)
		{
			if (Member->SecureDeviceAddressBase64 == SDABase64)
			{
				return &SearchResult;
			}
		}
	}
	return nullptr;
}
void FOnlineSessionLive::PingResultsAndTriggerDelegates(const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	Vector<SecureDeviceAddress^>^ Addresses = ref new Vector<SecureDeviceAddress^>();
	Vector<QualityOfServiceMetric>^ Metrics = ref new Vector<QualityOfServiceMetric>();
	Metrics->Append(QualityOfServiceMetric::LatencyAverage);

	for (auto SearchResult : SearchSettings->SearchResults)
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SearchResult.Session.SessionInfo);
		auto Host = GetLiveSessionHost(LiveInfo->GetLiveMultiplayerSession());
		if (nullptr == Host) //Non Thunderhead dedicated servers need to manually ping the result here...
		{
			continue;
		}

		Platform::String^ HostSDABase64 = Host->SecureDeviceAddressBase64;
		if (nullptr == HostSDABase64)
		{
			continue;
		}

		SecureDeviceAddress^ SDA = SecureDeviceAddress::FromBase64String(HostSDABase64);
		if (nullptr == SDA) //Non Thunderhead dedicated servers need to manually ping the result here...
		{
			continue;
		}

		UE_LOG(LogOnline, Log, TEXT("Measuring Address: %s"), HostSDABase64->Data());
		Addresses->Append(SDA);
	}

	if (Addresses->Size > 0)
	{
		Concurrency::create_task(
			QualityOfService::MeasureQualityOfServiceAsync(
			Addresses,
			Metrics,
			QOS_TIMEOUT_MILLISECONDS,
			QOS_PROBE_COUNT
			))
			.then([this, SearchSettings](Concurrency::task<MeasureQualityOfServiceResult^> Task)
		{
			try
			{
				MeasureQualityOfServiceResult^ Result = Task.get();

				for (auto Measurement : Result->Measurements)
				{

					if (Measurement->Status == QualityOfServiceMeasurementStatus::PartialResults || Measurement->Status == QualityOfServiceMeasurementStatus::Success)
					{
						if (FOnlineSessionSearchResult* Result = ResultFromSDA(Measurement->SecureDeviceAddress->GetBase64String(), SearchSettings))
						{
							Result->PingInMs = Measurement->MetricValue->GetUInt32();
						}
					}
				}
			}
			catch(Platform::COMException^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("MeasureQualityOfServiceAsync failed: 0x%0.8X"), Ex->HResult);
			}

			LiveSubsystem->ExecuteNextTick([this,SearchSettings]()
			{
				SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
				CurrentSessionSearch = nullptr;
				TriggerOnFindSessionsCompleteDelegates(true);
			});
		});
	}
	else
	{
		LiveSubsystem->ExecuteNextTick([this,SearchSettings]()
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
			CurrentSessionSearch = nullptr;
			TriggerOnFindSessionsCompleteDelegates(true);
		});
	}
}

int32 FOnlineSessionLive::GetHostingPlayerNum(const FUniqueNetId& HostNetId) const
{
	FOnlineIdentityLivePtr IdentityPtr = LiveSubsystem->GetIdentityLive();
	if (!IdentityPtr.IsValid())
	{
		return -1;
	}

	return IdentityPtr->GetPlatformUserIdFromUniqueNetId(HostNetId);
}

bool FOnlineSessionLive::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	return LiveSubsystem->GetMatchmakingInterfaceLive()->StartMatchmaking(LocalPlayers, SessionName, NewSessionSettings, SearchSettings);
}

bool FOnlineSessionLive::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	return LiveSubsystem->GetMatchmakingInterfaceLive()->CancelMatchmaking(SearchingPlayerNum, SessionName);
}

bool FOnlineSessionLive::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	return LiveSubsystem->GetMatchmakingInterfaceLive()->CancelMatchmaking(SearchingPlayerId, SessionName);
}

bool FOnlineSessionLive::JoinSession(int32 ControllerIndex, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(ControllerIndex);
	if (!UniqueId.IsValid())
	{
		LiveSubsystem->ExecuteNextTick([SessionName, ControllerIndex, this]()
		{
			UE_LOG_ONLINE(Warning, TEXT("JoinSession failed; unable to find player at index %d"), ControllerIndex);
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		});
		return false;
	}

	return JoinSession(*UniqueId, SessionName, DesiredSession);
}

bool FOnlineSessionLive::JoinSession(const FUniqueNetId& UserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	UE_LOG_ONLINE(Log, TEXT("User %s Attempting to join session %s"), *UserId.ToString(), *SessionName.ToString());

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(UserId);
	if (LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Join session %s failed, user %s has no live context"), *SessionName.ToString(), *UserId.ToString());
		LiveSubsystem->ExecuteNextTick([SessionName, this]()
		{
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		});
		return false;
	}

	// work out if we're already in the session of this name or not
	FNamedOnlineSession* const NamedSessionCheck = GetNamedSession(SessionName);
	if (NamedSessionCheck)
	{
		// Check if we're trying to join a different session of this same type while in a different session (different than joining the same session multiple times)
		const FString ExistingSessionId(NamedSessionCheck->GetSessionIdStr());
		const FString NewSessionId(DesiredSession.GetSessionIdStr());

		if (ExistingSessionId == NewSessionId)
		{
			UE_LOG_ONLINE(Warning, TEXT("Join session failed; session (%s) already exists, can't join twice"), *SessionName.ToString());
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Join session failed; already in session of type %s, you must leave session %s before joining %s"), *SessionName.ToString(), *ExistingSessionId, *NewSessionId);
		}

		LiveSubsystem->ExecuteNextTick([SessionName, this]()
		{
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		});
		return false;
	}

	// If there's no secure device association template, we can't get the host's address.
	if (!PeerTemplate && !DesiredSession.Session.SessionSettings.bIsDedicated)
	{
		UE_LOG_ONLINE(Warning, TEXT("Join session %s failed, no host secure device association template"), *SessionName.ToString());
		LiveSubsystem->ExecuteNextTick([SessionName, this]()
		{
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
		});
		return false;
	}

	// Check for a Join from URI (Join In Progress from Matchmade Sessions)
	FString SessionURI;
	if (DesiredSession.Session.SessionSettings.Get(SETTING_GAME_SESSION_URI, SessionURI))
	{
		FNamedOnlineSession* const NamedSession = AddNamedSession(SessionName, DesiredSession.Session.SessionSettings);
		FString SessionTemplateName;
		DesiredSession.Session.SessionSettings.Get(SETTING_SESSION_TEMPLATE_NAME, SessionTemplateName);

		MultiplayerSessionReference^ SessionReference = MultiplayerSessionReference::ParseFromUriPath(ref new Platform::String(*SessionURI));

		const bool bIsMatchmakingSession = true;
		const bool bSetActivity = DesiredSession.Session.SessionSettings.bUsesPresence;
		LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveJoinSession>(this, SessionReference, PeerTemplate, LiveContext, NamedSession, LiveSubsystem, MAX_RETRIES, bIsMatchmakingSession, bSetActivity, TOptional<FString>());
		return true;
	}

	// Create a named session from the search result data
	FNamedOnlineSession* const NamedSession = AddNamedSession(SessionName, DesiredSession.Session);
	// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
	// Comments and other OSS implementations indicate the on non-host machines HostingPlayerNum 
	// should be the local index of the player that iniated Join...
	NamedSession->HostingPlayerNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(UserId);
	// @ATG_CHANGE : END
	NamedSession->LocalOwnerId = MakeShared<FUniqueNetIdLive>(UserId);

	FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(DesiredSession.Session.SessionInfo);
	if (!LiveInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Join session %s failed, invalid session info on search result"), *SessionName.ToString());
		RemoveNamedSession(SessionName);

		LiveSubsystem->ExecuteNextTick([SessionName, this]()
		{
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		});
		return false;
	}

	MultiplayerSessionReference^ LiveSessionRef = LiveInfo->GetLiveMultiplayerSessionRef();

	const bool bIsMatchmakingSession = false;
	const bool bSetActivity = DesiredSession.Session.SessionSettings.bUsesPresence;

	// Ensure this finishes before session notifications are processed so session is initialized
	LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveJoinSession>(this, LiveSessionRef, PeerTemplate, LiveContext, NamedSession, LiveSubsystem, MAX_RETRIES, bIsMatchmakingSession, bSetActivity, LiveInfo->GetSessionInviteHandle());
	return true;
}

MultiplayerSessionMember^ FOnlineSessionLive::GetCurrentUserFromSession(MultiplayerSession^ LiveSession)
{
	for (auto Member : LiveSession->Members)
	{
		if (Member->IsCurrentUser)
		{
			return Member;
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// This API returns the "Advertised Session" which my friend is in, not all sessions they are in.
//-----------------------------------------------------------------------------

bool FOnlineSessionLive::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	try
	{
		Platform::Collections::Vector<Platform::String^>^ FriendVector = ref new Platform::Collections::Vector<Platform::String^>;
		FriendVector->Append(ref new Platform::String(*Friend.ToString()));

		XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
		if (LiveContext == nullptr)
		{
			UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to retrieve Friend's multiplayer, no available LiveContext for user %d"), LocalUserNum);
			LiveSubsystem->ExecuteNextTick([this, LocalUserNum]()
			{
				TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());
			});
			return false;
		}

		const FUniqueNetIdLive& LiveFriend = static_cast<const FUniqueNetIdLive>(Friend);

		auto GetActivitiesOp =
			// @ATG_CHANGE : BEGIN - UWP support
			LiveContext->MultiplayerService->GetActivitiesForUsersAsync(LiveContext->AppConfig->ServiceConfigurationId,
			// @ATG_CHANGE : END - UWP support
																		FriendVector->GetView());

		Concurrency::create_task(GetActivitiesOp)
			.then([this, LocalUserNum, LiveContext, LiveFriend](Concurrency::task<IVectorView<MultiplayerActivityDetails^> ^> Task)
			{
				try
				{
					IVectorView<MultiplayerActivityDetails^>^ ActivityDetails = Task.get();
					if (ActivityDetails->Size < 1)
					{
						// Friend has no advertised active session
						UE_LOG_ONLINE(Verbose, TEXT("FindFriendSession: Friend has no multiplayer activity"));
						LiveSubsystem->ExecuteNextTick([this, LocalUserNum, LiveFriend]()
						{
							TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());

							FOnlinePresenceLivePtr PresentInt = LiveSubsystem->GetPresenceLive();
							if (PresentInt.IsValid())
							{
								PresentInt->OnFindFriendSessionCompleteForSessionUpdated(LocalUserNum, false, FOnlineSessionSearchResult(), LiveFriend);
							}
						});
					}
					else
					{
						auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(ActivityDetails->GetAt(0)->SessionReference);
						Concurrency::create_task(GetSessionOp)
							.then([this, LocalUserNum, LiveContext, LiveFriend](Concurrency::task<MultiplayerSession^> Task)
							{
								try
								{
									MultiplayerSession^ FriendLiveSession = Task.get();

									MultiplayerSessionMember^ SessionHost = GetLiveSessionHost(FriendLiveSession);

									Platform::String^ HostDisplayName = SessionHost != nullptr ? SessionHost->Gamertag : nullptr;

									LiveSubsystem->ExecuteNextTick([this, LocalUserNum, FriendLiveSession, HostDisplayName, LiveFriend]()
									{
										FOnlineSessionSearchResult SearchResult = CreateSearchResultFromSession(FriendLiveSession, HostDisplayName);

										// Trigger success delegate
										{
											TArray<FOnlineSessionSearchResult> FriendSessions;
											FriendSessions.Add(SearchResult);

											TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, true, FriendSessions); 
										}

										FOnlinePresenceLivePtr PresentInt = LiveSubsystem->GetPresenceLive();
										if (PresentInt.IsValid())
										{
											PresentInt->OnFindFriendSessionCompleteForSessionUpdated(LocalUserNum, true, SearchResult, LiveFriend);
										}
									});
								}
								catch (Platform::Exception^ Ex)
								{
									// Ignore JSON parse errors, they indicate we don't have access to read the session (private sessions)
									if (Ex->HResult != WEB_E_INVALID_JSON_STRING)
									{
										UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to retrieve MultiplayerSession with 0x%0.8X"), Ex->HResult);
									}

									LiveSubsystem->ExecuteNextTick([this, LocalUserNum, LiveFriend]()
									{
										TArray<FOnlineSessionSearchResult> FriendSessions;
										TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, FriendSessions);

										FOnlinePresenceLivePtr PresentInt = LiveSubsystem->GetPresenceLive();
										if (PresentInt.IsValid())
										{
											PresentInt->OnFindFriendSessionCompleteForSessionUpdated(LocalUserNum, false, FOnlineSessionSearchResult(), LiveFriend);
										}
									});
								}
							}
						);
					}
				}
				catch (Platform::Exception^ ex)
				{
					UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to retrieve Friend's multiplayer activity with 0x%0.8X"), ex->HResult);
					LiveSubsystem->ExecuteNextTick([this, LocalUserNum]()
					{
						TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>()); 
					});
				}
			}
		);
	}
	catch (Platform::Exception^ ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to query Friend's multiplayer activity with 0x%0.8X"), ex->HResult);
		LiveSubsystem->ExecuteNextTick([this, LocalUserNum]()
		{
			TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>()); 
		});
		return false;
	}

	return true;
};

bool FOnlineSessionLive::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	int32 ControllerId = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(LocalUserId);
	if (ControllerId < 0 || ControllerId >= MAX_LOCAL_PLAYERS)
	{
		LiveSubsystem->ExecuteNextTick([this, ControllerId]()
		{
			TArray<FOnlineSessionSearchResult> FriendSessions;
			TriggerOnFindFriendSessionCompleteDelegates(-1, false, FriendSessions); 
		});
		return false;
	}

	return FindFriendSession(ControllerId, Friend);
}

bool FOnlineSessionLive::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	bool bSuccessfullyJoinedFriendSession = false;

	UE_LOG(LogOnline, Display, TEXT("FOnlineSessionLive::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) - not implemented"));

	int32 LocalUserNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(LocalUserId);

	TArray<FOnlineSessionSearchResult> EmptyResult;
	TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, bSuccessfullyJoinedFriendSession, EmptyResult);

	return bSuccessfullyJoinedFriendSession;
}

bool FOnlineSessionLive::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	if (!Friend.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite invalid friend to session %s"), *SessionName.ToString());
		return false;
	}

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friend %s to session %s, LocalUserNum %d is invalid"), *Friend.ToString(), *SessionName.ToString(), LocalUserNum);
		return false;
	}

	Platform::Collections::Vector<Platform::String^>^ FriendsToInvite = ref new Platform::Collections::Vector<Platform::String^>();
	FriendsToInvite->Append(ref new Platform::String(*Friend.ToString()));

	return SendSessionInviteToFriends_Internal(LiveContext, SessionName, FriendsToInvite->GetView());
}

bool FOnlineSessionLive::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	if (!LocalUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite friend to session %s, LocalUserId is invalid"), *SessionName.ToString());
		return false;
	}

	if (!Friend.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite invalid friend to session %s"), *SessionName.ToString());
		return false;
	}

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserId);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friend %s to session %s, LocalUserId %s is invalid"), *Friend.ToString(), *SessionName.ToString(), *LocalUserId.ToString());
		return false;
	}

	Platform::Collections::Vector<Platform::String^>^ FriendsToInvite = ref new Platform::Collections::Vector<Platform::String^>();
	FriendsToInvite->Append(ref new Platform::String(*Friend.ToString()));

	return SendSessionInviteToFriends_Internal(LiveContext, SessionName, FriendsToInvite->GetView());
}

bool FOnlineSessionLive::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	if (Friends.Num() < 1)
	{
		// Return true in this case, but log it since it's strange
		UE_LOG_ONLINE(Warning, TEXT("Attempted to invite any empty array of friends to session %s"), *SessionName.ToString());
		return true;
	}

	for (const TSharedRef<const FUniqueNetId>& Friend : Friends)
	{
		if (!Friend->IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Cannot Invite invalid friend to session %s"), *SessionName.ToString());
			return false;
		}
	}

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, LocalUserNum %d is invalid"), *SessionName.ToString(), LocalUserNum);
		return false;
	}

	Platform::Collections::Vector<Platform::String^>^ FriendsToInvite = ref new Platform::Collections::Vector<Platform::String^>();
	for (const TSharedRef<const FUniqueNetId>& Friend : Friends)
	{
		FriendsToInvite->Append(ref new Platform::String(*Friend->ToString()));
	}

	return SendSessionInviteToFriends_Internal(LiveContext, SessionName, FriendsToInvite->GetView());
}

bool FOnlineSessionLive::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	if (!LocalUserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite friend to session %s, LocalUserId is invalid"), *SessionName.ToString());
		return false;
	}

	if (Friends.Num() < 1)
	{
		// Return true in this case, but log it since it's strange
		UE_LOG_ONLINE(Warning, TEXT("Attempted to invite any empty array of friends to session %s"), *SessionName.ToString());
		return true;
	}

	for (const TSharedRef<const FUniqueNetId>& Friend : Friends)
	{
		if (!Friend->IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Cannot Invite invalid friend to session %s"), *SessionName.ToString());
			return false;
		}
	}

	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LocalUserId);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, LocalUserId %s is invalid"), *SessionName.ToString(), *LocalUserId.ToString());
		return false;
	}

	Platform::Collections::Vector<Platform::String^>^ FriendsToInvite = ref new Platform::Collections::Vector<Platform::String^>();
	for (const TSharedRef<const FUniqueNetId>& Friend : Friends)
	{
		FriendsToInvite->Append(ref new Platform::String(*Friend->ToString()));
	}

	return SendSessionInviteToFriends_Internal(LiveContext, SessionName, FriendsToInvite->GetView());}

bool FOnlineSessionLive::SendSessionInviteToFriends_Internal(XboxLiveContext^ LiveContext, FName SessionName, Windows::Foundation::Collections::IVectorView<Platform::String^>^ FriendXuidVectorView)
{
	FNamedOnlineSession* SessionPtr = GetNamedSession(SessionName);
	if (!SessionPtr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, that session does not exist"), *SessionName.ToString());
		return false;
	}

	if (SessionPtr->SessionState < EOnlineSessionState::Pending || SessionPtr->SessionState > EOnlineSessionState::InProgress)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, that session is in state %d"), *SessionName.ToString(), SessionPtr->SessionState);
		return false;
	}

	FOnlineSessionInfoLivePtr SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SessionPtr->SessionInfo);
	if (!SessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, that session has invalid info"), *SessionName.ToString());
		return false;
	}

	MultiplayerSessionReference^ LiveSessionReference = SessionInfo->GetLiveMultiplayerSessionRef();
	if (!LiveSessionReference)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot Invite Friends to session %s, that session has an invalid reference"), *SessionName.ToString());
		return false;
	}

	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSendSessionInviteToFriends>(LiveSubsystem, LiveContext, LiveSessionReference, FriendXuidVectorView);
	return true;
}

void FOnlineSessionLive::UpdateSessionChangedStats()
{
	if (SessionUpdateStatName.IsEmpty() || SessionUpdateEventName.IsEmpty())
	{
		return;
	}

	UE_LOG_ONLINE(VeryVerbose, TEXT("Updating Session Changed Stats"))

	// This is a hack so that friends may subscribe to session change notifications

	FOnlineIdentityLivePtr IdentityPtr = LiveSubsystem->GetIdentityLive();
	IOnlineEventsPtr EventsInt = LiveSubsystem->GetEventsInterface();
	if (IdentityPtr.IsValid() && EventsInt.IsValid())
	{
		for (int32 Index = 0; Index < MAX_LOCAL_PLAYERS; ++Index)
		{
			const TSharedPtr<const FUniqueNetId> LocalPlayerNetId = IdentityPtr->GetUniquePlayerId(Index);
			if (LocalPlayerNetId.IsValid() && LocalPlayerNetId->IsValid())
			{
				static const FName SessionUpdateName(*SessionUpdateStatName);
				static const FVariantData IncrementData = FVariantData(static_cast<int32>(1));

				FOnlineEventParms StatParams;
				StatParams.Add(SessionUpdateName, IncrementData);
				if (!EventsInt->TriggerEvent(*LocalPlayerNetId, *SessionUpdateEventName, StatParams))
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to trigger session updated event"));
				}
			}
		}
	}
}

bool FOnlineSessionLive::IsSubscribedToSessionStatUpdates(const FUniqueNetIdLive& PlayerId) const
{
	return SessionUpdateStatSubsriptions.Contains(PlayerId);
}

void FOnlineSessionLive::AddSessionUpdateStatSubscription(const FUniqueNetIdLive& PlayerId)
{
	SessionUpdateStatSubsriptions.Add(PlayerId);
}

void FOnlineSessionLive::SetCurrentUserActive(int32 UserNum, MultiplayerSession^ LiveSession, bool bIsActive)
{
	check(LiveSession);

	//. Mark the current user as active, or otherwise
	LiveSession->SetCurrentUserStatus(bIsActive ? MultiplayerSessionMemberStatus::Active : MultiplayerSessionMemberStatus::Inactive);
}

/** Get a resolved connection string from a session info */
static bool GetConnectStringFromSessionInfo(const FOnlineSessionInfoLivePtr& SessionInfo, FString& ConnectInfo, int32 PortOverride = 0)
{
	bool bSuccess = false;

	if (SessionInfo.IsValid())
	{
		const TSharedPtr<FInternetAddr> IpAddr = SessionInfo->GetHostAddr();
		if (IpAddr.IsValid() && IpAddr->IsValid())
		{
			if (PortOverride != 0)
			{
				ConnectInfo = FString::Printf(TEXT("%s:%d"), *IpAddr->ToString(false), PortOverride);
			}
			else
			{
				ConnectInfo = FString::Printf(TEXT("%s"), *IpAddr->ToString(true));
			}

			bSuccess = true;
		}
	}

	return bSuccess;
}

bool FOnlineSessionLive::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	bool bSuccess = false;
	// Find the session
	FNamedOnlineSession* const Session = GetNamedSession(SessionName);
	if (Session != NULL)
	{
		FOnlineSessionInfoLivePtr SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);

		if (PortType == NAME_BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(Session->SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == NAME_GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}

		if (!bSuccess)
		{
			UE_LOG_ONLINE(Warning, TEXT("Invalid session info for session %s in GetResolvedConnectString()"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning,
			TEXT("Unknown session name (%s) specified to GetResolvedConnectString()"),
			*SessionName.ToString());
	}

	return bSuccess;
}

bool FOnlineSessionLive::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	bool bSuccess = false;
	if (SearchResult.Session.SessionInfo.IsValid())
	{
		FOnlineSessionInfoLivePtr SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SearchResult.Session.SessionInfo);

		if (PortType == NAME_BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(SearchResult.Session.SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == NAME_GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}
	}

	if (!bSuccess || ConnectInfo.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
	}

	return bSuccess;
}

FOnlineSessionSettings* FOnlineSessionLive::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* const MySession = GetNamedSession(SessionName);
	if (!MySession)
	{
		return nullptr;
	}

	return &MySession->SessionSettings;
}

bool FOnlineSessionLive::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShared<FUniqueNetIdLive>(PlayerId));
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionLive::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

				FUniqueNetIdMatcher PlayerMatch(*PlayerId);
				if (Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) == INDEX_NONE)
				{
					Session->RegisteredPlayers.Add(PlayerId);
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Player %s already registered in session %s"), *Players[PlayerIdx]->ToDebugString(), *SessionName.ToString());
				}

				RegisterVoice(*PlayerId);
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to join for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
	}

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

bool FOnlineSessionLive::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray<TSharedRef<const FUniqueNetId>> Players;
	Players.Add(MakeShared<FUniqueNetIdLive>(PlayerId));
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionLive::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	bool bSuccess = false;

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

				FUniqueNetIdMatcher PlayerMatch(*PlayerId);
				int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
				if (RegistrantIndex != INDEX_NONE)
				{
					Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
					UnregisterVoice(*PlayerId);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to leave for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

bool FOnlineSessionLive::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);

	if (!NamedSession)
	{
		LiveSubsystem->ExecuteNextTick([this, SessionName]()
		{
			TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		});
		return false;
	}

	if (bShouldRefreshOnlineData)
	{
		XboxLiveContext^ LiveContext = nullptr;
		const TSharedPtr<const FUniqueNetId>& HostNetId(NamedSession->OwningUserId);
		if (HostNetId.IsValid())
		{
			LiveContext = LiveSubsystem->GetLiveContext(*HostNetId);
		}

		if (LiveContext == nullptr)
		{
			if (!bOnlyHostUpdateSession)
			{
				const TSharedPtr<const FUniqueNetId>& LocalOwnerId(NamedSession->LocalOwnerId);
				if (LocalOwnerId.IsValid())
				{
					LiveContext = LiveSubsystem->GetLiveContext(*LocalOwnerId);
				}
			}
		}

		if (LiveContext == nullptr)
		{
			LiveSubsystem->ExecuteNextTick([this, SessionName]()
			{
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
			});
			return false;
		}

		LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveUpdateSession>(SessionName, LiveContext, LiveSubsystem, MAX_RETRIES, UpdatedSessionSettings);
		return true;
	}
	else //Update Player constants/Player group info
	{
		int32 NumPendingUpdates = 0;
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			if (MultiplayerSession^ Session = LiveInfo->GetLiveMultiplayerSession())
			{
				for (MultiplayerSessionMember^ Member : Session->Members)
				{
					if (XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(FUniqueNetIdLive(Member->XboxUserId)))
					{
						constexpr const int32 RetryCount = 10;
						LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveUpdateSessionMember>(SessionName, LiveContext, LiveSubsystem, RetryCount);
						++NumPendingUpdates;
					}
				}
			}
		}

		if (NumPendingUpdates == 0)
		{
			LiveSubsystem->ExecuteNextTick([this, SessionName]()
			{
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
			});
			return false;
		}

		return true;
	}
}

void FOnlineSessionLive::ReadSettingsFromLiveJson(MultiplayerSession^ LiveSession, FOnlineSession& Session, XboxLiveContext^ LiveContext)
{
	FSessionSettings NewSettings;

	// Copy existing values that are not service based
	for (const FOnlineKeyValuePairs<FName, FOnlineSessionSetting>::ElementType& Pair : Session.SessionSettings.Settings)
	{
		const FName						SettingName = Pair.Key;
		const FOnlineSessionSetting&	SettingValue = Pair.Value;

		if (SettingValue.AdvertisementType < EOnlineDataAdvertisementType::ViaOnlineService)
		{
			NewSettings.Add(SettingName, SettingValue);
		}
	}

	Platform::String^ PlatformPropertiesJson = LiveSession->SessionProperties->SessionCustomPropertiesJson;
	FString PropertiesJson(PlatformPropertiesJson->Data());
	TSharedPtr< FJsonObject > JObj;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(PropertiesJson);

	if (FJsonSerializer::Deserialize(Reader, JObj) && JObj.IsValid())
	{
		for (const TMap<FString, TSharedPtr<FJsonValue>>::ElementType& Pair : JObj->Values)
		{
			const FString&					JSettingName = Pair.Key;
			const TSharedPtr<FJsonValue>&	JSettingValue = Pair.Value;

			FOnlineSessionSetting NewSetting;

			// Create setting of matching data type
			switch (JSettingValue->Type)
			{
				case EJson::Array:
				case EJson::Object:
				case EJson::String:
					NewSetting = FOnlineSessionSetting(JSettingValue->AsString(), EOnlineDataAdvertisementType::ViaOnlineService);
					break;

				case EJson::Number:
					NewSetting = FOnlineSessionSetting(JSettingValue->AsNumber(), EOnlineDataAdvertisementType::ViaOnlineService);
					break;

				case EJson::Boolean:
					NewSetting = FOnlineSessionSetting(JSettingValue->AsBool(), EOnlineDataAdvertisementType::ViaOnlineService);
					break;

				default: continue;
			}

			if (JSettingName == TEXT("HostXboxUserId"))
			{
				if (LiveContext != nullptr)
				{
					// If we pass in a live context, we are assuming this is on an async task already
					// In this case, look up the name from the json value to override the OwningUserName
					Platform::String^ HostXboxUserId = ref new Platform::String(*NewSetting.Data.ToString());

					try
					{
						auto ProfileOp = LiveContext->ProfileService->GetUserProfileAsync(HostXboxUserId);

						// We're already in an async callback so it should be OK to block on the get() here.
						XboxUserProfile^ HostProfile = Concurrency::create_task(ProfileOp).get();
						Session.OwningUserName = FString(HostProfile->GameDisplayName->Data());
					}
					catch (Platform::COMException^ Ex)
					{
						UE_LOG(LogOnline, Warning, TEXT("A ProfileService::GetUserProfileAsync call failed with 0x%0.8X"), Ex->HResult);
					}
				}
			}
			else if (JSettingName == TEXT("_flags") && NewSetting.Data.GetType() == EOnlineKeyValuePairDataType::String)
			{
				FString SessionSettingsFlagsValue;
				NewSetting.Data.GetValue(SessionSettingsFlagsValue);

				int16 SessionSettingsFlags = 0;
				LexFromString(SessionSettingsFlags, *SessionSettingsFlagsValue);
				
				int32 BitShift = 0;
				Session.SessionSettings.bShouldAdvertise = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bAllowJoinInProgress = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bIsLANMatch = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bIsDedicated = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bUsesStats = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bAllowInvites = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bUsesPresence = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bAllowJoinViaPresence = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bAllowJoinViaPresenceFriendsOnly = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
				Session.SessionSettings.bAntiCheatProtected = (SessionSettingsFlags & (1 << BitShift++)) ? true : false;
			}
			else
			{
				NewSettings.Add(FName(*JSettingName), NewSetting);
			}
		}
	}

	UpdateMatchMembersJson(NewSettings, LiveSession);

	// Finally, replace existing with new settings (this deletes settings that have been removed)
	Session.SessionSettings.Settings = NewSettings;
}

void FOnlineSessionLive::ExtractJsonMemberSettings(MultiplayerSession^ LiveSession, FString& OutJsonString)
{
	bool NeedComma = false;

	// We could have used JsonWriter here, but it escapes JSON characters, so instead
	// we just build it manually so that we can pass our snippets of JSON directly

	OutJsonString = FString(TEXT("{\"Members\":["));

	for (MultiplayerSessionMember^ member : LiveSession->Members)
	{
		OutJsonString += FString::Printf(TEXT("%s{\"xuid\":\"%s\", \"constants\":%s, \"properties\":%s}"),
					NeedComma ? TEXT(",") : TEXT(""),
					member->XboxUserId->Data(),
					member->MemberCustomConstantsJson->Data(),
					member->MemberCustomPropertiesJson->Data());

		NeedComma = true;
	}

	OutJsonString +=  FString(TEXT("]}"));
}

void FOnlineSessionLive::UpdateMatchMembersJson(FSessionSettings& UpdatedSettings, MultiplayerSession^ LiveSession)
{
	FString MemberSessionJson;

	ExtractJsonMemberSettings(LiveSession, MemberSessionJson);
	UpdatedSettings.Remove(SETTING_MATCH_MEMBERS_JSON);
	UpdatedSettings.Add(SETTING_MATCH_MEMBERS_JSON, FOnlineSessionSetting(MemberSessionJson, EOnlineDataAdvertisementType::DontAdvertise));
}

void FOnlineSessionLive::WriteSettingsToLiveJson(const FOnlineSessionSettings& SessionSettings, MultiplayerSession^ LiveSession, User^ HostUser)
{
	{
		int32 BitShift = 0;
		int32 SessionSettingsFlags = 0;
		SessionSettingsFlags |= ((int32)SessionSettings.bShouldAdvertise) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bAllowJoinInProgress) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bIsLANMatch) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bIsDedicated) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bUsesStats) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bAllowInvites) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bUsesPresence) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bAllowJoinViaPresence) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bAllowJoinViaPresenceFriendsOnly) << BitShift++;
		SessionSettingsFlags |= ((int32)SessionSettings.bAntiCheatProtected) << BitShift++;
		Platform::String^ SessionSettingsFlagsName = ref new Platform::String(L"_flags");
		// Wrap with quotes so we send it as a JSON string and not a JSON number (we don't want to convert to double).  FString::FromInt will not wrap in quotes.
		Platform::String^ SessionSettingsFlagsValue = ref new Platform::String(*FString::Printf(TEXT("\"%d\""), SessionSettingsFlags));
		LiveSession->SetSessionCustomPropertyJson(SessionSettingsFlagsName, SessionSettingsFlagsValue);
	}

	for (FSessionSettings::TConstIterator It(SessionSettings.Settings); It; ++It)
	{
		const FName& SettingName = It.Key();
		const FOnlineSessionSetting& SettingValue = It.Value();

		// Only upload values that are marked for service use
		if (SettingValue.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			Platform::String^ PlatformName  = ref new Platform::String(*SettingName.ToString());
			Platform::String^ PlatformValue = ref new Platform::String(*SettingValue.Data.ToString());

			LiveSession->SetSessionCustomPropertyJson(PlatformName, PlatformValue);
		}
	}

	// If we have a host, write the name of the host to the session
	// This is a workaround for the client not having a reliable way to determine who the host is in splitscreen
	if (HostUser != nullptr)
	{
		// We have to add quotes so it's treated as a string in json
		FString XboxUserId = FString::Printf(TEXT("\"%s\""), *FString(HostUser->XboxUserId->Data()));

		Platform::String^ XboxUserIdStr = ref new Platform::String(*XboxUserId);

		LiveSession->SetSessionCustomPropertyJson("HostXboxUserId", XboxUserIdStr);
	}
}

// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
void FOnlineSessionLive::WriteSessionPrivacySettingsToLiveJson(const FOnlineSessionSettings& SessionSettings, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession)
{
	// Note that sessions with "userAuthorizationStyle" : true MUST currently have read and join
	// restrictions of either Followed or Local.  Here we're making a best-effort attempt to match
	// the title's request.  Note that because we check the value of a session capability it is NOT
	// valid to call this before the session has been written to the service.
	bool ServiceWillAllowPublicSession = !LiveSession->SessionConstants->CapabilitiesUserAuthorizationStyle;
	if (SessionSettings.bShouldAdvertise)
	{
		LiveSession->SessionProperties->ReadRestriction = ServiceWillAllowPublicSession ? MultiplayerSessionRestriction::None : MultiplayerSessionRestriction::Followed;
	}
	else
	{
		LiveSession->SessionProperties->ReadRestriction = MultiplayerSessionRestriction::Local;
	}

	if (SessionSettings.bAllowJoinViaPresenceFriendsOnly)
	{
		LiveSession->SessionProperties->JoinRestriction = MultiplayerSessionRestriction::Followed;
	}
	else if (SessionSettings.bAllowJoinViaPresence)
	{
		LiveSession->SessionProperties->JoinRestriction = ServiceWillAllowPublicSession ? MultiplayerSessionRestriction::None : MultiplayerSessionRestriction::Followed;
	}
	else
	{
		LiveSession->SessionProperties->JoinRestriction = MultiplayerSessionRestriction::Local;
	}
}
// @ATG_CHANGE : END

bool FOnlineSessionLive::StartSession(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Can't start a match multiple times
	if (Session->SessionState != EOnlineSessionState::Pending &&
		Session->SessionState != EOnlineSessionState::Ended)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Generate a new RoundId for Xbox events.
	FOnlineSessionInfoLivePtr SessionInfoLive = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
	if (SessionInfoLive.IsValid())
	{
		SessionInfoLive->SetRoundId(FGuid::NewGuid());
	}

	// TODO: Handle join in progress vs. not join in progress.
	Session->SessionState = EOnlineSessionState::InProgress;

	TriggerOnStartSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionLive::EndSession(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState != EOnlineSessionState::InProgress)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Ended;
	TriggerOnEndSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionLive::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	UE_LOG_ONLINE(Log, TEXT("Attempting to destroy session %s"), *SessionName.ToString());

	// Technically we can't actually destroy a session on Live, all we can do is Leave() it,
	// hope everyone else leaves also, and let it time out.

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
		LiveSubsystem->ExecuteNextTick([this, SessionName, CompletionDelegate]()
		{
			CompletionDelegate.ExecuteIfBound(SessionName, false);
			TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		});
		return false;
	}

	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// Purposefully skip the delegate call as one should already be in flight
		UE_LOG_ONLINE(Warning, TEXT("Already in process of destroying session (%s)"), *SessionName.ToString());
		LiveSubsystem->ExecuteNextTick([this, SessionName, CompletionDelegate]()
		{
			CompletionDelegate.ExecuteIfBound(SessionName, false);
			TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		});
		return false;
	}

	Session->SessionState = EOnlineSessionState::Destroying;

	FOnlineSessionInfoLivePtr LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
	if (!LiveSessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Destroying an online session (%s) will null Live info. No writes to the MPSD will occur."), *SessionName.ToString());
		RemoveNamedSession(SessionName);
		LiveSubsystem->ExecuteNextTick([this, SessionName, CompletionDelegate]()
		{
			CompletionDelegate.ExecuteIfBound(SessionName, true);
			TriggerOnDestroySessionCompleteDelegates(SessionName, true);
		});
		return false;
	}

	MultiplayerSession^ LiveSession = LiveSessionInfo->GetLiveMultiplayerSession();
	if (!LiveSession)
	{
		UE_LOG_ONLINE(Warning, TEXT("Destroying a session with a null Live MultiplayerSession (%s)"), *SessionName.ToString());
		RemoveNamedSession(SessionName);
		LiveSubsystem->ExecuteNextTick([this, SessionName, CompletionDelegate]()
		{
			CompletionDelegate.ExecuteIfBound(SessionName, true);
			TriggerOnDestroySessionCompleteDelegates(SessionName, true);
		});
		return true;
	}

	FSessionMessageRouterPtr SessionRouter = LiveSubsystem->GetSessionMessageRouter();
	if (SessionRouter.IsValid())
	{
		SessionRouter->ClearOnSessionChangedDelegate(OnSessionChangedDelegate, LiveSession->SessionReference);;
	}
	if (IsConsoleHost(LiveSession))
	{
		LiveSubsystem->GetMatchmakingInterfaceLive()->RemoveMatchmakingTicket(Session->SessionName);
	}

	SecureDeviceAssociation^ Association = LiveSessionInfo->GetAssociation();
	if (Association)
	{
		Association->DestroyAsync();
	}
	LiveSessionInfo->SetAssociation(nullptr);

	LiveSubsystem->ExecuteNextTick([this, SessionName, LiveSession, CompletionDelegate]
	{
		// Start a task to Leave() the first local user found in the session.
		// The tasks will chain until every local user is removed.
		CreateDestroyTask(SessionName, LiveSession, LiveSubsystem, true, CompletionDelegate);
	});
	return true;
}

TSharedPtr<const FUniqueNetId> FOnlineSessionLive::CreateSessionIdFromString(const FString& SessionIdStr)
{
	TSharedPtr<const FUniqueNetId> SessionId;
	if (!SessionIdStr.IsEmpty())
	{
		SessionId = MakeShared<FUniqueNetIdString>(SessionIdStr, LIVE_SUBSYSTEM);
	}
	return SessionId;
}

FNamedOnlineSession* FOnlineSessionLive::GetNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (FNamedOnlineSession& Session : Sessions)
	{
		if (Session.SessionName == SessionName)
		{
			return &Session;
		}
	}
	return nullptr;
}

void FOnlineSessionLive::RemoveNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			UE_LOG_ONLINE(Log, TEXT("Removing Session %s"), *SessionName.ToString());
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

bool FOnlineSessionLive::HasPresenceSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (const FNamedOnlineSession& Session : Sessions)
	{
		if (Session.SessionSettings.bUsesPresence)
		{
			return true;
		}
	}

	return false;
}

class FNamedOnlineSession* FOnlineSessionLive::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
}

class FNamedOnlineSession* FOnlineSessionLive::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions) FNamedOnlineSession(SessionName, Session);
}

EOnlineSessionState::Type FOnlineSessionLive::GetSessionState(FName SessionName) const
{
	FScopeLock ScopeLock(&SessionLock);
	for (const FNamedOnlineSession& Session : Sessions)
	{
		if (Session.SessionName == SessionName)
		{
			return Session.SessionState;
		}
	}

	return EOnlineSessionState::NoSession;
}

IAsyncOperation<MultiplayerSession^>^ FOnlineSessionLive::CreateSessionOperation(
	const int32 UserIndex,
	const FOnlineSessionSettings& SessionSettings,
	const FString& Keyword, const FString& SessionTemplateName)
{
	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(UserIndex);

	return InternalCreateSessionOperation(LiveContext, SessionSettings, Keyword, SessionTemplateName);
}

IAsyncOperation<MultiplayerSession^>^ FOnlineSessionLive::CreateSessionOperation(
	const FUniqueNetId& UserId,
	const FOnlineSessionSettings& SessionSettings,
	const FString& Keyword, const FString& SessionTemplateName)
{
	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(UserId);

	return InternalCreateSessionOperation(LiveContext, SessionSettings, Keyword, SessionTemplateName);
}

IAsyncOperation<MultiplayerSession^>^ FOnlineSessionLive::InternalCreateSessionOperation(
	XboxLiveContext^ LiveContext,
	const FOnlineSessionSettings& SessionSettings,
	const FString& Keyword, const FString& SessionTemplateName)
{
	if (!LiveContext)
	{
		return nullptr;
	}

	GUID NewGUID;
	CoCreateGuid(&NewGUID);
	Platform::Guid SessionGuidName = Platform::Guid(NewGUID);
	Platform::String^ UniqueSessionName = LiveSubsystem->RemoveBracesFromGuidString(SessionGuidName.ToString());

	FString TemplateNameString;
	const FOnlineSessionSetting* TemplateNameSetting = SessionSettings.Settings.Find(SETTING_SESSION_TEMPLATE_NAME);
	if (TemplateNameSetting)
	{
		TemplateNameSetting->Data.GetValue(TemplateNameString);
	}

	MultiplayerSessionReference^ SessionRef = ref new MultiplayerSessionReference(
		// @ATG_CHANGE : BEGIN UWP LIVE support - don't use the console-specific path for getting config ID
		LiveContext->AppConfig->ServiceConfigurationId,
		// @ATG_CHANGE : END
		ref new Platform::String(*SessionTemplateName),
		UniqueSessionName);

	IVector<Platform::String^>^ InitiatorXboxUserIds = ref new Platform::Collections::Vector<Platform::String^>;
	InitiatorXboxUserIds->Append(LiveContext->User->XboxUserId);

	Platform::String^ CustomConstantsJson = ref new Platform::String(L"{}");
	const FOnlineSessionSetting* CustomJsonSetting = SessionSettings.Settings.Find(SETTING_CUSTOM);
	if (CustomJsonSetting)
	{
		FString InJson;

		CustomJsonSetting->Data.GetValue(InJson);

		if (InJson.Len())
		{
			CustomConstantsJson = ref new Platform::String(*InJson);
		}
	}

	// Create the session object
	// @ATG_CHANGE : BEGIN UWP LIVE support - remove deprecated bool parameter (only present on Xbox anyway)
	MultiplayerSession^ LiveSession = ref new MultiplayerSession(
		LiveContext, SessionRef,
		0, // 0 will use the number of players from the session template in the Xbox Developers Portal service config
		MultiplayerSessionVisibility::Any,
		InitiatorXboxUserIds->GetView(), CustomConstantsJson);
	// @ATG_CHANGE : END

	Platform::String^ PlayerCustomConstantBlob = nullptr;
	FString Key = FString::Printf(TEXT("%s%s"), SETTING_SESSION_MEMBER_CONSTANT_CUSTOM_JSON_XUID_PREFIX, LiveContext->User->XboxUserId->Data());

	// Add keyword
	if (!Keyword.IsEmpty())
	{
		Platform::Collections::Vector<Platform::String^>^ Keywords = ref new Platform::Collections::Vector<Platform::String^>();
		Keywords->Append(ref new Platform::String(*Keyword));
		LiveSession->SessionProperties->Keywords = Keywords->GetView();
	}

	// Get our session's join/read restriction from our settings
	const MultiplayerSessionRestriction SessionRestriction = GetLiveSessionRestrictionFromSettings(SessionSettings);
	LiveSession->SessionProperties->JoinRestriction = SessionRestriction;
	LiveSession->SessionProperties->ReadRestriction = SessionRestriction;

	const FOnlineSessionSetting* CurrentPlayerConstantCustomJsonSetting = SessionSettings.Settings.Find(FName(*Key));
	if (CurrentPlayerConstantCustomJsonSetting)
	{
		FString CurrentPlayerConstantCustomJson;
		CurrentPlayerConstantCustomJsonSetting->Data.GetValue(CurrentPlayerConstantCustomJson);
		PlayerCustomConstantBlob = ref new Platform::String(*CurrentPlayerConstantCustomJson);
	}

	// Set current user to be active and joined
	// @ATG_CHANGE : BEGIN UWP LIVE support
	LiveSession->Join(PlayerCustomConstantBlob, true, false);
	// @ATG_CHANGE : END
	LiveSession->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
	LiveSession->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

	// Indicate what events to subscribe to
	LiveSession->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);

	// Add custom settings
	// @ATG_CHANGE : BEGIN UWP LIVE support
	WriteSettingsToLiveJson(SessionSettings, LiveSession, SystemUserFromXSAPIUser(LiveContext->User));
	// @ATG_CHANGE : END UWP LIVE support

	auto writeSessionOp = LiveContext->MultiplayerService->WriteSessionAsync(LiveSession, MultiplayerSessionWriteMode::CreateNew);

	return writeSessionOp;
}

void FOnlineSessionLive::DetermineSessionHost(FName SessionName, MultiplayerSession^ LiveSession)
{
	FNamedOnlineSession* NamedSession = GetNamedSession(SessionName);
	if (!NamedSession)
	{
		return;
	}

	bool bHosting = NamedSession->bHosting;
	int32 HostingPlayerNum = NamedSession->HostingPlayerNum;

	MultiplayerSessionMember^ SessionMember = GetLiveSessionHost(LiveSession);
	if (!SessionMember)
	{
		UE_LOG(LogOnline, Verbose, TEXT("Host not currently set, unable to update host/owner session information"));
		return;
	}

	const TSharedRef<const FUniqueNetId> HostNetId = MakeShared<FUniqueNetIdLive>(SessionMember->XboxUserId);
	if (SessionMember->IsCurrentUser)
	{
		bHosting = true;
		HostingPlayerNum = GetHostingPlayerNum(*HostNetId);
	}

	NamedSession->OwningUserId = HostNetId;
	NamedSession->bHosting = bHosting;
	NamedSession->HostingPlayerNum = HostingPlayerNum;
	UE_LOG(LogOnline, Log, TEXT("Picking %ls as host for session"), SessionMember->XboxUserId->Data());
}

MultiplayerSessionMember^ FOnlineSessionLive::GetMemberFromDeviceToken(MultiplayerSession^ LiveSession, Platform::String^ DeviceToken)
{
	for (MultiplayerSessionMember^ member : LiveSession->Members)
	{
		if (FCString::Stricmp(member->DeviceToken->Data(), DeviceToken->Data()) == 0)
		{
			return member;
		}
	}

	return nullptr;
}

MultiplayerSession^ FOnlineSessionLive::SetHostDeviceTokenSynchronous(int32 UserNum, FName SessionName, MultiplayerSession^ LiveSession,
																	  XboxLiveContext^ Context)
{
	while (LiveSession && LiveSession->SessionProperties->HostDeviceToken == nullptr)
	{
		MultiplayerSessionMember^ CurrentMember = GetCurrentUserFromSession(LiveSession);
		if (CurrentMember == nullptr)
		{
			UE_LOG(LogOnline, Log, TEXT("User in this console is not part of the Game Session, so not attempting to set Host"));
			break;
		}

		LiveSession->SetHostDeviceToken(CurrentMember->DeviceToken);

		//. Assume the host always wants to be active
		SetCurrentUserActive(UserNum, LiveSession, true);

		//. Write the changes
		auto writeSessionOp = Context->MultiplayerService->TryWriteSessionAsync(LiveSession, MultiplayerSessionWriteMode::SynchronizedUpdate);

		Concurrency::create_task(writeSessionOp).then([this,&LiveSession](Concurrency::task<WriteSessionResult^> t)
		{
			try
			{
				WriteSessionResult^ Result = t.get();
				if (Result->Succeeded || Result->Status == WriteSessionStatus::OutOfSync)
				{
					// Either we wrote successfully, or someone else wrote and we got the latest session
					LiveSession = Result->Session;
				}
				else
				{
					LiveSession = nullptr;
					UE_LOG(LogOnline, Log, TEXT("SetHostDeviceTokenSynchronous: TryWriteSessionAsync failed: %ls"), Result->Session->MultiplayerCorrelationId->Data());
				}
			}
			catch (Platform::Exception^ ex)
			{
				LiveSession = nullptr;
				UE_LOG(LogOnline, Log, TEXT("SetHostDeviceTokenSynchronous: TryWriteSessionAsync failed with 0x%0.8X"), ex->HResult);
			}
		}).wait();
	}

	return LiveSession;

}

void FOnlineSessionLive::RegisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = LiveSubsystem->GetVoiceInterface();

	if (VoiceInt.IsValid())
	{
		if (!LiveSubsystem->IsLocalPlayer(PlayerId))
		{
			VoiceInt->RegisterRemoteTalker(PlayerId);
		}
		else
		{
			int32 LocalUserNum =
				LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(PlayerId);
			VoiceInt->RegisterLocalTalker(LocalUserNum);
		}
	}
}

void FOnlineSessionLive::UnregisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = LiveSubsystem->GetVoiceInterface();

	if (VoiceInt.IsValid())
	{
		if (!LiveSubsystem->IsLocalPlayer(PlayerId))
		{
			VoiceInt->UnregisterRemoteTalker(PlayerId);
		}
		else
		{
			const int32 LocalUserNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(PlayerId);
			VoiceInt->UnregisterLocalTalker(LocalUserNum);
		}
	}
}

TSharedRef<FInternetAddr> FOnlineSessionLive::GetAddrFromDeviceAssociation(Windows::Xbox::Networking::ISecureDeviceAssociation^ SDA)
{
	SOCKADDR_STORAGE RemoteSocketAddress;
	Platform::ArrayReference<BYTE> RemoteSocketAddressBytes((BYTE*)&RemoteSocketAddress, sizeof(RemoteSocketAddress));

	SDA->GetRemoteSocketAddressBytes(RemoteSocketAddressBytes);

	// this is utterly evil, but I need the IP address as either a string or a uint32
	// an IPv6 address can't fit into a uint32, so we have to convert it to a string
	// which means doing incredibly specific code that should be wrapped
	sockaddr_in6* In6Addr = (sockaddr_in6*)&RemoteSocketAddress;
	char IPStr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, (void*)&In6Addr->sin6_addr, IPStr, INET6_ADDRSTRLEN);

	ISocketSubsystem* const SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();

	bool bIsValid;
	Addr->SetIp(*FString(IPStr), bIsValid);
	Addr->SetPort(ntohs(In6Addr->sin6_port));

	return Addr;
}

void FOnlineSessionLive::CheckPendingSessionInvite()
{
	// If the title was protocol activated before OSS was initialized (likely at launch), the startup code saved the
	// activation URI.
	// @ATG_CHANGE : BEGIN UWP LIVE support
	const FString ActivationUriString = FPlatformMisc::GetProtocolActivationUri();

	// On Xbox we should have a user by now, but we probably won't on UWP.
	if(!ActivationUriString.IsEmpty() && (PLATFORM_XBOXONE || User::Users->Size > 0))
	{
		Windows::Foundation::Uri^ ActivationUri = ref new Windows::Foundation::Uri(ref new Platform::String(*ActivationUriString));

		// See if this activation was in response to a session invite
		if (SaveInviteFromActivation(ActivationUri))
		{
			FPlatformMisc::SetProtocolActivationUri(FString());
		}
	}
	// @ATG_CHANGE : END	
}

MultiplayerSessionMember^ FOnlineSessionLive::GetLiveSessionHost(MultiplayerSession^ LiveSession)
{
	if (!LiveSession || !LiveSession->SessionProperties)
	{
		return nullptr;
	}

	Platform::String^ HostToken = LiveSession->SessionProperties->HostDeviceToken;
	if (!HostToken)
	{
		return nullptr;
	}

	for (MultiplayerSessionMember^ Member : LiveSession->Members)
	{
		if (Member->DeviceToken == HostToken)
		{
			return Member;
		}
	}

	return nullptr;
}

FOnlineSessionSearchResult FOnlineSessionLive::CreateSearchResultFromSession(MultiplayerSession^ LiveSession, Platform::String^ HostDisplayName, XboxLiveContext^ LiveContext)
{
	FOnlineSessionSearchResult NewSearchResult;
	NewSearchResult.Session.SessionInfo = MakeShared<FOnlineSessionInfoLive>(LiveSession);

	if (HostDisplayName)
	{
		NewSearchResult.Session.OwningUserName = FString(HostDisplayName->Data());
	}

	// Try to find the host.
	MultiplayerSessionMember^ Host = GetLiveSessionHost(LiveSession);
	if (Host)
	{
		NewSearchResult.Session.OwningUserId = MakeShared<FUniqueNetIdLive>(Host->XboxUserId);
	}
	else
	{
		NewSearchResult.Session.OwningUserName = FString(LiveSession->SessionReference->SessionName->Data());
	}

	ReadSettingsFromLiveJson(LiveSession, NewSearchResult.Session, LiveContext);

	DebugLogLiveSession(LiveSession);

	// Find number of open slots.
	int32 MaxSlots = LiveSession->SessionConstants->MaxMembersInSession;
	int32 FilledSlots = LiveSession->Members->Size;
	int32 OpenSlots = MaxSlots - FilledSlots;
	NewSearchResult.Session.NumOpenPrivateConnections = 0;
	NewSearchResult.Session.NumOpenPublicConnections = OpenSlots;
	NewSearchResult.Session.SessionSettings.NumPublicConnections = MaxSlots;
	NewSearchResult.Session.SessionSettings.bAllowJoinInProgress = !LiveSession->SessionProperties->Closed;

	return NewSearchResult;
}

void FOnlineSessionLive::SaveSessionInvite(User^ AcceptingUser, Platform::String^ SessionHandle)
{
	check(AcceptingUser != nullptr);
	check (SessionHandle != nullptr);

	// Set the invite data on the game thread since that's where it will be consumed
	LiveSubsystem->ExecuteNextTick(	[this, AcceptingUser, SessionHandle]
	{
		PendingInvite.AcceptingUser = AcceptingUser;
		PendingInvite.SessionHandle = SessionHandle;
		PendingInvite.bHaveInvite = true;
	});
}

bool FOnlineSessionLive::SaveInviteFromActivation(Windows::Foundation::Uri^ ActivationUri)
{
	Platform::String^ SessionHandle = nullptr;
	Platform::String^ UserXuid = nullptr;

	if (ActivationUri->Host == "inviteHandleAccept")
	{
		SessionHandle = ActivationUri->QueryParsed->GetFirstValueByName("handle");
		UserXuid = ActivationUri->QueryParsed->GetFirstValueByName("invitedXuid");
	}
	else if (ActivationUri->Host == "activityHandleJoin")
	{
		SessionHandle = ActivationUri->QueryParsed->GetFirstValueByName("handle");
		UserXuid = ActivationUri->QueryParsed->GetFirstValueByName("joinerXuid");
	}
	else
	{
		return false;
	}

	if (SessionHandle != nullptr && UserXuid != nullptr)
	{
		// Find the user the invite is for.
		// For the sake of centralizing access to Users, grab the cached list from the identity interface
		FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();

		User^ JoiningUser = nullptr;
		IVectorView<User^>^ CachedUsers = Identity->GetCachedUsers();
		for (User^ CurrentUser : CachedUsers)
		{
			if (CurrentUser->XboxUserId == UserXuid)
			{
				JoiningUser = CurrentUser;
				break;
			}
		}

		if (!JoiningUser)
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::SaveInviteFromActivation: couldn't find a local user to accept the invite."));
			return true;
		}

		// Technically, SaveSessionInvite takes the user that accepted the invite, while we're giving it the
		// user the invite was sent to. These may not match if multiple people are signed in and the wrong
		// one acts on the invite toast. Currently we expect the game to detect and react appropriately to this
		// case, if it cares. We may want to do something better here.
		// If the user joined from a gamercard, this shouldn't be an issue.
		SaveSessionInvite(JoiningUser, SessionHandle);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::SaveInviteFromActivation: couldn't save invite, missing required info Handle=[%ls] UserXuid=[%ls]"),
			SessionHandle == nullptr ? L"INVALID" : SessionHandle->Data(),
			UserXuid == nullptr ? L"INVALID" : UserXuid->Data());
	}
	return true;
}

void FOnlineSessionLive::OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ EventArgs)
{
	if (EventArgs->Kind == Windows::ApplicationModel::Activation::ActivationKind::Protocol)
	{
		ProtocolActivatedEventArgs^ ProtocolArgs = static_cast<ProtocolActivatedEventArgs^>(EventArgs);
		Windows::Foundation::Uri^ ActivationUri = ref new Windows::Foundation::Uri(ProtocolArgs->Uri->RawUri);

		UE_LOG_ONLINE(Log, TEXT("----- Got activation URI: %ls"), ActivationUri->AbsoluteUri->Data());

		// See if this activation was in response to a session invite or gamercard join
		SaveInviteFromActivation(ActivationUri);
	}
}

bool FOnlineSessionLive::IsConsoleHost(MultiplayerSession^ LiveSession)
{
	if (LiveSession == nullptr)
	{
		return false;
	}

	MultiplayerSessionMember^ HostMember = GetLiveSessionHost(LiveSession);
	if (!HostMember)
	{
		return false;
	}

	for (MultiplayerSessionMember^ Member : LiveSession->Members)
	{
		if (Member && Member->IsCurrentUser && Member->DeviceToken == HostMember->DeviceToken)
		{
			return true;
		}
	}

	return false;
}

void FOnlineSessionLive::Tick(float DeltaTime)
{
	TickPendingInvites(DeltaTime);
}

void FOnlineSessionLive::TickPendingInvites(float DeltaTime)
{
	if (bIsDestroyingSessions)
	{
		// Don't accept invites while we're destroying all of our sessions
		return;
	}

	if (LiveSubsystem->ConvertedNetworkConnectivityLevel != EOnlineServerConnectionStatus::Connected)
	{
		// Don't process invites until we're fully connected
		return;
	}

	if (!PendingInvite.bHaveInvite)
	{
		return;
	}

	if (PendingInvite.AcceptingUser == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: bHaveInvite is true but AcceptingUser is null."));
		PendingInvite.bHaveInvite = false;
		return;
	}

	if (PendingInvite.SessionHandle == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: bHaveInvite is true but SessionHandle is null."));
		PendingInvite.bHaveInvite = false;
		return;
	}

	const FPlatformUserId AcceptingUserIndex = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromXboxUser(PendingInvite.AcceptingUser);
	if (AcceptingUserIndex < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: bHaveInvite is true but unknown player %s."), PendingInvite.AcceptingUser->XboxUserId->Data());
		PendingInvite.bHaveInvite = false;
		return;
	}

	XboxLiveContext^ Context = LiveSubsystem->GetLiveContext(PendingInvite.AcceptingUser);
	if (!Context)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: couldn't create an XboxLiveContext for the AcceptingUser."));
		return;
	}

	const FUniqueNetIdLive UniqueNetId(PendingInvite.AcceptingUser->XboxUserId);
	const FString SessionInviteHandle(PendingInvite.SessionHandle->Data());

	auto AsyncOp = Context->MultiplayerService->GetCurrentSessionByHandleAsync(PendingInvite.SessionHandle);
	Concurrency::create_task(AsyncOp).then([this, AcceptingUserIndex, UniqueNetId, SessionInviteHandle](Concurrency::task<MultiplayerSession^> SessionTask)
	{
		try
		{
			MultiplayerSession^ LiveSession = SessionTask.get();
			if (LiveSession == nullptr)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: Failed to get session from invite"));
				return;
			}

			LiveSubsystem->ExecuteNextTick([this, LiveSession, AcceptingUserIndex, UniqueNetId, SessionInviteHandle]()
			{
				FOnlineSessionSearchResult SearchResult = CreateSearchResultFromSession(LiveSession, nullptr);
				TSharedPtr<FOnlineSessionInfoLive> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SearchResult.Session.SessionInfo);
				if (SessionInfo.IsValid())
				{
					SessionInfo->SetSessionInviteHandle(SessionInviteHandle);
				}

				TSharedRef<const FUniqueNetId> UniqueNetIdRef(MakeShared<FUniqueNetIdLive>(UniqueNetId));

				TriggerOnSessionUserInviteAcceptedDelegates(true, AcceptingUserIndex, UniqueNetIdRef, SearchResult);
			});
		}
		catch (Platform::COMException^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::TickPendingInvites: Error getting game session by handle: 0x%0.8x"), Ex->HResult);
			LiveSubsystem->ExecuteNextTick([this, AcceptingUserIndex, UniqueNetId]()
			{
				TSharedRef<const FUniqueNetId> UniqueNetIdRef(MakeShared<FUniqueNetIdLive>(UniqueNetId));
				const FOnlineSessionSearchResult EmptyResult;
				TriggerOnSessionUserInviteAcceptedDelegates(false, AcceptingUserIndex, UniqueNetIdRef, EmptyResult);
			});
		}
	});

	PendingInvite.bHaveInvite = false;
	PendingInvite.AcceptingUser = nullptr;
	PendingInvite.SessionHandle = nullptr;
}

void FOnlineSessionLive::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(PlayerId);
	if (LiveContext == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);
	if (NamedSession == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return;
	}

	FOnlineSessionInfoLivePtr LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	if (!LiveSessionInfo.IsValid())
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveRegisterLocalUser>(SessionName, LiveContext, LiveSubsystem, FUniqueNetIdLive(PlayerId), Delegate);
}

void FOnlineSessionLive::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(PlayerId);
	if (LiveContext == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);
	if (NamedSession == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	FOnlineSessionInfoLivePtr LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	if (!LiveSessionInfo.IsValid())
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveUnregisterLocalUser>(SessionName, LiveContext, LiveSubsystem, FUniqueNetIdLive(PlayerId), Delegate);
}

void FOnlineSessionLive::LogAssociationStateChange(SecureDeviceAssociation^ Association, SecureDeviceAssociationStateChangedEventArgs^ EventArgs)
{
	if (EventArgs == nullptr)
	{
		return;
	}

	UE_LOG_ONLINE(Log, TEXT("SecureDeviceAssociation with remote host %s changed from state %s to %s"),
		Association->RemoteHostName->DisplayName->Data(),
		AssociationStateToString(EventArgs->OldState),
		AssociationStateToString(EventArgs->NewState));
}

const TCHAR* FOnlineSessionLive::AssociationStateToString(SecureDeviceAssociationState State)
{
	switch (State)
	{
		case SecureDeviceAssociationState::CreatingInbound:
		{
			return TEXT("CreatingInbound");
		}
		case SecureDeviceAssociationState::CreatingOutbound:
		{
			return TEXT("CreatingOutbound");
		}
		case SecureDeviceAssociationState::Destroyed:
		{
			return TEXT("Destroyed");
		}
		case SecureDeviceAssociationState::DestroyingLocal:
		{
			return TEXT("DestroyingLocal");
		}
		case SecureDeviceAssociationState::DestroyingRemote:
		{
			return TEXT("DestroyingRemote");
		}
		case SecureDeviceAssociationState::Invalid:
		{
			return TEXT("Invalid");
		}
		case SecureDeviceAssociationState::Ready:
		{
			return TEXT("Ready");
		}
	}

	return TEXT("Unknown");
}

bool FOnlineSessionLive::CanUserJoinSession(Windows::Xbox::System::User^ JoiningUser, MultiplayerSession^ LiveSession)
{
	if (JoiningUser == nullptr || LiveSession == nullptr)
	{
		return false;
	}

	if (LiveSession->Members->Size < LiveSession->SessionConstants->MaxMembersInSession)
	{
		return true;
	}

	// Check for a reservation
	for (MultiplayerSessionMember^ CurrentMember : LiveSession->Members)
	{
		if (CurrentMember->XboxUserId == JoiningUser->XboxUserId)
		{
			return true;
		}
	}

	return false;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction FOnlineSessionLive::GetLiveSessionRestrictionFromSettings(const FOnlineSessionSettings& SessionSettings)
{
	if (SessionSettings.bShouldAdvertise)
	{
		// "None" restriction means anyone may interact with this session if there's room, this is the session default
		// @ATG_CHANGE : BEGIN - UWP doesn't allow unrestricted sessions, so clamp here until we get the info from the service to tell if
		// the title has used an openly advertizable session template that's locked to closed platforms, or a non-advertizable template that's
		// open to non-closed platforms
		return MultiplayerSessionRestriction::Followed;
		// @ATG_CHANGE : END
	}

	if (SessionSettings.bAllowJoinViaPresence)
	{
		// "Followed" restriction means anyone who follows a member of this session may interact with this session
		return MultiplayerSessionRestriction::Followed;
	}

	// "Local" restriction means only people who created the session, are on the same console as session members, or
	// those who have been invited may interact with this session
	return MultiplayerSessionRestriction::Local;
}

void FOnlineSessionLive::OnSessionChanged(FName SessionName, MultiplayerSessionChangeTypes Diff)
{
	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);
	MultiplayerSession^ UpdatedLiveSession = nullptr;

	if (NamedSession)
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		check(LiveInfo.IsValid());
		UpdatedLiveSession = LiveInfo->GetLiveMultiplayerSession();
	}

	if ((Diff & MultiplayerSessionChangeTypes::InitializationStateChange) == MultiplayerSessionChangeTypes::InitializationStateChange)
	{
		OnInitializationStateChanged(SessionName);
	}

	if ((Diff & MultiplayerSessionChangeTypes::MemberListChange) == MultiplayerSessionChangeTypes::MemberListChange)
	{
		OnMemberListChanged(UpdatedLiveSession, SessionName);
	}

	if ((Diff & MultiplayerSessionChangeTypes::HostDeviceTokenChange) == MultiplayerSessionChangeTypes::HostDeviceTokenChange)
	{
		if (NamedSession)
		{
			DetermineSessionHost(SessionName, UpdatedLiveSession);
		}
	}

	if ((Diff & MultiplayerSessionChangeTypes::CustomPropertyChange) == MultiplayerSessionChangeTypes::CustomPropertyChange)
	{
		if (NamedSession)
		{
			ReadSettingsFromLiveJson(UpdatedLiveSession, *NamedSession, nullptr);
		}
	}

	if (((Diff & MultiplayerSessionChangeTypes::MemberCustomPropertyChange) == MultiplayerSessionChangeTypes::MemberCustomPropertyChange)
		|| ((Diff & MultiplayerSessionChangeTypes::MemberStatusChange) == MultiplayerSessionChangeTypes::MemberStatusChange))
	{
		if (NamedSession)
		{
			UpdateMatchMembersJson(NamedSession->SessionSettings.Settings, UpdatedLiveSession);
		}
	}

	if (NamedSession)
	{
		NamedSession->SessionSettings.Set(SETTING_CHANGE_NUMBER, static_cast<uint64>(UpdatedLiveSession->ChangeNumber), EOnlineDataAdvertisementType::DontAdvertise);
	}
}

void FOnlineSessionLive::OnInitializationStateChanged(const FName& SessionName)
{
	check(IsInGameThread());

	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnInitializationStateChanged - game thread"));

	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);
	if (NamedSession == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnInitializationStateChanged - session doesn't exist or was destroyed before task ran"));
		return;
	}

	FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	check(LiveInfo.IsValid());

	if (LiveInfo->IsSessionReady())
	{
		return;
	}

	// We only track initialization changes for the user associated with this session instance,
	// not other local users. The use of initialization groups in JoinSessionAsync ensures all
	// local users pass or fail QoS together.
	// Theoretically, this could be wrong if another local user joins the session via
	// matchmaking later on. I'm not sure this is something any game would actually try to
	// do, but it's something to be aware of.
	MultiplayerSessionMember^ SessionMember = LiveInfo->GetLiveMultiplayerSession()->CurrentUser;

	if (!SessionMember->InitializeRequested)
	{
		UE_LOG_ONLINE(Log, TEXT("  QoS not requested for this member, skipping"));
		return;
	}

	if (SessionMember->InitializationFailureCause != MultiplayerMeasurementFailure::None)
	{
		UE_LOG_ONLINE(Log, TEXT("  Qos failed for this member, failure case: %u"), (int)SessionMember->InitializationFailureCause);
		FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
		MatchmakingInterface->SetTicketState(SessionName, EOnlineLiveMatchmakingState::None);
		MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, false);
		return;
	}

	if (SessionMember->InitializationEpisode == 0)
	{
		UE_LOG_ONLINE(Log, TEXT("  QoS succeeded"));

		XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LiveInfo->GetLiveMultiplayerSession());
		LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveGameSessionReady>(LiveSubsystem, LiveContext, NamedSession->SessionName, LiveInfo->GetLiveMultiplayerSessionRef());
		return;
	}

	if (SessionMember->InitializationEpisode == LiveInfo->GetLiveMultiplayerSession()->InitializingEpisode)
	{
		// This member is participating in QoS for this episode
		MultiplayerInitializationStage Stage = LiveInfo->GetLiveMultiplayerSession()->InitializationStage;

		switch (Stage)
		{
			case MultiplayerInitializationStage::None:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = None"));
				break;

			case MultiplayerInitializationStage::Unknown:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Unknown"));
				break;

			case MultiplayerInitializationStage::Joining:
				// Nothing to be done here, just wait for the other devices to finish joining the session
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Joining"));
				break;

			case MultiplayerInitializationStage::Measuring:
			{
				// Title will measure and upload QoS result, service will do the evaluation.
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Measuring"));

				XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LiveInfo->GetLiveMultiplayerSession());

				LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveMeasureAndUploadQos>(this, LiveContext, NamedSession, LiveSubsystem, MAX_RETRIES, QOS_TIMEOUT_MILLISECONDS, QOS_PROBE_COUNT);
				break;
			}

			case MultiplayerInitializationStage::Evaluating:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Evaluating"));
				// @todo Currently the engine supports system-evaluated QoS. Code for title-evaluated
				// QoS would go here.
				break;

			case MultiplayerInitializationStage::Failed:
				{
					// QoS failed for the session overall
					UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Failed"));
					FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
					MatchmakingInterface->SetTicketState(SessionName, EOnlineLiveMatchmakingState::None);
					MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, false);
					break;
				}
			default:
				UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnInitializationStateChanged - Got unexpected InitializationStage: %u"), (int)Stage);
				break;
		}
	}
}

void FOnlineSessionLive::OnMemberListChanged(MultiplayerSession^ LiveSession, const FName& SessionName)
{
	check(IsInGameThread());

	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnMemberListChanged - game thread"));

	FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName);
	if (NamedSession == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnMatchmakingStatusChanged - session doesn't exist or was destroyed before task ran"));
		return;
	}

	UpdateMatchMembersJson(NamedSession->SessionSettings.Settings, LiveSession);

	bool AllowMigration = false;
	if (NamedSession->SessionSettings.Get(SETTING_ALLOW_ARBITER_MIGRATION, AllowMigration) && AllowMigration)
	{
		bool ShouldMigrate = true;
		for (MultiplayerSessionMember^ Member : LiveSession->Members)
		{
			if (LiveSession->SessionProperties->HostDeviceToken == Member->DeviceToken)
			{
				ShouldMigrate = false;
				break;
			}
		}

		if (ShouldMigrate)
		{
			OnHostInvalid(SessionName);
		}
	}

	FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	check(LiveInfo.IsValid());

	// @v2live Should this go inside the MatchmakingState check below?
	if (!NamedSession->OwningUserId.IsValid())
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineSessionLive::OnMemberListChanged: NamedSession->OwningUserId is not set, but the host should be handling this event."));
		return;
	}

	if (!LiveSubsystem->IsLocalPlayer(*NamedSession->OwningUserId))
	{
		return;
	}

	// Don't add players if the game doesn't support join in progress.
	// @todo: figure out how to support non-join-in-progress games!
	//   Idea: expose a function for the game to call that queries available players, reserves them, and pulls them
	//   this way the game has control and can pull players between rounds.
	//	 On the new multiplayer APIs, there are no parties, so this would likely require a second session to hold
	//	 waiting players.
	//   Idea 2: Maybe the engine can detect when the session switches to Pending. Maybe games want more control though
	if (!NamedSession->SessionSettings.bAllowJoinInProgress)
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineSessionLive::OnMemberListChanged: Game is not join in progress, not resubmitting match ticket."));
		return;
	}

	// Cancel the current match ticket and re-advertise with the new number of open slots.
	// If the session isn't doing matchmaking, this is a no-op.
	LiveSubsystem->GetMatchmakingInterfaceLive()->SubmitMatchingTicket(LiveInfo->GetLiveMultiplayerSessionRef(), SessionName, true);
}

void FOnlineSessionLive::OnHostInvalid(const FName& SessionName)
{
	if (FNamedOnlineSession* const NamedSession = GetNamedSession(SessionName))
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			MultiplayerSession^ LiveSession =  LiveInfo->GetLiveMultiplayerSession();
			if (XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(LiveSession))
			{
				LiveSession->SetHostDeviceToken(LiveSession->CurrentUser->DeviceToken);
				auto writeSessionOp = LiveContext->MultiplayerService->TryWriteSessionAsync(
					LiveSession,
					Multiplayer::MultiplayerSessionWriteMode::SynchronizedUpdate);

				Concurrency::create_task(writeSessionOp).then([this,SessionName](Concurrency::task<WriteSessionResult^> t)
				{
					try
					{
						WriteSessionResult^ Result = t.get();
						if (Result->Succeeded || Result->Status == WriteSessionStatus::OutOfSync)
						{
							LiveSubsystem->RefreshLiveInfo(SessionName, Result->Session);

							LiveSubsystem->ExecuteNextTick([this, SessionName, Result]
							{
								DetermineSessionHost(SessionName, Result->Session);
							});
						}
					}
					catch(...)
					{
						// Greedy selection, someone else grabbed it
					}
				});
			}
		}
	}
}

void FOnlineSessionLive::OnSessionNeedsInitialState(FName SessionName)
{
	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnSessionNeedsInitialState"));

	// Sync with the MultiplayerSession's initialization state. Any other processing needed immediately
	// after creating/joining a session can be added here.
	OnInitializationStateChanged(SessionName);
}

/** Detect a loss of connection to the subscription service and exit multiplayer. */
void FOnlineSessionLive::OnMultiplayerSubscriptionsLost()
{
	check(IsInGameThread());

	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnMultiplayerSubscriptionsLost - game thread"));
	UE_LOG_ONLINE(Log, TEXT("  Connection to multiplayer service lost. Destroying session objects."));

	// We were automatically removed from any Live sessions, so clean them up.
	if (!bIsDestroyingSessions && Sessions.Num() > 0)
	{
		bIsDestroyingSessions = true;	// if multiple users lose subscriptions simultaneously, only try to destroy once
		OnSubscriptionLostDestroyCompleteDelegateHandle = AddOnDestroySessionCompleteDelegate_Handle(OnSubscriptionLostDestroyCompleteDelegate);

		FScopeLock Lock(&SessionLock);

		TArray<FNamedOnlineSession> SessionsCopy = Sessions;
		for (const FNamedOnlineSession& CurrentSession : SessionsCopy)
		{
			DestroySession(CurrentSession.SessionName);
		}
	}
}

void FOnlineSessionLive::OnSubscriptionLostDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (Sessions.Num() == 0 || !bWasSuccessful)
	{
		if (Sessions.Num() == 0)
		{
			UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnSubscriptionLostDestroyComplete - all sessions destroyed."));
		}
		else if (!bWasSuccessful)
		{
			// @v2live: We currently give up when this occurs. Is this right?
			UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnSubscriptionLostDestroyComplete - couldn't destroy session %s."), *SessionName.ToString());
		}

		ClearOnDestroySessionCompleteDelegate_Handle(OnSubscriptionLostDestroyCompleteDelegateHandle);
		bIsDestroyingSessions = false;

		// Inform the game of the subscription failure
		FUniqueNetIdLive NetId;	// We don't know the original NetId here, but it shouldn't matter since sessions were destroyed for everyone
		TriggerOnSessionFailureDelegates(NetId, ESessionFailure::ServiceConnectionLost);
	}
}

FNamedOnlineSession* FOnlineSessionLive::GetNamedSessionForLiveSessionRef(MultiplayerSessionReference^ LiveSessionRef)
{
	check(IsInGameThread());

	FScopeLock Lock(&SessionLock);

	for (auto& CurrentSession : Sessions)
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(CurrentSession.SessionInfo);
		if (!LiveInfo.IsValid())
		{
			continue;
		}

		if (LiveInfo->GetLiveMultiplayerSessionRef() &&
			FOnlineSubsystemLive::AreSessionReferencesEqual(LiveInfo->GetLiveMultiplayerSessionRef(), LiveSessionRef))
		{
			return &CurrentSession;
		}
	}

	return nullptr;
}

FDelegateHandle FOnlineSessionLive::AddOnMatchmakingCompleteDelegate_Handle(const FOnMatchmakingCompleteDelegate& Delegate)
{
	return LiveSubsystem->GetMatchmakingInterfaceLive()->AddOnMatchmakingCompleteDelegate_Handle(Delegate);
}

void FOnlineSessionLive::ClearOnMatchmakingCompleteDelegate_Handle(FDelegateHandle& Handle)
{
	LiveSubsystem->GetMatchmakingInterfaceLive()->ClearOnMatchmakingCompleteDelegate_Handle(Handle);
}

void FOnlineSessionLive::TriggerOnMatchmakingCompleteDelegates(FName Param1, bool Param2)
{
	LiveSubsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(Param1, Param2);
}

FDelegateHandle FOnlineSessionLive::AddOnCancelMatchmakingCompleteDelegate_Handle(const FOnCancelMatchmakingCompleteDelegate& Delegate)
{
	return LiveSubsystem->GetMatchmakingInterfaceLive()->AddOnCancelMatchmakingCompleteDelegate_Handle(Delegate);
}

void FOnlineSessionLive::ClearOnCancelMatchmakingCompleteDelegate_Handle(FDelegateHandle& Handle)
{
	LiveSubsystem->GetMatchmakingInterfaceLive()->ClearOnCancelMatchmakingCompleteDelegate_Handle(Handle);
}

void FOnlineSessionLive::TriggerOnCancelMatchmakingCompleteDelegates(FName Param1, bool Param2)
{
	LiveSubsystem->GetMatchmakingInterfaceLive()->TriggerOnCancelMatchmakingCompleteDelegates(Param1, Param2);
}