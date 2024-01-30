// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/AvalancheInstanceSettings.h"
#include "IAvalancheBroadcastSettings.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvaMediaPlaybackMessages.h"
#include "Templates/SharedPointer.h"

class FAvaMediaPlaybackInstance;
class FAvaMediaSyncManager;
class UAvaMediaServerPlaybackTransition;
struct FAvaAssetSyncStatusReceivedParams;

DECLARE_LOG_CATEGORY_EXTERN(LogAvaPlaybackServer, Log, All);

/**
 * Playback (and Broadcast) Server
 *
 * The playback server implements the commands for broadcast (channels and outputs) and playback.
 * Playback assets are either "playables" or "playback graphs", however the playback server will
 * create playback graphs for everything. If the asset to play is a "playable" (either a level or
 * a ava blueprint), it will create a transient playback graph for it.
 *
 * A lot of the playback commands are geared toward running a playback graph with a single playable node,
 * mostly because this system is only used with playlists or playback graphs run on the client side.
 * So, it will emulate a client-side playable with a local transient playback graph with one player node.
 * The use case of running a more complex playback graph asset on the server side has not occured yet.
 */
class AVALANCHEMEDIA_API FAvaMediaPlaybackServer : public TSharedFromThis<FAvaMediaPlaybackServer>
{
public:
	FAvaMediaPlaybackServer();
	virtual ~FAvaMediaPlaybackServer();

	void Init(const FString& InAssignedServerName);

	struct FPlaybackInstanceReference
	{
		FGuid Id;
		FSoftObjectPath Path;
	};
	
	TArray<FPlaybackInstanceReference> StopPlaybacks(const FString& InChannelName = FString(), const FSoftObjectPath& InAssetPath = FSoftObjectPath(), bool bInUnload = true);
	TArray<FPlaybackInstanceReference> StartPlaybacks();

	/**
	 * Returns a list of channel names from all the playing playback instances.
	 * Optionally filter for a given asset.
	 */
	TArray<FString> GetAllChannelsFromPlayingPlaybacks(const FSoftObjectPath& InAssetPath = FSoftObjectPath()) const;
	
	void StartBroadcast();
	void StopBroadcast();

	/**
	 *	Indicate the manager is in a shutdown sequence and will force game instances to destroy worlds right away.
	*/
	void StartShuttingDown();

	/** Returns the server's name. */
	const FString& GetName() const { return ServerName;}

	bool HasUserData(const FString& InKey) const { return UserDataEntries.Contains(InKey);}
	
	const FString& GetUserData(const FString& InKey) const;

	/** Add user data to this server. This is replicated and accessible to the client. */
	void SetUserData(const FString& InKey, const FString& InData);

	/** Remove the server's user data entry from the given key. */
	void RemoveUserData(const FString& InKey);

	/** Returns the list of connected clients. */
	TArray<FString> GetClientNames() const;

	/** Returns the client address. */
	FMessageAddress GetClientAddress(const FString& InClientName) const;

	/** Returns true if the corresponding client user data for the given client name is found. */
	bool HasClientUserData(const FString& InClientName, const FString& InKey) const;

	/**
	 * Returns the corresponding client user data for the given client name and key.
	 * Returns empty string if not found.
	 */
	const FString& GetClientUserData(const FString& InClientName, const FString& InKey) const;

	/**
	 * Access broadcast settings replicated from connected client.
	 * Will return nullptr if no clients are connected.
	 */
	const IAvalancheBroadcastSettings* GetBroadcastSettings() const;

	/**
	 * Access Avalanche Instance settings replicated from connected client(s).
	 * Will return nullptr if no clients are connected.
	 */
	const FAvalancheInstanceSettings* GetAvalancheInstanceSettings() const;
	
	/** Access the server's playback manager. */
	const FAvaMediaPlaybackManager& GetPlaybackManager() const { check(Manager); return *Manager; }
	FAvaMediaPlaybackManager& GetPlaybackManager() { check(Manager); return *Manager; }

	TSharedPtr<FAvaMediaPlaybackInstance> FindActivePlaybackInstance(const FGuid& InInstanceId) const
	{
		const TSharedPtr<FAvaMediaPlaybackInstance>* FoundInstance = ActivePlaybackInstances.Find(InInstanceId);
		return FoundInstance ? *FoundInstance : TSharedPtr<FAvaMediaPlaybackInstance>();
	}

