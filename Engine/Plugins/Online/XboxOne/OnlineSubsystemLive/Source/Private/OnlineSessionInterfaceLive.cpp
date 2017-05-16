// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineMatchmakingInterfaceLive.h"
#include "VoiceInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// @ATG_CHANGE : UWP Live Support - BEGIN
#if PLATFORM_XBOXONE
#include "XboxOnePostApi.h"
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
// @ATG_CHANGE :  UWP LIVE support: Xbox headers to pch

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Social;
using namespace concurrency;

namespace
{
	/** The maximum number of Live sessions to return when searching for orphaned sessions. */
	const int MAX_ORPHANED_SESSIONS_RESULTS = 100;
	const int MAX_RETRIES = 20;

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
			UE_LOG_ONLINE(Log, TEXT("DebugLogLiveSession: Session is null."));
			return;
		}

		UE_LOG_ONLINE(Log, TEXT("DebugLogLiveSession:\n"));
		UE_LOG_ONLINE(Log, TEXT("  MaxMembersInSession: %d\n"), Session->SessionConstants->MaxMembersInSession);
		UE_LOG_ONLINE(Log, TEXT("  Members->Size: %d. Members:\n"), Session->Members->Size);

		for (auto Member : Session->Members)
		{
			UE_LOG_ONLINE(Log,
				TEXT( "    Gamertag: %s, Live ID: %s, status: %s" ),
				Member->Gamertag->Data(), Member->XboxUserId->Data(), GetSessionMemberStatusString(Member->Status));
		}
	}
}

