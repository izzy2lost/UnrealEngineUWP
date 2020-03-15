// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLivePackage.h"
#include "SessionMessageRouter.h"

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionLive : public IOnlineSession
{
private:

	virtual FDelegateHandle AddOnMatchmakingCompleteDelegate_Handle(const FOnMatchmakingCompleteDelegate& Delegate) override;
	virtual void ClearOnMatchmakingCompleteDelegate_Handle(FDelegateHandle& Handle) override;
	virtual void TriggerOnMatchmakingCompleteDelegates(FName Param1, bool Param2) override;

	virtual FDelegateHandle AddOnCancelMatchmakingCompleteDelegate_Handle(const FOnCancelMatchmakingCompleteDelegate& Delegate) override;
	virtual void ClearOnCancelMatchmakingCompleteDelegate_Handle(FDelegateHandle& Handle) override;
	virtual void TriggerOnCancelMatchmakingCompleteDelegates(FName Param1, bool Param2) override;
	

	/** Reference to the main LIVE subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

	/** Need to hold on to an instance of the template for secure socket communication to work. */
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ PeerTemplate;

	/** Token to track SDA incoming events, needed to unregister from the event at shutdown. */
	Windows::Foundation::EventRegistrationToken TokenSecureAssociationIncoming;	

	/** Performs common constructor operations. */
	void Initialize();

	/**
	 * Tick invites captured from launch URI
	 * Waits for a delegate to be listening before triggering
	 */
	void TickPendingInvites(float DeltaTime);

	/** Handle initialization flow during matchmaking */
	void OnInitializationStateChanged(const FName& SessionName);

	/** Handle players being added or removed during matchmaking */
	void OnMemberListChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, const FName& SessionName);

	/** Read and react to any initial state from the MultiplayerSession after creating/joining */
	void OnSessionNeedsInitialState(FName SessionName);

	void OnMultiplayerSubscriptionsLost();

	/** On subscription loss, fire appropriate delegate after cleaning up the sessions. */
	void OnSubscriptionLostDestroyComplete(FName SessionName, bool bWasSuccessful);

	/** Responds to protocol activations to determine if an invite was accepted, and joins the session if so. */
	void OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ EventArgs);
		
	/** Token to track activation events, for invites that occur after launch */
	Windows::Foundation::EventRegistrationToken ActivatedToken;

	// @ATG_CHANGE : BEGIN UWP invite support
	/** Token to user added events, for invites where users are not initially signed in */
	Windows::Foundation::EventRegistrationToken UserAddedToken;
	// @ATG_CHANGE : END

	/**
	 * Parses a protocol activation URI. If it was from an accepted invite, begins the invite accepted flow.
	 * @param ActivationUri the protocol activation URI
	 * @return true if we processed the activation URI, false if not
	 */
	bool SaveInviteFromActivation(Windows::Foundation::Uri^ ActivationUri);
	
	/**
	 * Turns a session handle into a Session, then into an FOnlineSessionSearchResult, then triggers
	 * the OnSessionInviteAccepted delegates.
	 */
	void SaveSessionInvite(
		Windows::Xbox::System::User^ AcceptingUser,
		Platform::String^ SessionHandle );

	/** This task calls AddNamedSession so it needs access. */
	friend class FOnlineAsyncTaskLiveCreateSession;

	/**
	 * Registers and updates voice data for the given player id
	 *
	 * @param PlayerId player to register with the voice subsystem
	 */
	void RegisterVoice(const FUniqueNetId& PlayerId);

	/**
	 * Unregisters a given player id from the voice subsystem
	 *
	 * @param PlayerId player to unregister with the voice subsystem
	 */
	void UnregisterVoice(const FUniqueNetId& PlayerId);

	/** Called when signin is complete */
	void CleanUpOrphanedSessions(Windows::Xbox::System::User^ User) const;

	/** Token for un-registering the signin callback */
	Windows::Foundation::EventRegistrationToken SignInCompletedToken;

	/** Any join/invite from a protocol activation */
	struct FPendingInviteData
	{
		/** Whether there is a pending invite or not */
		bool bHaveInvite;

		/** Cached pointer to the user who accepted the invite */
		Windows::Xbox::System::User^ AcceptingUser;

		/** Cached handle to the session to join */
		Platform::String^ SessionHandle;

		FPendingInviteData()
			: bHaveInvite(false)
			, AcceptingUser(nullptr)
			, SessionHandle(nullptr)
		{
		}
	};

	/** Contains information about a join/invite parsed from the protocol activation */
	FPendingInviteData PendingInvite;

	/** Common implementation of creating a session based on an XboxLiveContext
	 *
	 * @param LiveContext The Live context of the local user creating the session
	 * @param InSessionSettings Settings to use for the new session
	 * @param Keyword If non-empty, keyword to set on the session. Used for filtering sessions during searches
	 */
	Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ InternalCreateSessionOperation(
		Microsoft::Xbox::Services::XboxLiveContext^ LiveContext,
		const FOnlineSessionSettings& InSessionSettings,
		const FString& Keyword,
		const FString& SessionTemplateName);

	/** Starts QoS against the search results */
	void PingResultsAndTriggerDelegates(const TSharedRef<FOnlineSessionSearch>& SearchSettings);

	/** Get LocalPlayerNum/ControllerId for a Host's netid and -1 on failure */
	int32 GetHostingPlayerNum(const FUniqueNetId& HostNetId) const;

