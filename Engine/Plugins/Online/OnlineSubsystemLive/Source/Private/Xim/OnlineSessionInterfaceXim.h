#pragma once

#include "OnlineSessionInterface.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "XimMessageRouter.h"

namespace xbox
{
	namespace services
	{
		namespace xbox_integrated_multiplayer
		{
			class xim_player;
			enum class xim_network_exit_reason;
		}
	}
}

class FOnlineSessionInfoXim : public FOnlineSessionInfo
{
private:
	FString ProtocolActivationString;
	FString ConnectString;
	FString MultiplayerCorrelationId;
	FGuid RoundId;
	TSharedRef<const FUniqueNetIdLive> SessionId;
	bool Matchmade;
public:
	FOnlineSessionInfoXim(TSharedRef<const FUniqueNetIdLive> OwnerId, bool InMatchmade) : SessionId(OwnerId), Matchmade(InMatchmade) {}
	FOnlineSessionInfoXim(TSharedRef<const FUniqueNetIdLive> OwnerId, const FString& InActivationString) : SessionId(OwnerId), ProtocolActivationString(InActivationString) {}

	const FString& GetProtocolActivationString() const { return ProtocolActivationString; }
	bool IsMatchmade() const { return Matchmade; }
	FGuid GetRoundId() const { return RoundId; }
	void NewRound() { RoundId = FGuid::NewGuid(); }
	const WCHAR* GetMultiplayerCorrelationId() { return MultiplayerCorrelationId.IsEmpty() ? nullptr : *MultiplayerCorrelationId; }
	void SetMultiplayerCorrelationId(const WCHAR* CorrelationId) { check(MultiplayerCorrelationId.IsEmpty()); MultiplayerCorrelationId = CorrelationId; }

	virtual const uint8* GetBytes() const override { return nullptr; }
	virtual int32 GetSize() const override { return 0; }
	virtual bool IsValid() const override { return SessionId->IsValid(); }
	virtual FString ToString() const override { return ProtocolActivationString; }
	virtual FString ToDebugString() const override { return ProtocolActivationString; }
	virtual const FUniqueNetId& GetSessionId() const override { return *SessionId; }
};

class FOnlineSessionXim : public IOnlineSession, public TSharedFromThis<FOnlineSessionXim, ESPMode::ThreadSafe>
{
private:
	/** Reference to the main LIVE subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

	bool FindSessions(int32 SearchingPlayerNum, const FUniqueNetId& SearchingPlayerId, FOnXimJoinableUsersCompletedDelegate OnCompletionDelegate);
	bool FindSessions(int32 SearchingPlayerNum, const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings);
	bool FindFriendSession(int32 SearchingPlayerNum, const FUniqueNetId& SearchingPlayerId, const TArray<TSharedRef<const FUniqueNetId>>& Friend);
	void OnListenServerDetermined();
	void HookXimEvents();
	void SessionSearchResultsFromUserId(int32 LocalUserNum, TSharedRef<FUniqueNetIdLive> SessionOwnerId, FOnlineSessionSearchResult& OutSearchResult);
	void SetXimAllowedJoinsFromSettings(const FOnlineSessionSettings& SessionSettings);
	void SetXimCustomPropertiesFromSettings(const FOnlineSessionSettings& SessionSettings);

	FDelegateHandle XimPlayerJoinedHandle;
	FDelegateHandle XimPlayerLeftHandle;
	FDelegateHandle XimMoveSucceededHandle;
	FDelegateHandle XimNetworkExitedHandle;
	FDelegateHandle XimPlayerPropertyChangedHandle;
	FDelegateHandle XimNetworkPropertyChangedHandle;
	FDelegateHandle XimJoinableUsersCompletedHandle;

	TMap<FUniqueNetIdLive, FOnRegisterLocalPlayerCompleteDelegate> LocalPlayerRegisteredDelegates;
	TMap<FUniqueNetIdLive, FOnUnregisterLocalPlayerCompleteDelegate> LocalPlayerUnregisteredDelegates;
	FOnDestroySessionCompleteDelegate ExtraDestroySessionDelegate;

	void OnXimPlayerJoined(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);
	void OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);
	void OnXimMoveSucceeded();
	void OnXimNetworkExited(xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason Reason);
	void OnXimPlayerPropertyChanged(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer, const FString& PropertyName);
	void OnXimNetworkPropertyChanged(const FString& PropertyName);

protected:
	// IOnlineSession interface
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;


PACKAGE_SCOPE:
	/**
	* Constructor.
	*
	* @param InSubsystem Pointer to the subsystem that created this object.
	*/
	FOnlineSessionXim(class FOnlineSubsystemLive* InSubsystem);


	void Tick(float DeltaTime);
	TSharedPtr<FNamedOnlineSession> CurrentSession;

public:

	void CheckPendingSessionInvite();

	// IOnlineSession interface
	virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;
	virtual void RemoveNamedSession(FName SessionName) override;
	virtual bool HasPresenceSession() override;
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
	virtual bool CreateSession(int32 HostingPlayerControllerIndex, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = false) override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override { return false; }
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override { return false; }
	virtual bool JoinSession(int32 ControllerIndex, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& UserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType = NAME_GamePort)  override;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)  override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)  override;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false)  override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)  override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)  override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override {}
};