FOnlineSessionLive::FOnlineSessionLive(class FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
	, PeerTemplate(nullptr)
	, bIsDestroyingSessions(false)
{ 
	Initialize(); 
}

FOnlineSessionLive::~FOnlineSessionLive()
{
	if( PeerTemplate )
	{
		PeerTemplate->AssociationIncoming -= TokenSecureAssociationIncoming;
	}

	try
	{
		Windows::Xbox::System::User::SignInCompleted -= SignInCompletedToken;
	}
	catch(Platform::Exception^ )
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

	// Look up the secure device association template name in the engine ini settings.
	if(GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("SecureDeviceAssociationTemplateName"), TemplateName, GEngineIni))
	{
		try
		{
			PeerTemplate = SecureDeviceAssociationTemplate::GetTemplateByName(ref new Platform::String(*TemplateName));
			
			// Listen for Secure Association incoming
			auto AssociationIncomingEvent = ref new TypedEventHandler<SecureDeviceAssociationTemplate^, SecureDeviceAssociationIncomingEventArgs^>(
				[] (Platform::Object^, SecureDeviceAssociationIncomingEventArgs^ EventArgs)
			{
				if(EventArgs->Association)
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
		UE_LOG_ONLINE(Warning, TEXT("No SecureDeviceAssociationTemplateName specified in the engine ini file."));
	}

	// Clean up orphaned sessions.
	// If an online game is ended abruptly due to a network disconnect, crash, or just stopping the debugger,
	// we may end up with stale sessions in the MPSD that have no corresponding game running.
	// According to https://forums.xboxlive.com/AnswerPage.aspx?qid=216d3346-19ba-4bb0-af9e-0b2001fb57fe&tgt=1,
	// games should detect sessions with only the local user in them at startup and call Leave() on those sessions.
	// Also enable multiplayer subscriptions to get session updates.

	// Grab the user list, this is a cross-VM call but since it's only done once at startup the extra milliseconds
	// shouldn't be a big deal.
	auto Users = Windows::Xbox::System::User::Users;

	for(auto CurrentUser : Users)
	{
		CleanUpOrphanedSessions(CurrentUser);

	}
	using namespace Windows::Xbox::System;
	using namespace Windows::Foundation;
	EventHandler<SignInCompletedEventArgs^>^ SignInCompletedEvent = ref new EventHandler<SignInCompletedEventArgs^>(
		[this] (Platform::Object^, SignInCompletedEventArgs^ EventArgs)
	{		
		CleanUpOrphanedSessions( EventArgs->User );
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

	// @ATG_CHANGE : BEGIN - on UWP users might now arrive until later.  Re-check invites at that point.
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
		auto LiveContext = LiveSubsystem->GetLiveContext(User);
		
		// @ATG_CHANGE :  BEGIN UWP LIVE support
		auto SessionsRequest = ref new MultiplayerGetSessionsRequest(LiveContext->AppConfig->ServiceConfigurationId, MAX_ORPHANED_SESSIONS_RESULTS);
		SessionsRequest->IncludePrivateSessions = true;
		SessionsRequest->IncludeReservations = true;
		SessionsRequest->IncludeInactiveSessions = true;
		SessionsRequest->XboxUserIdFilter = User->XboxUserId;
		SessionsRequest->VisibilityFilter = MultiplayerSessionVisibility::Any;

		auto AsyncGetOp = LiveContext->MultiplayerService->GetSessionsAsync(SessionsRequest);
		// @ATG_CHANGE :  END

		create_task(AsyncGetOp).then([User, LiveContext](task<IVectorView<MultiplayerSessionStates^>^> Task)
		{
			try
			{
				auto Results = Task.get();

				UE_LOG_ONLINE(Log, TEXT("Found %d potentially orphaned sessions."), Results->Size);

				for(auto SessionState : Results)
				{
					UE_LOG_ONLINE(Log, TEXT("Potentially orphaned session:"));
					UE_LOG_ONLINE(Log, TEXT("  Template name: %s"), SessionState->SessionReference->SessionTemplateName->Data());
					UE_LOG_ONLINE(Log, TEXT("  Id: %s"), SessionState->SessionReference->SessionName->Data());
					UE_LOG_ONLINE(Log, TEXT("  State: %s"), SessionState->Status.ToString()->Data());
					
					auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionState->SessionReference);
					
					create_task(GetSessionOp).then([User, LiveContext, SessionState](task<MultiplayerSession^> SessionTask)
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
									create_task(WriteOp).then([Session](task<MultiplayerSession^> WriteTask)
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

bool FOnlineSessionLive::CreateSession(int32 HostingPlayerControllerIndex, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(HostingPlayerControllerIndex);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Log, L"Couldn't find unique id for HostingPlayerNum %d", HostingPlayerControllerIndex);
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
	Session->LocalOwnerId = MakeShareable(new FUniqueNetIdLive(HostingPlayerId));

	FString TemplateNameString;
	const FOnlineSessionSetting* TemplateNameSetting = NewSessionSettings.Settings.Find( SETTING_SESSION_TEMPLATE_NAME );
	if ( TemplateNameSetting )
	{
		TemplateNameSetting->Data.GetValue(TemplateNameString);
	}

	Windows::Xbox::System::User^ CreatingUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(FUniqueNetIdLive(HostingPlayerId));

	FString Keyword;
	NewSessionSettings.Get(SEARCH_KEYWORDS, Keyword);

	try
	{
		auto writeSessionOp = CreateSessionOperation(HostingPlayerId, NewSessionSettings, Keyword, TemplateNameString);
		if(!writeSessionOp)
		{
			UE_LOG(LogOnline, Log, TEXT("Failed to create async create session operation"));
			RemoveNamedSession(SessionName);
			TriggerOnCreateSessionCompleteDelegates(SessionName, false);
			return false;
		}

		// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
		create_task(writeSessionOp).then([this,CreatingUser,SessionName,NewSessionSettings](task<MultiplayerSession^> CreateTask)
		// @ATG_CHANGE :  END
		{    
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession = nullptr;
			try 
			{
				LiveSession = CreateTask.get(); // if t.get() didn't throw, it succeeded

				LiveSubsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(OnSessionChangedDelegate, LiveSession->SessionReference);
				// Now that the session is created, we can get the device token and set this console as the host.
				MultiplayerSessionMember^ HostMember = nullptr;
				for (auto Member : LiveSession->Members)
				{
					if(Member->XboxUserId == CreatingUser->XboxUserId)
					{
						HostMember = Member;
					}
				}

				TSharedPtr<const FUniqueNetId> CreatingUserUniqueId = MakeShareable(new FUniqueNetIdLive(CreatingUser->XboxUserId));

				if(HostMember == nullptr)
				{
					UE_LOG_ONLINE(Warning, TEXT("Could not find creator in session members. Not setting host."));
					
					auto NewTask = new FOnlineAsyncTaskLiveCreateSession(
						this,
						CreatingUserUniqueId,
						SessionName,
						LiveSession);
					LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewTask);
					return;
				}

				// Simple host selection - the user that creates the session is the host.
				LiveSession->SetHostDeviceToken(HostMember->DeviceToken);

				// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
				// Now that the session is created its constants should be fully initialized - as such
				// it's now safe to rely on constants to help determine joinability.
				WriteSessionPrivacySettingsToLiveJson(NewSessionSettings, LiveSession);
				// @ATG_CHANGE :  END

				XboxLiveContext^ Context = LiveSubsystem->GetLiveContext(CreatingUser);
				
				// This will be the session used for invites/join in progress if supported.
				Context->MultiplayerService->SetActivityAsync(LiveSession->SessionReference);

				auto WriteSessionOp = Context->MultiplayerService->WriteSessionAsync(
					LiveSession,
					MultiplayerSessionWriteMode::UpdateExisting);

				create_task(WriteSessionOp).then([this, CreatingUserUniqueId,SessionName, LiveSession](task<MultiplayerSession^> WriteTask)
				{
					try
					{
						auto NewSession = WriteTask.get(); // if t.get() didn't throw, it succeeded
						
						auto NewTask = new FOnlineAsyncTaskLiveCreateSession(
							this,
							CreatingUserUniqueId,
							SessionName,
							NewSession);
						LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewTask);
					}
					catch ( Platform::COMException^ ex )
					{
						UE_LOG_ONLINE(Warning, TEXT("WriteSessionAsync failed attempting to write host device token."));
						
						LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, SessionName, LiveSession]()
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
			catch (Platform::Exception^ ex)
			{
				UE_LOG(LogOnline, Log, TEXT("Create Session Task failed with 0x%0.8X"), ex->HResult);
	
				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, SessionName, LiveSession]()
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
	catch(Platform::Exception^ ex)
	{
		UE_LOG(LogOnline, Log, L"Create Session Task failed with 0x%0.8X", ex->HResult);
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
		if(MaxResult == 0)
		{
			// Default value is 100, this is arbitrary
			MaxResult = 100;
		}
		SearchSettings->QuerySettings.Get(SETTING_CONTRACT_VERSION_FILTER,	ContractVersionFilter);
		SearchSettings->QuerySettings.Get(SETTING_FIND_PRIVATE_SESSIONS,	IncludePrivateSessions);
		SearchSettings->QuerySettings.Get(SETTING_FIND_RESERVED_SESSIONS,	IncludeReservations);
		SearchSettings->QuerySettings.Get(SETTING_FIND_INACTIVE_SESSIONS,	IncludeInactiveSessions);
		SearchSettings->QuerySettings.Get(SETTING_MULTIPLAYER_VISIBILITY,	MultiplayerVisibility);

		String ^sessionTemplateNameFilter	= ref new Platform::String( *InGameType );
		String ^xboxUserIdFilter			= ref new Platform::String( *InUser );
		String ^keywordFilter				= ref new Platform::String( *InKeywords );

		try
		{
			XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(SearchingPlayerId);
			if (LiveContext == nullptr)
			{
				SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
				TriggerOnFindSessionsCompleteDelegates(false);
				return false;
			}

			// @ATG_CHANGE :  BEGIN UWP LIVE support
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
			// @ATG_CHANGE :  END

			create_task(SearchOp)
				.then( [this,SearchSettings,LiveContext] (task<IVectorView<MultiplayerSessionStates^>^> SearchTask)
			{
				try
				{
					auto SearchResults = SearchTask.get(); // if t.get() didn't throw, it succeeded
					ExpectedResults = SearchResults->Size;

					if (ExpectedResults == 0)
					{
						// Finish on the Game thread
						LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, SearchSettings]()
						{
							SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
							CurrentSessionSearch = nullptr;
							TriggerOnFindSessionsCompleteDelegates(true);  
						});
						return;
					}
			
					for(auto SessionState : SearchResults)
					{
						if (SessionState->SessionReference != nullptr)
						{
							auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionState->SessionReference);
							create_task(GetSessionOp)
								.then( [this,SearchSettings,LiveContext] (task<MultiplayerSession^> t)
							{
								// Lock for the entirety of this scope to protect safe access to SearchSettings' SearchResults and ExpectedResults
								FScopeLock Lock(&SessionResultLock);
								try 
								{
									MultiplayerSession^ SearchResult = t.get();
									if (SearchResult)
									{										
										String^ HostDisplayName = ref new String(TEXT("Unknown host"));
										auto HostMember = GetLiveSessionHost(SearchResult);
										if(HostMember)
										{
												// XR-46 permits the use of Gamertag here.
												HostDisplayName = HostMember->Gamertag;
										}     
										auto NewSearchResult = CreateSearchResultFromSession(SearchResult, HostDisplayName, LiveContext);
										SearchSettings->SearchResults.Add(NewSearchResult);
									}
								}
								catch (Platform::Exception^ ex)
								{
									UE_LOG(LogOnline, Log,TEXT("A MultiplayerService::GetCurrentSessionAsync call failed with 0x%0.8X"), ex->HResult);
								}

								ExpectedResults--;

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
							ExpectedResults--;

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
					
					LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this,SearchSettings]()
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

bool FOnlineSessionLive::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegates)
{
	FOnlineSessionSearchResult EmptyResult;
	CompletionDelegates.ExecuteIfBound(0, false, EmptyResult);
	return true;
}

FOnlineSessionSearchResult* ResultFromSDA(String^ SDABase64, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	for (FOnlineSessionSearchResult& SearchResult : SearchSettings->SearchResults)
	{
		FOnlineSessionInfoLive* LiveSessionInfo = static_cast<FOnlineSessionInfoLive*>(SearchResult.Session.SessionInfo.Get());
		for (auto Member : LiveSessionInfo->GetLiveMultiplayerSession()->Members)
		{
			if (Member->SecureDeviceAddressBase64 == SDABase64)
				return &SearchResult;
		}
	}
	return nullptr;
}
void FOnlineSessionLive::PingResultsAndTriggerDelegates(const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{ 
	auto Addresses = ref new Vector<SecureDeviceAddress^>();
	auto Metrics = ref new Vector<QualityOfServiceMetric>();
	Metrics->Append(QualityOfServiceMetric::LatencyAverage);
	
	for (auto SearchResult : SearchSettings->SearchResults)
	{
		FOnlineSessionInfoLive* LiveSessionInfo = static_cast<FOnlineSessionInfoLive*>(SearchResult.Session.SessionInfo.Get());
		auto Host = GetLiveSessionHost(LiveSessionInfo->GetLiveMultiplayerSession());
		if (nullptr == Host) //Non Thunderhead dedicated servers need to manually ping the result here...
		{
			continue;
		}

		String^ HostSDABase64 = Host->SecureDeviceAddressBase64;
		if (nullptr == HostSDABase64)
		{
			continue;
		}
		
		auto SDA = SecureDeviceAddress::FromBase64String(HostSDABase64);
		if (nullptr == SDA) //Non Thunderhead dedicated servers need to manually ping the result here...
		{
			continue;
		}

		UE_LOG(LogOnline, Log, TEXT("Measuring Address: %s"), HostSDABase64->Data());
		Addresses->Append(SDA);
	}

	if (Addresses->Size > 0)
	{
		create_task(
			QualityOfService::MeasureQualityOfServiceAsync(
			Addresses, 
			Metrics, 
			QOS_TIMEOUT_MILLISECONDS,  
			QOS_PROBE_COUNT               
			))
			.then([this, SearchSettings](task<MeasureQualityOfServiceResult^> Task)
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

			LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this,SearchSettings]()
			{
				SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
				CurrentSessionSearch = nullptr;
				TriggerOnFindSessionsCompleteDelegates(true); 
			});
		});
	}
	else
	{
		LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this,SearchSettings]()
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
			CurrentSessionSearch = nullptr;
			TriggerOnFindSessionsCompleteDelegates(true); 
		});
	}
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
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	return JoinSession(*UniqueId, SessionName, DesiredSession);
}

bool FOnlineSessionLive::JoinSession(const FUniqueNetId& UserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	bool bRetVal = true;

	// work out if we're already in the session or not
	auto NamedSession = GetNamedSession(SessionName);

	if(NamedSession)
	{
		UE_LOG_ONLINE(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		return false;
	}

	// If there's no secure device association template, we can't get the host's address.
	if(!PeerTemplate)
	{
		UE_LOG_ONLINE(Warning, TEXT("No secure device association template, unable to join host."));
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
		return false;
	}

	// Check for a Join from URI (Invites, Join In Progress from Matchmade Sessions)
	FString SessionURI;
	if (DesiredSession.Session.SessionSettings.Get(SETTING_GAME_SESSION_URI, SessionURI))
	{
		NamedSession = AddNamedSession(SessionName, DesiredSession.Session.SessionSettings);
		FString SessionTemplateName;
		DesiredSession.Session.SessionSettings.Get(SETTING_SESSION_TEMPLATE_NAME, SessionTemplateName);

		auto SessionReference = Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference::ParseFromUriPath(ref new String(*SessionURI));

		SessionReference->ParseFromUriPath(ref new String(*SessionURI));

		if (auto LiveContext = LiveSubsystem->GetLiveContext(UserId))
		{
			FOnlineAsyncTaskLiveJoinSession* Task =
				new FOnlineAsyncTaskLiveJoinSession( this,
				SessionReference,
				PeerTemplate,
				LiveContext,
				NamedSession,
				LiveSubsystem,
				MAX_RETRIES,
				true);
			LiveSubsystem->QueueAsyncTask(Task);
			//LiveSubsystem->GetAsyncTaskManager()->Add(Task);

			return true;
		}

		RemoveNamedSession(SessionName);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	// Create a named session from the search result data
	NamedSession = AddNamedSession(SessionName, DesiredSession.Session);
	// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
	// Comments and other OSS implementations indicate the on non-host machines HostingPlayerNum 
	// should be the local index of the player that iniated Join...
	NamedSession->HostingPlayerNum = LiveSubsystem->GetIdentityLive()->GetControllerIndexForId(UserId);
	// @ATG_CHANGE :  END
	NamedSession->LocalOwnerId = MakeShareable(new FUniqueNetIdLive(UserId));

	if(!DesiredSession.Session.SessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid session info on search result"));
		RemoveNamedSession(SessionName);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	const FOnlineSessionInfoLive* SearchSessionInfo = static_cast<FOnlineSessionInfoLive*>(DesiredSession.Session.SessionInfo.Get());

	auto LiveSession = SearchSessionInfo->GetLiveMultiplayerSession();
	auto LiveContext = LiveSubsystem->GetLiveContext(UserId);
	//Protect against signout
	if (LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid session info on search result"));
		RemoveNamedSession(SessionName);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	FOnlineAsyncTaskLiveJoinSession* Task =
		new FOnlineAsyncTaskLiveJoinSession( this,
											 LiveSession->SessionReference,
											 PeerTemplate,
											 LiveContext,
											 NamedSession,
											 LiveSubsystem,
											 MAX_RETRIES );
	
	// Ensure this finishes before session notifications are processed so session is initialized
	LiveSubsystem->QueueAsyncTask(Task);
	
	return true;
}

MultiplayerSessionMember^ FOnlineSessionLive::GetCurrentUserFromSession(MultiplayerSession^ LiveSession)
{
	for (auto Member : LiveSession->Members)
	{
		if(Member->IsCurrentUser)
		{
			return Member;
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// This API returns the "Advertised Session" which my friend is in, not all sessions he is in.
//-----------------------------------------------------------------------------

bool FOnlineSessionLive::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	try
	{
		Platform::Collections::Vector<Platform::String^>^ FriendVector = ref new Platform::Collections::Vector<Platform::String^>;
		FriendVector->Append(ref new Platform::String( *Friend.ToString() ));

		auto LiveContext = LiveSubsystem->GetLiveContext(LocalUserNum);
		if (LiveContext == nullptr)
		{
			FOnlineSessionSearchResult FriendSession;
			TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, FriendSession);
			return false;
		}

		// @ATG_CHANGE :  BEGIN UWP LIVE support
		auto GetActivitiesOp =
			LiveContext->MultiplayerService->GetActivitiesForUsersAsync( LiveContext->AppConfig->ServiceConfigurationId,
																		 FriendVector->GetView() );
		// @ATG_CHANGE :  END

		create_task(GetActivitiesOp)
			.then([LiveContext](task<IVectorView<MultiplayerActivityDetails^> ^> Task)
		{
			try
			{
				IVectorView<MultiplayerActivityDetails^>^ ActivityDetails = Task.get();
				if(ActivityDetails->Size == 0)
				{
					throw ref new Platform::InvalidArgumentException(); //  User does not have any advertisable session, let the exception handler below deal with it.
				}

				return LiveContext->MultiplayerService->GetCurrentSessionAsync( ActivityDetails->GetAt(0)->SessionReference );
			}
			catch(Platform::Exception^ ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to retrieve Friend's multiplayer activity with 0x%0.8X"), ex->HResult);
				throw;
			}
		})
		.then([this, LocalUserNum, LiveContext](task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> Task)
		{
			try
			{
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ FriendLiveSession = Task.get();				
				String^ HostDisplayName = GetLiveSessionHost(FriendLiveSession)->Gamertag;

				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, LocalUserNum, FriendLiveSession, HostDisplayName]()
				{
					FOnlineSessionSearchResult FriendSession = CreateSearchResultFromSession( FriendLiveSession, HostDisplayName );
					TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, true, FriendSession); 
				});
			}
			catch (Platform::Exception^ ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FindFriendSession: Failed to retrieve MultiplayerSession with 0x%0.8X"), ex->HResult);
				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, LocalUserNum]()
				{
					FOnlineSessionSearchResult FriendSession;
					TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, FriendSession); 
				});
			}
		});
	}
	catch (Platform::Exception^ ex)
	{
		FOnlineSessionSearchResult FriendSession;
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, FriendSession); 
		return false;
	}

	return true;
};

bool FOnlineSessionLive::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	auto ControllerId = LiveSubsystem->GetIdentityLive()->GetControllerIndexForId(LocalUserId);
	if (ControllerId == -1)
	{
		FOnlineSessionSearchResult FriendSession;
			TriggerOnFindFriendSessionCompleteDelegates(-1, false, FriendSession); 
		return false;
	}

	return FindFriendSession(ControllerId, Friend);
};

void FOnlineSessionLive::SetCurrentUserActive(int32 UserNum, MultiplayerSession^ LiveSession, bool bIsActive)
{
	check( LiveSession );

	//. Mark the current user as active, or otherwise
	LiveSession->SetCurrentUserStatus(
		bIsActive ?  
		MultiplayerSessionMemberStatus::Active : 
	MultiplayerSessionMemberStatus::Inactive );
}

/** Get a resolved connection string from a session info */
static bool GetConnectStringFromSessionInfo(TSharedPtr<FOnlineSessionInfoLive>& SessionInfo, FString& ConnectInfo, int32 PortOverride = 0)
{
	bool bSuccess = false;

	if (SessionInfo.IsValid())
	{
		TSharedPtr<FInternetAddr> IpAddr = SessionInfo->GetHostAddr();
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

bool FOnlineSessionLive::GetResolvedConnectString(FName SessionName, FString& ConnectInfo)
{
	bool bSuccess = false;
	// Find the session
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session != NULL)
	{
		TSharedPtr<FOnlineSessionInfoLive> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
		bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
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
		TSharedPtr<FOnlineSessionInfoLive> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(SearchResult.Session.SessionInfo);

		if (PortType == BeaconPort)
		{
			int32 BeaconListenPort = DEFAULT_BEACON_PORT;
			if (!SearchResult.Session.SessionSettings.Get(SETTING_BEACONPORT, BeaconListenPort) || BeaconListenPort <= 0)
			{
				// Reset the default BeaconListenPort back to DEFAULT_BEACON_PORT because the SessionSettings value does not exist or was not valid
				BeaconListenPort = DEFAULT_BEACON_PORT;
			}
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == GamePort)
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

// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
FOnlineSessionSettings* FOnlineSessionLive::GetSessionSettings(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::GetSessionSettings: couldn't find session '%s'"), *SessionName.ToString());
		return nullptr;
	}

	return &Session->SessionSettings;
}
// @ATG_CHANGE :  END

bool FOnlineSessionLive::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdLive(PlayerId)));
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
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdLive(PlayerId)));
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
	auto NamedSession = GetNamedSession(SessionName);

	if(!NamedSession)
	{
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if(bShouldRefreshOnlineData)
	{
		XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(NamedSession->HostingPlayerNum);
		if(LiveContext == nullptr)
		{
			TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
			return false;
		}

		auto UpdateSessionTask = new FOnlineAsyncTaskLiveUpdateSession(SessionName, LiveContext, LiveSubsystem, MAX_RETRIES, UpdatedSessionSettings);
		LiveSubsystem->GetAsyncTaskManager()->AddToParallelTasks(UpdateSessionTask);
	}
	else //Update Player constants/Player group info 
	{
		int NumPendingUpdates = 0;
		auto LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			if (auto Session = LiveInfo->GetLiveMultiplayerSession())
			{
				for (auto Member : LiveInfo->GetLiveMultiplayerSession()->Members)
				{
					if (XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(FUniqueNetIdLive(Member->XboxUserId->Data())))
					{
						FOnlineAsyncTaskLiveUpdateSessionMember* UpdateSessionTask = new FOnlineAsyncTaskLiveUpdateSessionMember(SessionName, LiveContext, LiveSubsystem, 10);
						LiveSubsystem->GetAsyncTaskManager()->AddToParallelTasks(UpdateSessionTask);
						++NumPendingUpdates;
					}
				}
			}
		}

		if (NumPendingUpdates == 0)
		{
			TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		}

		return NumPendingUpdates != 0;
	}

	return true;
}

void FOnlineSessionLive::ReadSettingsFromLiveJson(MultiplayerSession^ LiveSession, FOnlineSession& Session, Microsoft::Xbox::Services::XboxLiveContext^ LiveContext)
{
	String^ PlatformPropertiesJson = LiveSession->SessionProperties->SessionCustomPropertiesJson;
	FString PropertiesJson( PlatformPropertiesJson->Data() );
	TSharedPtr< FJsonObject > JObj;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( PropertiesJson );

	if ( FJsonSerializer::Deserialize(Reader, JObj) && JObj.IsValid() )
	{
		FSessionSettings  NewSettings;

		// Copy existing values that are not service based
		for ( FSessionSettings::TConstIterator It( Session.SessionSettings.Settings ); It; ++It)
		{
			const FName&					SettingName = It.Key();
			const FOnlineSessionSetting&	SettingValue = It.Value();

			if ( SettingValue.AdvertisementType < EOnlineDataAdvertisementType::ViaOnlineService ) 
			{
				NewSettings.Add( SettingName, SettingValue );
			}
		}

		TMap< FString, TSharedPtr<FJsonValue> > JSettings = JObj->Values;

		for ( auto it = JSettings.CreateConstIterator(); it; ++it )
		{
			const FString					JSettingName = it.Key();
			const TSharedPtr<FJsonValue>	JSettingValue = it.Value();

			FOnlineSessionSetting NewSetting;

			// Create setting of matching data type
			switch ( JSettingValue->Type )
			{
				case EJson::Array:
				case EJson::Object:
				case EJson::String:		
					NewSetting = FOnlineSessionSetting( JSettingValue->AsString(), EOnlineDataAdvertisementType::ViaOnlineService ); 
					break;

				case EJson::Number:		
					NewSetting = FOnlineSessionSetting( JSettingValue->AsNumber(), EOnlineDataAdvertisementType::ViaOnlineService );
					break;

				case EJson::Boolean:	
					NewSetting = FOnlineSessionSetting( JSettingValue->AsBool(), EOnlineDataAdvertisementType::ViaOnlineService );
					break;

				default: continue;
			}

			if ( JSettingName == TEXT( "HostXboxUserId" ) )
			{
				if ( LiveContext != nullptr )
				{
					// If we pass in a live context, we are assuming this is on an async task already
					// In this case, look up the name from the json value to override the OwningUserName
					Platform::String^ HostXboxUserId = ref new Platform::String( *NewSetting.Data.ToString() );

					auto ProfileOp = LiveContext->ProfileService->GetUserProfileAsync( HostXboxUserId );

					try
					{
						// We're already in an async callback so it should be OK to block on the get() here.
						auto HostProfile = create_task(ProfileOp).get();
						Session.OwningUserName = FString( HostProfile->GameDisplayName->Data() );
					}
					catch(Platform::COMException^ Ex)
					{
						UE_LOG(LogOnline, Log,TEXT("A ProfileService::GetUserProfileAsync call failed with 0x%0.8X"), Ex->HResult);
					}
				}
			}
			else
			{
				NewSettings.Add( FName( *JSettingName ), NewSetting );
			}
		}

		UpdateMatchMembersJson(NewSettings, LiveSession);

		// Finally, replace existing with new settings (this deletes settings that have been removed)
		Session.SessionSettings.Settings = NewSettings;
	}
}

void FOnlineSessionLive::ExtractJsonMemberSettings(MultiplayerSession^ LiveSession, FString& OutJsonString)
{
	bool NeedComma = false;

	// We could have used JsonWriter here, but it escapes JSON characters, so instead 
	// we just build it manually so that we can pass our snippets of JSON directly

	OutJsonString = FString( TEXT("{\"Members\":[") );

	for ( MultiplayerSessionMember^ member : LiveSession->Members )
	{
		OutJsonString += FString::Printf( TEXT("%s{\"xuid\":\"%s\", \"constants\":%s, \"properties\":%s}"),
					NeedComma ? TEXT(",") : TEXT(""),
					member->XboxUserId->Data(),
					member->MemberCustomConstantsJson->Data(),
					member->MemberCustomPropertiesJson->Data() );	

		NeedComma = true;
	}

	OutJsonString +=  FString( TEXT("]}") );
}

void FOnlineSessionLive::UpdateMatchMembersJson(FSessionSettings& UpdatedSettings, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession)
{
	FString MemberSessionJson;

	ExtractJsonMemberSettings( LiveSession, MemberSessionJson );
	UpdatedSettings.Remove( SETTING_MATCH_MEMBERS_JSON );
	UpdatedSettings.Add( SETTING_MATCH_MEMBERS_JSON, FOnlineSessionSetting( MemberSessionJson, EOnlineDataAdvertisementType::DontAdvertise ) );
}

void FOnlineSessionLive::WriteSettingsToLiveJson(const FOnlineSessionSettings& SessionSettings, MultiplayerSession^ LiveSession, User^ HostUser)
{
	for ( FSessionSettings::TConstIterator It( SessionSettings.Settings ); It; ++It)
	{
		const FName& SettingName = It.Key();
		const FOnlineSessionSetting& SettingValue = It.Value();

		// Only upload values that are marked for service use
		if ( SettingValue.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService )
		{
			Platform::String^ PlatformName  = ref new Platform::String( *SettingName.ToString() );
			Platform::String^ PlatformValue = ref new Platform::String( *SettingValue.Data.ToString() );

			LiveSession->SetSessionCustomPropertyJson( PlatformName, PlatformValue );
		}
	}

	// If we have a host, write the name of the host to the session
	// This is a workaround for the client not having a reliable way to determine who the host is in splitscreen
	if ( HostUser != nullptr )
	{
		// We have to add quotes so it's treated as a string in json
		FString XboxUserId = FString::Printf( TEXT( "\"%s\"" ), *FString( HostUser->XboxUserId->Data() ) );
		
		Platform::String^ XboxUserIdStr = ref new Platform::String( *XboxUserId );

		LiveSession->SetSessionCustomPropertyJson( "HostXboxUserId", XboxUserIdStr );
	}
}

// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
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
// @ATG_CHANGE :  END

bool FOnlineSessionLive::StartSession( FName SessionName )
{
	auto Session = GetNamedSession(SessionName);
	if(!Session)
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
	auto SessionInfoLive = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
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
	if(!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if(Session->SessionState != EOnlineSessionState::InProgress)
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
	// Technically we can't actually destroy a session on Live, all we can do is Leave() it,
	// hope everyone else leaves also, and let it time out.

	auto Session = GetNamedSession(SessionName);
	if(!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// Purposefully skip the delegate call as one should already be in flight
		UE_LOG_ONLINE(Warning, TEXT("Already in process of destroying session (%s)"), *SessionName.ToString());
		return false;
	}

	Session->SessionState = EOnlineSessionState::Destroying;

	auto LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
	
	if(!LiveSessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Destroying an online session (%s) will null Live info. No writes to the MPSD will occur."), *SessionName.ToString());
		RemoveNamedSession(SessionName);
		CompletionDelegate.ExecuteIfBound(SessionName, true);
		TriggerOnDestroySessionCompleteDelegates(SessionName, true);
		return false;
	}
	
	auto LiveSession = LiveSessionInfo->GetLiveMultiplayerSession();

	if(!LiveSession)
	{
		UE_LOG_ONLINE(Warning, TEXT("Destroying a session with a null Live MultiplayerSession (%s)"), *SessionName.ToString());
		RemoveNamedSession(SessionName);
		CompletionDelegate.ExecuteIfBound(SessionName, true);
		TriggerOnDestroySessionCompleteDelegates(SessionName, true);
		return true;
	}

	LiveSubsystem->GetSessionMessageRouter()->ClearOnSessionChangedDelegate(OnSessionChangedDelegate, LiveSession->SessionReference);
	if ( IsConsoleHost(LiveSession) )
	{
		LiveSubsystem->GetMatchmakingInterfaceLive()->RemoveMatchmakingTicket(Session->SessionName);
	}

	auto Association = LiveSessionInfo->GetAssociation();
	if (Association)
	{
		Association->DestroyAsync();
	}
	LiveSessionInfo->SetAssociation(nullptr);

	// Start a task to Leave() the first local user found in the session.
	// The tasks will chain until every local user is removed.
	CreateDestroyTask(SessionName, LiveSession, LiveSubsystem, true, CompletionDelegate);

	return true;
}

FNamedOnlineSession* FOnlineSessionLive::GetNamedSession( FName SessionName )
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return &Sessions[SearchIndex];
		}
	}
	return NULL;
}

void FOnlineSessionLive::RemoveNamedSession( FName SessionName )
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

class FNamedOnlineSession* FOnlineSessionLive::AddNamedSession( FName SessionName, const FOnlineSessionSettings& SessionSettings )
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
}

class FNamedOnlineSession* FOnlineSessionLive::AddNamedSession( FName SessionName, const FOnlineSession& Session )
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions) FNamedOnlineSession(SessionName, Session);
}

EOnlineSessionState::Type FOnlineSessionLive::GetSessionState( FName SessionName ) const 
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return Sessions[SearchIndex].SessionState;
		}
	}

	return EOnlineSessionState::NoSession;
}