PACKAGE_SCOPE:

	static const int QOS_TIMEOUT_MILLISECONDS = 5000;
	static const int QOS_PROBE_COUNT = 8;

	/**
	 * Constructor.
	 *
	 * @param InSubsystem Pointer to the subsystem that created this object.
	 */
	FOnlineSessionLive(class FOnlineSubsystemLive* InSubsystem);

	/**
	 * Session tick for various background tasks
	 */
	void Tick(float DeltaTime);

	/** Shared functionality between creating a game session and a matchmaking session
	 *
	 * @param UserIndex The index of the local user creating the session
	 * @param InSessionSettings Settings to use for the new session
	 * @param Keyword If non-empty, keyword to set on the session. Used for filtering sessions during searches
	 */
	Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ CreateSessionOperation(
		const FUniqueNetId& UserId,
		const FOnlineSessionSettings& InSessionSettings,
		const FString& Keyword,
		const FString& SessionTemplateName);

	/** Shared functionality between creating a game session and a matchmaking session
	 *
	 * @param UserIndex The index of the local user creating the session
	 * @param InSessionSettings Settings to use for the new session
	 * @param Keyword If non-empty, keyword to set on the session. Used for filtering sessions during searches
	 */
	Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ CreateSessionOperation(
		int UserIndex,
		const FOnlineSessionSettings& InSessionSettings,
		const FString& Keyword,
		const FString& SessionTemplateName);

	/**
	 * Read from Live session document into UE settings
	 *
	 * @param LiveSession the session object to read settings from
	 * @param SessionSettings the settings object to write to
	 */
	static void ReadSettingsFromLiveJson(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, FOnlineSession& Session, Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = nullptr);

	/**
	 * Write from UE settings into Live document (note that this does not write the session back to Live; caller should)
	 *
	 * @param UpdatedSessionSettings the settings object to read from
	 * @param LiveSession the session object to write to
	 */
	static void WriteSettingsToLiveJson(const FOnlineSessionSettings& UpdatedSessionSettings, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, Windows::Xbox::System::User^ HostUser);

	// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
	static void WriteSessionPrivacySettingsToLiveJson(const FOnlineSessionSettings& SessionSettings, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession);
	// @ATG_CHANGE : END

	/**
	 * Extract one FString that contains all the members settings
	 *
	 * @param LiveSession the session object to read from
	 * @param OutJsonString the string to write to
	 */
	static void ExtractJsonMemberSettings(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, FString& OutJsonString);

	/** Replaces the JSON member list in UpdatedSettings with the latest from LiveSession */
	static void UpdateMatchMembersJson(FSessionSettings& UpdatedSettings, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession);

	/** Mark the user as active within the session */
	void SetCurrentUserActive(int32 UserNum, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, bool bIsActive);
	
	/** Look up the NamedSession corresponding to a Live session reference */
	FNamedOnlineSession* GetNamedSessionForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef);

	/* Set me as the host using a host device token, does nothing if token already set. Synchronous, so only use from lambda */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ SetHostDeviceTokenSynchronous(int32 UserNum, FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession,
		Microsoft::Xbox::Services::XboxLiveContext^ Context);

	/** Get my info from a session */
	static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ GetCurrentUserFromSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession);

	/** work out who we want as host and set it in the named session */
	void DetermineSessionHost(FName session, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession);

	/** Get member from device token */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ GetMemberFromDeviceToken(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession, Platform::String^ DeviceToken);

	/** Return the secure device association template */
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ GetSDATemplate() const { return PeerTemplate; }

	/** Turns a secure device association into an FInternetAddr */
	static TSharedRef<FInternetAddr> GetAddrFromDeviceAssociation(Windows::Xbox::Networking::ISecureDeviceAssociation^ SDA);

	/** Uses data from a MultiplayerSession to initialize an FOnlineSessionSearchResult. */
	static FOnlineSessionSearchResult CreateSearchResultFromSession(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession,
		Platform::String^ HostDisplayName,
		Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = nullptr);

	/** Returns the host of a session, or null if there is no host. */
	// @ATG_CHANGE :  BEGIN - VS 2017 fix
	static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ GetLiveSessionHost(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession );
	// @ATG_CHANGE :  END

	/** Returns true if the local console is the host of the session */
	static bool IsConsoleHost( Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession );

	/** Can be used in the association state change event to log the changes */
	static void LogAssociationStateChange(
		Windows::Xbox::Networking::SecureDeviceAssociation^ Association,
		Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs^ EventArgs);

	/** Returns a string representation of the state */
	static const TCHAR* AssociationStateToString(Windows::Xbox::Networking::SecureDeviceAssociationState State);

	/** Returns true if the session has either an open slot or a slot reserved for the user. */
	static bool CanUserJoinSession(
		Windows::Xbox::System::User^ JoiningUser,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession);

	/** Returns what the Multiplayer Session restriction should be based on session settings */
	static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction GetLiveSessionRestrictionFromSettings(const FOnlineSessionSettings& SessionSettings);

	/** Returns true if this session allows invites and join in presence */
	static bool AreInvitesAndJoinViaPresenceAllowed(const FOnlineSessionSettings& OnlineSessionSettings);

	/** Critical sections for thread safe operation of session lists */
	mutable FCriticalSection SessionLock;

	/** Current search object */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

	/** Current session settings */
	TArray<FNamedOnlineSession> Sessions;

	FCriticalSection SessionResultLock;
	int ExpectedResults;
	// Synchronization for session changes handlers and destruction on subscription loss
	bool bIsDestroyingSessions;
	FOnEndSessionCompleteDelegate OnSubscriptionLostDestroyCompleteDelegate;
	FDelegateHandle OnSubscriptionLostDestroyCompleteDelegateHandle;