	bool RemoveActivePlaybackInstance(const FGuid& InInstanceId)
	{
		return ActivePlaybackInstances.Remove(InInstanceId) > 0;	
	}

	bool RemovePlaybackInstanceTransition(const FGuid& InTransitionId);
	
	void SendPlayableTransitionEvent(
		const FGuid& InTransitionId, const FGuid& InInstanceId, EAvalanchePlayableTransitionEventFlags InFlags,
		const FName& InChannelName, const FString& InClientName);
	
public:
	// Message handlers
	void HandlePlaybackPing(const FAvaMediaPlaybackPing& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleUpdateClientUserData(const FAvaMediaUpdateClientUserData& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleStatCommand(const FAvaMediaStatCommand& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleDeviceProviderDataRequest(const FAvaDeviceProviderDataRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleUpdateClientInfo(const FAvaMediaUpdateClientInfo& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleAvalancheInstanceSettingsUpdate(const FAvalancheInstanceSettingsUpdate& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePackageEvent(const FAvaMediaPlaybackPackageEvent& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePlaybackAssetStatusRequest(const FAvaMediaPlaybackAssetStatusRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePlaybackRequest(const FAvaMediaPlaybackRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleAnimPlaybackRequest(const FAvaMediaAnimPlaybackRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleRemoteControlUpdateRequest(const FAvaMediaRemoteControlUpdateRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePlayableTransitionStartRequest(const FAvaMediaTransitionStartRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePlayableTransitionStopRequest(const FAvaMediaTransitionStopRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleBroadcastSettingsUpdate(const FAvaMediaBroadcastSettingsUpdate& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleBroadcastRequest(const FAvaMediaBroadcastRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleBroadcastChannelSettingsUpdate(const FAvaMediaBroadcastChannelSettingsUpdate& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleBroadcastStatusRequest(const FAvaMediaBroadcastStatusRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	/** Returns underlying message bus endpoint address id  */
	FString GetMessageEndpointAddressId() const;

protected:
	void Tick();

	void RegisterCommands();
	
	// Command handlers
	void StartPlaybackCommand(const TArray<FString>& InArgs);
	void StopPlaybackCommand(const TArray<FString>& InArgs);
	void StartBroadcastCommand(const TArray<FString>& InArgs);
	void StopBroadcastCommand(const TArray<FString>& InArgs);
	void SetUserDataCommand(const TArray<FString>& InArgs);
	void ShowStatusCommand(const TArray<FString>& InArgs);
	
	// Event handlers
	void OnAvaMediaSettingsChanged(UObject*, struct FPropertyChangedEvent&);
	void OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange);
	void OnMediaOutputStateChanged(const FAvaOutputChannel& InChannel, const UMediaOutput* InMediaOutput);
	void OnAvaAssetSyncStatusReceived(const FAvaAssetSyncStatusReceivedParams& InParams);
	void OnPlaybackInstanceInvalidated(const FAvaMediaPlaybackInstance& InPlaybackInstance);
	void OnPlaybackInstanceStatusChanged(const FAvaMediaPlaybackInstance& InPlaybackInstance);
	void OnPlaybackAssetRemoved(const FSoftObjectPath& InAssetPath);
	void OnPlayableSequenceEvent(UAvalanchePlayable* InPlayable, const FName& SequenceName, EAvalanchePlayableSequenceEventType InEventType);
	
	void ApplyAvaMediaSettings();

	template<typename MessageType>
	void FillServerInfo(MessageType* InMessage)
	{
		InMessage->ServerName = ServerName;
	}
	
	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const FMessageAddress& InRecipient, EMessageFlags flags = EMessageFlags::None)
	{
		FillServerInfo(InMessage);
		MessageEndpoint->Send(InMessage, MessageType::StaticStruct(), flags, nullptr,
			TArrayBuilder<FMessageAddress>().Add(InRecipient), FTimespan::Zero(), FDateTime::MaxValue());
	}

	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const TArray<FMessageAddress>& InRecipients, EMessageFlags flags = EMessageFlags::None)
	{
		FillServerInfo(InMessage);
		MessageEndpoint->Send(InMessage, MessageType::StaticStruct(), flags, nullptr,
			InRecipients, FTimespan::Zero(), FDateTime::MaxValue());
	}

	void SendUserDataUpdate(const TArray<FMessageAddress>& InRecipients);

	void SendChannelStatusUpdate(const FString& InChannelName, const FAvaOutputChannel& InChannel, const FMessageAddress& InSender, bool bInIncludeOutputData = false);
	void SendAllChannelStatusUpdate(const FMessageAddress& InSender, bool bInIncludeOutputData = false);
	void SendLogMessage(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time);

	// Playback Commands
	void ExecutePendingPlaybackCommands();

	TSharedPtr<FAvaMediaPlaybackInstance> GetOrLoadPlaybackInstance(const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	void LoadPlayback(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	void StartPlayback(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	void StopPlayback(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);	
	void UnloadPlayback(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	void SetPlaybackUserData(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InUserData);
	void SendPlaybackUserData(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId);
	void SendPlaybackStatus(const FMessageAddress& InReplyToAddress, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	
	void SendPlaybackStatus(const FMessageAddress& InSendTo, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath, EAvaMediaPlaybackStatus InStatus);
	void SendPlaybackStatus(const TArray<FMessageAddress>& InRecipients, const FGuid& InInstanceId, const FString& InChannelName, const FSoftObjectPath& InAssetPath, EAvaMediaPlaybackStatus InStatus);
	void SendPlaybackStatuses(const FMessageAddress& InSendTo, const FString& InChannelName, const TArray<FPlaybackInstanceReference>& InInstances, EAvaMediaPlaybackStatus InStatus);
	void SendAllPlaybackStatusesForChannelAndAssetPath(const FMessageAddress& InSendTo, const FString& InChannelName, const FSoftObjectPath& InAssetPath);
	void SendPlaybackAssetStatus(const FMessageAddress& InSendTo, const FSoftObjectPath& InAssetPath, EAvaMediaPlaybackAssetStatus InStatus);

	EAvaMediaPlaybackStatus GetUnloadedPlaybackStatus(const FSoftObjectPath& InAssetPath)
	{
		return Manager->GetUnloadedPlaybackStatus(InAssetPath);
	}

	bool UpdateChannelOutputConfig(FAvaOutputChannel& InChannel, const TArray<FAvaMediaOutputData>& InMediaOutputs, bool bInRefreshState);

private:
	FString ComputerName;
	FString ServerName;
	FString ProjectContentPath;
	uint32 ProcessId = 0;
	TMap<FString, FString> UserDataEntries;
	
	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	TArray<IConsoleObject*> ConsoleCommands;

	/** The playback server has its own playback manager to not interfere with the local one. */
	TSharedPtr<FAvaMediaPlaybackManager> Manager;

	// This is used to block sending status update from the channel event while the channels
	// are refreshing state on media output state changes. This avoid sending spurious channel states
	// while the update is not completed for all outputs.
	bool bBlockChannelStatusUpdate = false;

	struct FPendingPlaybackCommand
	{
		FMessageAddress ReplyTo;
		FAvaMediaPlaybackCommand Command;
	};

	/** Accumulate all the playback commands and execute them all in one batch on the next tick. */
	TArray<FPendingPlaybackCommand> PendingPlaybackCommands;

	/** Keep an map of active instances per id for fast lookup. */
	TMap<FGuid, TSharedPtr<FAvaMediaPlaybackInstance>> ActivePlaybackInstances;
	
	class FServerPlaybackInstanceTransitionCollection;
	TUniquePtr<FServerPlaybackInstanceTransitionCollection> PlaybackInstanceTransitions;
	
	/**
	 * Routes the log messages to the server for replication over the message bus.
	 */
	class FReplicationOutputDevice : public FOutputDevice
	{
	public:
		FReplicationOutputDevice(FAvaMediaPlaybackServer* InServer);
		virtual ~FReplicationOutputDevice() override;

		/** Set the minimum verbosity that will be replicated to the client. */
		void SetVerbosityThreshold(ELogVerbosity::Type InVerbosityThreshold);

	protected:
		//~ FOutputDevice interface.
		virtual void Serialize(const TCHAR* InText, ELogVerbosity::Type InVerbosity, const FName& InCategory) override;
		virtual void Serialize(const TCHAR* InText, ELogVerbosity::Type InVerbosity, const FName& InCategory, double InTime) override;

	private:
		FAvaMediaPlaybackServer* Server;
		ELogVerbosity::Type VerbosityThreshold = ELogVerbosity::Type::NoLogging; 
	};
	
	TUniquePtr<FReplicationOutputDevice> ReplicationOutputDevice;

	TOptional<ELogVerbosity::Type> LogReplicationVerbosityFromCommandLine;

	class FClientBroadcastSettings : public IAvalancheBroadcastSettings
	{
	public:
		FAvaMediaBroadcastSettings Settings;
		virtual ~FClientBroadcastSettings() override = default;

		// IAvalancheBroadcastSettings
		virtual const FLinearColor& GetChannelClearColor() const override { return Settings.ChannelClearColor; }
		virtual EPixelFormat GetDefaultPixelFormat() const override { return Settings.ChannelDefaultPixelFormat; }
		virtual const FIntPoint& GetDefaultResolution() const override { return Settings.ChannelDefaultResolution; }
		virtual bool IsDrawPlaceholderWidget() const override { return Settings.bDrawPlaceholderWidget; }
		virtual const FSoftObjectPath& GetPlaceholderWidgetClass() const override { return Settings.PlaceholderWidgetClass; }
		// ~IAvalancheBroadcastSettings
	};
	
	/** Remote Client Info */
	class FClientInfo
	{
	public:
		FMessageAddress Address;
		FString ClientName;
		FString ComputerName;
		FString ProjectContentPath;
		uint32 ProcessId = 0;
		TMap<FString, FString> UserDataEntries;
		FClientBroadcastSettings BroadcastSettings;
		FAvalancheInstanceSettings AvalancheInstanceSettings;

		bool bClientInfoReceived = false;
	
		/**
		 * The status of the server's asset compared to the remote is tracked
		 * on the server side. For now, the remote client is considered to have the
		 * reference asset. But in the future, a hub may hold the reference asset instead.
		 */
		TSharedPtr<FAvaMediaSyncManager> MediaSyncManager;

		TOptional<FDateTime> PingTimeout;
		
		FClientInfo(const FMessageAddress& InClientAddress, const FString& InClientName);
		~FClientInfo();

		
		/**
		* Add a time by which a new ping should arrive
		*/
		void AddTimeout(const FDateTime& InNewTimeout)
		{
			if (!PingTimeout || InNewTimeout < PingTimeout.GetValue())
			{
				PingTimeout = InNewTimeout;
			}
		}

		/**
		 * Check if new ping arrived on time
		*/
		bool HasTimedOut(const FDateTime& InNow) const
		{
			return PingTimeout && InNow > PingTimeout.GetValue();
		}

		/**
		 * Resets the ping timeout.
		 * This is done when the ping response is received.
		 */
		void ResetPingTimeout()
		{
			PingTimeout.Reset();
		}
	};
	TMap<FString, TSharedPtr<FClientInfo>> Clients;	// Key is ClientName

	FClientInfo& GetOrCreateClientInfo(const FString& InClientName, const FMessageAddress& InClientAddress);
	FClientInfo* GetClientInfo(const FMessageAddress& InClientAddress) const;
	FClientInfo* GetClientInfo(const FString& InClientName) const
	{
		const TSharedPtr<FClientInfo>* ClientInfoPtr = Clients.Find(InClientName);
		return ClientInfoPtr ? ClientInfoPtr->Get() : nullptr;
	}
	const FString& GetClientNameSafe(const FMessageAddress& InClientAddress) const;
	const FMessageAddress& GetClientAddressSafe(const FString& InClientName) const;
	TArray<FMessageAddress> GetAllClientAddresses(bool bInExcludeClientOnLocalProcess = false) const;
	void RemoveDeadClients(const FDateTime& InCurrentTime);
	void OnClientAdded(const FClientInfo& InClientInfo);
	void OnClientRemoved(const FClientInfo& InRemovedClient);
	/** A local client will run from the same computer and project folder as the current server. */
	bool IsLocalClient(const FClientInfo& ClientInfo) const;
	bool IsLocalClient(const FMessageAddress& InClientAddress) const
	{
		const FClientInfo* ClientInfo = GetClientInfo(InClientAddress);
		return ClientInfo && IsLocalClient(*ClientInfo);
	}
	bool IsClientOnLocalProcess(const FClientInfo& ClientInfo) const
	{
		return ClientInfo.ComputerName == ComputerName && ClientInfo.ProcessId == ProcessId;
	}
};