IAsyncOperation<MultiplayerSession^>^ FOnlineSessionLive::CreateSessionOperation(
	const int UserIndex,
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
	if(!LiveContext)
	{
		return nullptr;
	}

	GUID NewGUID;
	CoCreateGuid(&NewGUID);
	Platform::Guid SessionGuidName = Platform::Guid(NewGUID);
	Platform::String^ UniqueSessionName = LiveSubsystem->RemoveBracesFromGuidString(SessionGuidName.ToString()); 

	FString TemplateNameString;
	const FOnlineSessionSetting* TemplateNameSetting = SessionSettings.Settings.Find( SETTING_SESSION_TEMPLATE_NAME );
	if ( TemplateNameSetting )
	{
		TemplateNameSetting->Data.GetValue(TemplateNameString);
	}

	// @ATG_CHANGE :  BEGIN UWP LIVE support
	auto SessionRef = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(
		LiveContext->AppConfig->ServiceConfigurationId,
		ref new String(*SessionTemplateName),
		UniqueSessionName);
	// @ATG_CHANGE :  END

	IVector<Platform::String^>^ InitiatorXboxUserIds = ref new Platform::Collections::Vector<Platform::String^>;
	InitiatorXboxUserIds->Append( LiveContext->User->XboxUserId );

	Platform::String^ CustomConstantsJson = ref new String( L"{}" );
	const FOnlineSessionSetting* CustomJsonSetting = SessionSettings.Settings.Find( SETTING_CUSTOM );
	if ( CustomJsonSetting )
	{
		FString InJson;

		CustomJsonSetting->Data.GetValue( InJson );

		if ( InJson.Len() )
		{
			CustomConstantsJson = ref new String( *InJson );
		}
	}

	// Create the session object
	MultiplayerSession^ LiveSession = ref new MultiplayerSession(
		LiveContext, SessionRef,
		0, // 0 will use the number of players from the template in the service config
		MultiplayerSessionVisibility::Any, // Using Any here appears to use the visibility from
										   // the template in the service config
		InitiatorXboxUserIds->GetView(), CustomConstantsJson );

	Platform::String^ PlayerCustomConstantBlob = nullptr;
	FString KeyFormat = SETTING_SESSION_MEMBER_CONSTANT_CUSTOM_JSON_XUID.ToString();
	FString Key = FString::Printf(*KeyFormat, LiveContext->User->XboxUserId->Data());

	// Add keyword
	if(!Keyword.IsEmpty())
	{
		Platform::Collections::Vector<String^>^ Keywords = ref new Platform::Collections::Vector<String^>();
		Keywords->Append( ref new String(*Keyword) );
		LiveSession->SessionProperties->Keywords = Keywords->GetView();
	}

	const FOnlineSessionSetting* CurrentPlayerConstantCustomJsonSetting = SessionSettings.Settings.Find( FName(*Key) );
	if ( CurrentPlayerConstantCustomJsonSetting )
	{
		FString CurrentPlayerConstantCustomJson;
		CurrentPlayerConstantCustomJsonSetting->Data.GetValue( CurrentPlayerConstantCustomJson );
		PlayerCustomConstantBlob = ref new String(*CurrentPlayerConstantCustomJson);
	}

	// Set current user to be active and joined
	// @ATG_CHANGE :  BEGIN UWP LIVE support
	LiveSession->Join(PlayerCustomConstantBlob, true, false);
	// @ATG_CHANGE :  END
	LiveSession->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
	LiveSession->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());
	
	// Indicate what events to subscribe to
	LiveSession->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);
	
	// Add custom settings
	WriteSettingsToLiveJson(SessionSettings, LiveSession, SystemUserFromXSAPIUser(LiveContext->User));

	auto writeSessionOp = LiveContext->MultiplayerService->WriteSessionAsync( LiveSession, Multiplayer::MultiplayerSessionWriteMode::CreateNew );

	return writeSessionOp;
}