protected: // @todo: revert to protected because that's how it's declared in the parent class
	// IOnlineSession interface
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;

	// These classes need to access AddNamedSession
	friend class FOnlineMatchmakingInterfaceLive;

public:
	virtual ~FOnlineSessionLive();

	/**
	 * Checks whether an invite was accepted during launch. Invite information is saved by XboxOneLaunch.cpp when the
	 * ViewProvider receives an invite accepted or gamercard join protocol activation.
	 */
	void CheckPendingSessionInvite();

	// IOnlineSession interface
	virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;
	virtual void RemoveNamedSession(FName SessionName) override;
	virtual bool HasPresenceSession() override;
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
	virtual bool CreateSession(int32 HostingPlayerControllerIndex, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = false)  override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual TSharedPtr<const FUniqueNetId> CreateSessionIdFromString(const FString& SessionIdStr) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override {return false;}
	virtual bool JoinSession(int32 ControllerIndex, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& UserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)  override;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)  override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)  override;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false)  override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)  override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)  override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override { return Sessions.Num(); }
	virtual void DumpSessionState() override {}

	// Use task to get session in response to notification
	void OnSessionChanged(FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff);

	void OnHostInvalid(const FName& SessionName);

	/** Handle sending a session invite to friends */
	bool SendSessionInviteToFriends_Internal(Microsoft::Xbox::Services::XboxLiveContext^ LiveContext,
		FName SessionName,
		Windows::Foundation::Collections::IVectorView<Platform::String^>^ FriendXuidVectorView);

	/** Update the SessionUpdateStatName stat for all active players */
	void UpdateSessionChangedStats();

	/** Have we already subscribed to the PlayerId for session update stats? */
	bool IsSubscribedToSessionStatUpdates(const FUniqueNetIdLive& PlayerId) const;
	/** Add the PlayerId to the list of subscribed session update stats list */
	void AddSessionUpdateStatSubscription(const FUniqueNetIdLive& PlayerId);

PACKAGE_SCOPE:
	FDelegateHandle OnSubscriptionLostDelegateHandle;

	// Initialize session state after create/join
	FOnSessionNeedsInitialStateDelegate OnSessionNeedsInitialStateDelegate;

	FOnSessionChangedDelegate OnSessionChangedDelegate;

	/** Event to be called when our session updates */
	FString SessionUpdateEventName;
	/** Stat to subscribe to when we want to listen for a friend's session change event */
	FString SessionUpdateStatName;
	/** List of players that we are subscribed to for session stat updates */
	TSet<FUniqueNetIdLive> SessionUpdateStatSubsriptions;

	/** Whether only the host can update the session or if anyone can */
	bool bOnlyHostUpdateSession;

	static const int MAX_RETRIES = 20;
};

typedef TSharedPtr<FOnlineSessionLive, ESPMode::ThreadSafe> FOnlineSessionLivePtr;