void FOnlineSessionLive::DetermineSessionHost(FName SessionName, MultiplayerSession^ LiveSession)
{
	if (FNamedOnlineSession* NamedSession = GetNamedSession(SessionName))
	{
		String^ hostxuid;
		bool bHosting = NamedSession->bHosting;
		// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
		//int32 HostingPlayerNum = NamedSession->HostingPlayerNum;
		// @ATG_CHANGE :  END
		
		if(LiveSession && LiveSession->SessionProperties->HostDeviceToken != nullptr)
		{
			auto member = GetMemberFromDeviceToken(LiveSession,LiveSession->SessionProperties->HostDeviceToken);
			if(member)
			{
				UE_LOG(LogOnline, Log, TEXT("Determining host: using first host device token"));
				hostxuid = member->XboxUserId;

				if (member->IsCurrentUser)
				{
					bHosting = true;
					// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
					//HostingPlayerNum = 0;
					// @ATG_CHANGE :  END
				}
			}
		}

		if(hostxuid == nullptr)
		{
			UE_LOG(LogOnline, Error, TEXT("Host device token not set to valid player when determining host"));
			hostxuid = LiveSession->Members->GetAt(0)->XboxUserId;
		}
		else
		{
			NamedSession->OwningUserId = MakeShareable(new FUniqueNetIdLive( FString(hostxuid->Data()) ));
			NamedSession->bHosting = bHosting;
			// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
			NamedSession->HostingPlayerNum = LiveSubsystem->GetIdentityLive()->GetControllerIndexForId(*NamedSession->OwningUserId);
			// @ATG_CHANGE :  END
			UE_LOG(LogOnline, Log, TEXT("Picking %s as host for session"), hostxuid->Data());
		}
	}
}

MultiplayerSessionMember^ FOnlineSessionLive::GetMemberFromDeviceToken(MultiplayerSession^ LiveSession, Platform::String^ DeviceToken)
{
	for each (MultiplayerSessionMember^ member in LiveSession->Members)
	{
		if( _wcsicmp(member->DeviceToken->Data(), DeviceToken->Data()) == 0)
		{
			return member;
		}
	}

	return nullptr;
}

MultiplayerSession^ FOnlineSessionLive::SetHostDeviceTokenSynchronous(int32 UserNum, FName SessionName, MultiplayerSession^ LiveSession,
																	  XboxLiveContext^ Context)
{
	while(LiveSession && LiveSession->SessionProperties->HostDeviceToken == nullptr)
	{
		MultiplayerSessionMember^ CurrentMember = GetCurrentUserFromSession(LiveSession);
		if(CurrentMember == nullptr)
		{
			UE_LOG(LogOnline, Log, L"User in this console is not part of the Game Session, so not attempting to set Host");
			break;
		}

		LiveSession->SetHostDeviceToken(CurrentMember->DeviceToken);

		//. Assume the host always wants to be active
		SetCurrentUserActive( UserNum, LiveSession, true );

		//. Write the changes 
		auto writeSessionOp = Context->MultiplayerService->TryWriteSessionAsync( 
			LiveSession, 
			Multiplayer::MultiplayerSessionWriteMode::SynchronizedUpdate);

		create_task(writeSessionOp).then([this,&LiveSession](task<WriteSessionResult^> t)
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
					UE_LOG(LogOnline, Log, L"SetHostDeviceTokenSynchronous: TryWriteSessionAsync failed: %s", Result->Session->MultiplayerCorrelationId->Data());
				}
			}
			catch (Platform::Exception^ ex)
			{
				LiveSession = nullptr;
				UE_LOG(LogOnline, Log, L"SetHostDeviceTokenSynchronous: TryWriteSessionAsync failed with 0x%0.8X", ex->HResult);
			}
		}).wait();
	}

	return LiveSession;

}

void FOnlineSessionLive::RegisterVoice( const FUniqueNetId& PlayerId )
{
	IOnlineVoicePtr VoiceInt = LiveSubsystem->GetVoiceInterface();

	if ( VoiceInt.IsValid() )
	{
		if ( !LiveSubsystem->IsLocalPlayer( PlayerId ) )
		{
			VoiceInt->RegisterRemoteTalker( PlayerId );
		}
		else
		{
			int32 LocalUserNum =
				LiveSubsystem->GetIdentityLive()->GetControllerIndexForId( PlayerId );
			VoiceInt->RegisterLocalTalker( LocalUserNum );
		}
	}
}

void FOnlineSessionLive::UnregisterVoice( const FUniqueNetId& PlayerId )
{
	IOnlineVoicePtr VoiceInt = LiveSubsystem->GetVoiceInterface();

	if ( VoiceInt.IsValid() )
	{
		if ( !LiveSubsystem->IsLocalPlayer( PlayerId ) )
		{
			VoiceInt->UnregisterRemoteTalker( PlayerId );
		}
		else
		{
			int32 LocalUserNum =
				LiveSubsystem->GetIdentityLive()->GetControllerIndexForId( PlayerId );
			VoiceInt->UnregisterLocalTalker( LocalUserNum );
		}
	}
}

TSharedPtr<FInternetAddr> FOnlineSessionLive::GetAddrFromDeviceAssociation( Windows::Xbox::Networking::ISecureDeviceAssociation^ SDA )
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

	auto SocketSub = ISocketSubsystem::Get( PLATFORM_SOCKETSUBSYSTEM );
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
	// @ATG_CHANGE :  BEGIN UWP LIVE support
	const FString ActivationUriString = FPlatformMisc::GetProtocolActivationUri();

	// On Xbox we should have a user by now, but we probably won't on UWP.
	if(!ActivationUriString.IsEmpty() && (PLATFORM_XBOXONE || User::Users->Size > 0))
	{
		FPlatformMisc::SetProtocolActivationUri(FString());
		// @ATG_CHANGE :  END

		Windows::Foundation::Uri^ ActivationUri = ref new Windows::Foundation::Uri(ref new Platform::String(ActivationUriString.GetCharArray().GetData()));

		// See if this activation was in response to a session invite
		SaveInviteFromActivation(ActivationUri);
	}
}

MultiplayerSessionMember^ FOnlineSessionLive::GetLiveSessionHost(
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession )
{
	if(!LiveSession)
	{
		return nullptr;
	}

	auto HostToken = LiveSession->SessionProperties->HostDeviceToken;
	MultiplayerSessionMember^ Host = nullptr;
	for ( auto Member : LiveSession->Members )
	{
		if ( Member->DeviceToken == HostToken )
		{
			return Member;
		}
	}

	return nullptr;
}

FOnlineSessionSearchResult FOnlineSessionLive::CreateSearchResultFromSession(
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession,
	Platform::String^ HostDisplayName,
	XboxLiveContext^ LiveContext )
{
	FOnlineSessionSearchResult NewSearchResult;
	NewSearchResult.Session.SessionInfo =
		MakeShareable( new FOnlineSessionInfoLive( LiveSession ) );

	if ( HostDisplayName )
	{
		NewSearchResult.Session.OwningUserName = FString( HostDisplayName->Data() );
	}

	// Try to find the host.
	MultiplayerSessionMember^ Host = GetLiveSessionHost(LiveSession);

	if ( Host )
	{
		NewSearchResult.Session.OwningUserId =
			MakeShareable( new FUniqueNetIdLive( Host->XboxUserId->Data() ) );
	}
	else
	{
		NewSearchResult.Session.OwningUserName =
			FString( LiveSession->SessionReference->SessionName->Data() );
	}

	ReadSettingsFromLiveJson(LiveSession, NewSearchResult.Session, LiveContext);

	DebugLogLiveSession(LiveSession);

	// Find number of open slots.
	int MaxSlots = LiveSession->SessionConstants->MaxMembersInSession;
	int FilledSlots = LiveSession->Members->Size;
	int OpenSlots = MaxSlots - FilledSlots;
	NewSearchResult.Session.NumOpenPrivateConnections = 0;
	NewSearchResult.Session.NumOpenPublicConnections = OpenSlots;
	NewSearchResult.Session.SessionSettings.NumPublicConnections = MaxSlots;

	return NewSearchResult;
}

void FOnlineSessionLive::SaveSessionInvite(
	User^ AcceptingUser,
	Platform::String^ SessionHandle )
{
	if ( !AcceptingUser || !SessionHandle )
	{
		return;
	}

	// Set the invite data on the game thread since that's where it will be consumed
	LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue(	[this, AcceptingUser, SessionHandle]
	{
		PendingInvite.AcceptingUser = AcceptingUser;
		PendingInvite.SessionHandle = SessionHandle;
		PendingInvite.bHaveInvite = true;
	});
}

void FOnlineSessionLive::SaveInviteFromActivation(Windows::Foundation::Uri^ ActivationUri)
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

	if (SessionHandle != nullptr && UserXuid != nullptr)
	{
		// Find the user the invite is for.
		// This is an uncommon call so I'm OK with making the slow User::Users query
		// here
		const auto CachedUsers = User::Users;
		User^ JoiningUser = nullptr;

		for ( const auto CurrentUser : CachedUsers )
		{
			if ( CurrentUser->XboxUserId == UserXuid )
			{
				JoiningUser = CurrentUser;
				break;
			}
		}

		if ( !JoiningUser )
		{
			UE_LOG_ONLINE(Warning, TEXT( "FOnlineSessionLive::SaveInviteFromActivation: couldn't find a local user to accept the invite." ) );
			return;
		}

		// Technically, SaveSessionInvite takes the user that accepted the invite, while we're giving it the
		// user the invite was sent to. These may not match if multiple people are signed in and the wrong
		// one acts on the invite toast. Currently we expect the game to detect and react appropriately to this
		// case, if it cares. We may want to do something better here.
		// If the user joined from a gamercard, this shouldn't be an issue.
		SaveSessionInvite(JoiningUser, SessionHandle);
	}
}

void FOnlineSessionLive::OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ EventArgs)
{
	if(EventArgs->Kind == Windows::ApplicationModel::Activation::ActivationKind::Protocol)
	{
		ProtocolActivatedEventArgs^ ProtocolArgs = (ProtocolActivatedEventArgs^)EventArgs;				
		Windows::Foundation::Uri^ ActivationUri = ref new Windows::Foundation::Uri(ProtocolArgs->Uri->RawUri);

		UE_LOG_ONLINE(Log, TEXT("----- Got activation URI: %s"), ActivationUri->ToString()->Data());

		// See if this activation was in response to a session invite or gamercard join
		SaveInviteFromActivation(ActivationUri);
	}
}

bool FOnlineSessionLive::IsConsoleHost(
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession )
{
	if( LiveSession == nullptr )
	{
		return false;
	}

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ HostMember = GetLiveSessionHost(LiveSession);
	
	if( !HostMember )
	{
		return false;
	}

	for( auto Member : LiveSession->Members )
	{
		if ( Member && Member->IsCurrentUser && Member->DeviceToken == HostMember->DeviceToken )
		{
			return true;
		}
	}

	return false;
}

void FOnlineSessionLive::Tick( float DeltaTime )
{
	TickPendingInvites(DeltaTime);
}

void FOnlineSessionLive::TickPendingInvites( float DeltaTime )
{
	if (!PendingInvite.bHaveInvite)
	{
		return;
	}

	if (PendingInvite.AcceptingUser == nullptr)
	{
		UE_LOG_ONLINE(Warning,
			TEXT( "FOnlineSessionLive::TickPendingInvites: bHaveInvite is true but AcceptingUser is null." ) );
		PendingInvite.bHaveInvite = false;
		return;
	}

	if (PendingInvite.SessionHandle == nullptr)
	{
		UE_LOG_ONLINE(Warning,
			TEXT( "FOnlineSessionLive::TickPendingInvites: bHaveInvite is true but SessionHandle is null." ) );
	
		PendingInvite.bHaveInvite = false;
		return;
	}

	const auto AcceptingUserIndex =
		LiveSubsystem->GetIdentityLive()->GetControllerIndexForUser(
		PendingInvite.AcceptingUser );

	FUniqueNetIdLive UniqueNetId( PendingInvite.AcceptingUser->XboxUserId );

	auto Context = LiveSubsystem->GetLiveContext( PendingInvite.AcceptingUser );
	
	if ( !Context )
	{
		UE_LOG_ONLINE(Warning,
			TEXT( "FOnlineSessionLive::TickPendingInvites: couldn't create an XboxLiveContext for the AcceptingUser." ) );
		return;
	}

	auto AsyncOp = Context->MultiplayerService->GetCurrentSessionByHandleAsync( PendingInvite.SessionHandle );
	
	create_task( AsyncOp ).then( [this, AcceptingUserIndex, UniqueNetId]( task<MultiplayerSession^> SessionTask )
	{
		try
		{
			auto LiveSession = SessionTask.get();

			if (LiveSession == nullptr)
			{
				return;
			}

			LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue(	[this, LiveSession, AcceptingUserIndex, UniqueNetId]
			{
				auto SearchResult = CreateSearchResultFromSession(LiveSession, nullptr);

				TSharedPtr<const FUniqueNetId> UniqueNetIdPtr = MakeShareable(new FUniqueNetIdLive(UniqueNetId));

				TriggerOnSessionUserInviteAcceptedDelegates( true, AcceptingUserIndex, UniqueNetIdPtr, SearchResult );
			} );
		}
		catch ( Platform::COMException^ Ex )
		{
			UE_LOG( LogOnline, Warning,	TEXT( "FOnlineSessionLive::TickPendingInvites: error getting game session by handle: 0x%0.8x" ),
				Ex->HResult );
		}
	} );

	PendingInvite.bHaveInvite = false;
	PendingInvite.AcceptingUser = nullptr;
	PendingInvite.SessionHandle = nullptr;
}

void FOnlineSessionLive::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	auto LiveContext = LiveSubsystem->GetLiveContext(PlayerId);

	if (LiveContext == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	auto NamedSession = GetNamedSession(SessionName);

	if (NamedSession == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return;
	}

	auto LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);

	if (!LiveSessionInfo.IsValid())
	{
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	FOnlineAsyncTaskLiveRegisterLocalUser* RegisterTask = new FOnlineAsyncTaskLiveRegisterLocalUser(
		SessionName, LiveContext, LiveSubsystem, FUniqueNetIdLive(PlayerId), Delegate);

	LiveSubsystem->QueueAsyncTask(RegisterTask);
}

void FOnlineSessionLive::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	auto LiveContext = LiveSubsystem->GetLiveContext(PlayerId);

	if (LiveContext == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto NamedSession = GetNamedSession(SessionName);

	if (NamedSession == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto LiveSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);

	if (!LiveSessionInfo.IsValid())
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto RegisterTask = new FOnlineAsyncTaskLiveUnregisterLocalUser(
		SessionName, LiveContext, LiveSubsystem, FUniqueNetIdLive(PlayerId), Delegate);

	LiveSubsystem->QueueAsyncTask(RegisterTask);
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

bool FOnlineSessionLive::CanUserJoinSession(Windows::Xbox::System::User^ JoiningUser, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession)
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
	for (auto CurrentMember : LiveSession->Members)
	{
		if (FCString::Stricmp(CurrentMember->XboxUserId->Data(), JoiningUser->XboxUserId->Data()) == 0)
		{
			return true;
		}
	}

	return false;
}

void FOnlineSessionLive::OnSessionChanged(FName SessionName, MultiplayerSessionChangeTypes Diff)
{
	auto NamedSession = GetNamedSession(SessionName);
	MultiplayerSession^ UpdatedLiveSession = nullptr;

	if (NamedSession)
	{
		TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
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
			if (auto Host = GetLiveSessionHost(UpdatedLiveSession))
			{
				NamedSession->OwningUserId = LiveSubsystem->GetIdentityInterface()->CreateUniquePlayerId(Host->XboxUserId->Data());
				if (Host->IsCurrentUser)
				{
					NamedSession->bHosting = true;
					// @ATG_CHANGE :  BEGIN Allow modifying session visibility/joinability
					NamedSession->HostingPlayerNum = LiveSubsystem->GetIdentityLive()->GetControllerIndexForId(*NamedSession->OwningUserId);
					// @ATG_CHANGE :  END
				}
			}
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

	const auto NamedSession = GetNamedSession(SessionName);
	if (NamedSession == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnInitializationStateChanged - session doesn't exist or was destroyed before task ran"));
		return;
	}

	TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
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
		UE_LOG_ONLINE(Log, TEXT("  Qos failed for this member, failure case: %u"), static_cast<uint32>(SessionMember->InitializationFailureCause));
		FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
		MatchmakingInterface->SetTicketState(SessionName, EOnlineLiveMatchmakingState::None);
		MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, false);
		return;
	}

	if (SessionMember->InitializationEpisode == 0)
	{
		UE_LOG_ONLINE(Log, TEXT("  QoS succeeded"));

		auto LiveContext = LiveSubsystem->GetLiveContext(LiveInfo->GetLiveMultiplayerSession());
		auto SessionReadyTask = new FOnlineAsyncTaskLiveGameSessionReady(
			LiveSubsystem,
			LiveContext,
			NamedSession->SessionName,
			LiveInfo->GetLiveMultiplayerSessionRef());
		LiveSubsystem->QueueAsyncTask(SessionReadyTask);

		return;
	}

	if (SessionMember->InitializationEpisode == LiveInfo->GetLiveMultiplayerSession()->InitializingEpisode)
	{
		// This member is participating in QoS for this episode
		MultiplayerInitializationStage Stage = LiveInfo->GetLiveMultiplayerSession()->InitializationStage;

		switch (Stage)
		{
			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::None:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = None"));
				break;

			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::Unknown:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Unknown"));
				break;

			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::Joining:
				// Nothing to be done here, just wait for the other devices to finish joining the session
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Joining"));
				break;

			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::Measuring:
			{
				// Title will measure and upload QoS result, service will do the evaluation.
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Measuring"));

				auto LiveContext = LiveSubsystem->GetLiveContext(LiveInfo->GetLiveMultiplayerSession());

				FOnlineAsyncTaskLiveMeasureAndUploadQos* Task =
					new FOnlineAsyncTaskLiveMeasureAndUploadQos( this,
														 LiveContext,
														 NamedSession,
														 LiveSubsystem,
														 MAX_RETRIES,
														 QOS_TIMEOUT_MILLISECONDS,
														 QOS_PROBE_COUNT);
				LiveSubsystem->QueueAsyncTask(Task);
				break;
			}

			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::Evaluating:
				UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Evaluating"));
				// @todo Currently the engine supports system-evaluated QoS. Code for title-evaluated
				// QoS would go here.
				break;

			case Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage::Failed:
				{
					// QoS failed for the session overall
					UE_LOG_ONLINE(Log, TEXT("  InitializationStage = Failed"));
					FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
					MatchmakingInterface->SetTicketState(SessionName, EOnlineLiveMatchmakingState::None);
					MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, false);	
					break;
				}
			default:
				UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::OnInitializationStateChanged - Got unexpected InitializationStage: %u"), static_cast<uint32>(Stage));
				break;
		}
	}
}

void FOnlineSessionLive::OnMemberListChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, const FName& SessionName)
{
	check(IsInGameThread());

	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::OnMemberListChanged - game thread"));

	const auto NamedSession = GetNamedSession(SessionName);
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
		for (auto Member : LiveSession->Members)
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

	TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	check(LiveInfo.IsValid());

	// @v2live Should this go inside the MatchmakingState check below?
	if ( !NamedSession->OwningUserId.IsValid() )
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
	if ( !NamedSession->SessionSettings.bAllowJoinInProgress )
	{
		UE_LOG_ONLINE(Verbose, TEXT( "FOnlineSessionLive::OnMemberListChanged: Game is not join in progress, not resubmitting match ticket." ) );
		return;
	}

	// Cancel the current match ticket and re-advertise with the new number of open slots.
	// If the session isn't doing matchmaking, this is a no-op.
	LiveSubsystem->GetMatchmakingInterfaceLive()->SubmitMatchingTicket(LiveInfo->GetLiveMultiplayerSessionRef(), SessionName, true);
}

void FOnlineSessionLive::OnHostInvalid(const FName& SessionName)
{
	if (auto NamedSession = GetNamedSession(SessionName))
	{
		TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			auto LiveSession =  LiveInfo->GetLiveMultiplayerSession();
			if (auto LiveContext = LiveSubsystem->GetLiveContext(LiveSession))
			{
				LiveSession->SetHostDeviceToken(LiveSession->CurrentUser->DeviceToken);
				auto writeSessionOp = LiveContext->MultiplayerService->TryWriteSessionAsync( 
					LiveSession, 
					Multiplayer::MultiplayerSessionWriteMode::SynchronizedUpdate);

				create_task(writeSessionOp).then([this,SessionName](task<WriteSessionResult^> t)
				{
					try
					{
						WriteSessionResult^ Result = t.get();
						if (Result->Succeeded || Result->Status == WriteSessionStatus::OutOfSync)
						{
							LiveSubsystem->RefreshLiveInfo(SessionName, Result->Session);
						
							LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this, SessionName, Result]
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
	if(!bIsDestroyingSessions && Sessions.Num() > 0)
	{
		bIsDestroyingSessions = true;	// if multiple users lose subscriptions simultaneously, only try to destroy once
		OnSubscriptionLostDestroyCompleteDelegateHandle = AddOnDestroySessionCompleteDelegate_Handle(OnSubscriptionLostDestroyCompleteDelegate);

		FScopeLock Lock(&SessionLock);

		// @ATG_CHANGE :  BEGIN Avoid error from Sessions changing during iteration
		TArray<FNamedOnlineSession> SessionsCopy = Sessions;
		for (auto& CurrentSession : SessionsCopy)
		// @ATG_CHANGE :  END
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

FNamedOnlineSession* FOnlineSessionLive::GetNamedSessionForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef)
{
	check(IsInGameThread());
	
	FScopeLock Lock(&SessionLock);

	for (auto& CurrentSession : Sessions)
	{
		TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(CurrentSession.SessionInfo);
		if(!LiveInfo.IsValid())
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