// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaBroadcastProfile.h"
#include "AvaPlaylistMessages.h"
#include "MessageEndpoint.h"
#include "Playlist/AvalanchePlaylist.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

class FAvalancheManagedInstance;
class UMediaOutput;

/**
 * Implements a play list server that listens to commands on message bus.
 * The intention is to run a web socket transport bridge so the messages can
 * come from external applications.
 */
class AVALANCHEMEDIAEDITOR_API FAvaPlaylistServer : public TSharedFromThis<FAvaPlaylistServer>
{
public:
	FAvaPlaylistServer();
	virtual ~FAvaPlaylistServer();

	void Init(const FString& InAssignedHostName);

	void SetupBroadcastDelegates(UAvalancheBroadcast* InBroadcast);
	void SetupEditorDelegates();
	void RemoveBroadcastDelegates(UAvalancheBroadcast* InBroadcast) const;
	void RemoveEditorDelegates() const;
	
	void OnPagesChanged(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, EAvaPageChanges InChange) const;
	void OnPageListChanged(const FAvaPageListChangeParams& InParams) const;
	void PageBlueprintChanged(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, const FString& InBlueprintPath) const;
	void PageStatusChanged(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage) const;
	void PageChannelChanged(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, const FString& InChannelName) const;
	void PageAnimSettingsChanged(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage) const;
	void OnBroadcastChannelListChanged(const FAvaBroadcastProfile& InProfile) const;
	void OnAssetAddedOrRemoved(const FAssetData& InAssetData) const;

	/** Returns the endpoint's message address. */
	const FMessageAddress& GetMessageAddress() const;
	
	// Message handlers
	void HandlePlaylistPing(const FAvaPlaylistPing& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetPlaylists(const FAvaPlaylistGetPlaylists& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleLoadPlaylist(const FAvaPlaylistLoadPlaylist& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetPages(const FAvaPlaylistGetPages& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleCreatePage(const FAvaPlaylistCreatePage& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleCreateTemplate(const FAvaPlaylistCreateTemplate& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleDeletePage(const FAvaPlaylistDeletePage& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleDeleteTemplate(const FAvaPlaylistDeleteTemplate& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChangeTemplateBP(const FAvaPlaylistChangeTemplateBP& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetPageDetails(const FAvaPlaylistGetPageDetails& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChangePageChannel(const FAvaPlaylistPageChangeChannel& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleUpdatePageFromRCP(const FAvaPlaylistUpdatePageFromRCP& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePageAction(const FAvaPlaylistPageAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePagePreviewAction(const FAvaPlaylistPagePreviewAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePageActions(const FAvaPlaylistPageActions& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePagePreviewActions(const FAvaPlaylistPagePreviewActions& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	void HandleGetChannel(const FAvaPlaylistGetChannel& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetChannels(const FAvaPlaylistGetChannels& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChannelAction(const FAvaPlaylistChannelAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleAddChannelDevice(const FAvaPlaylistAddChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleEditChannelDevice(const FAvaPlaylistEditChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleRemoveChannelDevice(const FAvaPlaylistRemoveChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetChannelImage(const FAvaPlaylistGetChannelImage& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	
	void HandleGetDevices(const FAvaPlaylistGetDevices& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	
	void LogAndSendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const;
	void SendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const;
	void SendMessageImpl(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InMsg) const;

	void RegisterConsoleCommands();
	
	void ShowStatusCommand(const TArray<FString>& InArgs);

protected:
	struct FRequestInfo
	{
		int32 RequestId;
		FMessageAddress Sender;
	};
	
	struct FChannelImage;
	void FinishGetChannelImage(const FRequestInfo& InRequestInfo, const TSharedPtr<FChannelImage>& InChannelImage);

	void HandlePageActions(const FRequestInfo& InRequestInfo, const TArray<int32>& InPageIds,
		bool bInIsPreview, FName InPreviewChannelName, EAvaPlaylistPageActions InAction) const;
	
	/**
	 * Helper function to retrieve the appropriate playlist for editing commands.
	 */
	UAvalanchePlaylist* GetOrLoadPlaylistForEdit(const FMessageAddress& InSender, int32 InRequestId, const FString& InPlaylistPath);

	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const FMessageAddress& InRecipient, EMessageFlags InFlags = EMessageFlags::None) const
	{
		if (MessageEndpoint)
		{
			MessageEndpoint->Send(InMessage, InRecipient);
		}
	}
	
	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const TArray<FMessageAddress>& InRecipients, EMessageFlags InFlags = EMessageFlags::None) const
	{
		if (MessageEndpoint)
		{
			MessageEndpoint->Send(InMessage, MessageType::StaticStruct(), InFlags, nullptr,
				InRecipients, FTimespan::Zero(), FDateTime::MaxValue());
		}
	}

	void OnMessageBusNotification(const FMessageBusNotification& InNotification);

private:
	FString HostName;	
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;
	TArray<IConsoleObject*> ConsoleCommands;

	/** Keep information on connected clients. */
	struct FClientInfo
	{
		FMessageAddress Address;
		/** Api version for communication with this client. */
		int32 ApiVersion = -1;

		explicit FClientInfo(const FMessageAddress& InAddress) : Address(InAddress) {}
	};

	/** Keep track of remote clients context information. */
	TMap<FMessageAddress, TSharedPtr<FClientInfo>> Clients;

	/** Array of just the client addresses for sending responses. */
	TArray<FMessageAddress> ClientAddresses;
	
	FClientInfo* GetClientInfo(const FMessageAddress& InAddress) const
	{
		const TSharedPtr<FClientInfo>* ClientInfoPtr = Clients.Find(InAddress);
		return ClientInfoPtr ? ClientInfoPtr->Get() : nullptr;
	}
	
	FClientInfo& GetOrAddClientInfo(const FMessageAddress& InAddress)
	{
		if (const TSharedPtr<FClientInfo>* ExistingClientInfo = Clients.Find(InAddress))
		{
			return *ExistingClientInfo->Get();
		}

		const TSharedPtr<FClientInfo> NewClientInfo = MakeShared<FClientInfo>(InAddress);
		Clients.Add(InAddress, NewClientInfo);
		RefreshClientAddresses();
		return *NewClientInfo;
	}

	void RefreshClientAddresses();
	
	/** Pool of images that can be recycled. */
	TArray<TSharedPtr<FChannelImage>> AvailableChannelImages;
	
	/**
	 * Keeps a current playlist cached for commands operating on playlist.
	 * There is only one "current" playlist at a given time.
	 */
	struct FPlaylistCache
	{
		/** Currently loaded/cached playlist's path. */
		FSoftObjectPath CurrentPlaylistPath;
		
		/** Currently loaded/cached playlist object. */
		TStrongObjectPtr<UAvalanchePlaylist> CurrentPlaylist;

		FDelegateHandle OnPlaybackInstanceStatusChangedDelegateHandle;
		
		typedef TFunctionRef<void(UAvalanchePlaylist*)> FPlaylistEventFunction;
		
		/**
		 * Returns requested playlist specified by InPlaylistPath. Will load it if necessary or returned the cached one if it is the same.
		 * If the new playlist fails to load, the return value is nullptr, and the previous playlist will remain loaded.
		 * @param InPlaylistPath	Requested playlist path. 
		 * @param InUnloadCurrentPlaylistFunction	Function called, with the previously cached playlist, when a new playlist is loaded
		 *											and previous playlist needs to be unloaded.
		 * @param InNewPlaylistLoadedFunction	Function called, with the new playlist, when a new playlist is loaded.
		 */
		UAvalanchePlaylist* GetOrLoadPlaylist(const FSoftObjectPath& InPlaylistPath, 
			const FPlaylistEventFunction InUnloadCurrentPlaylistFunction,
			const FPlaylistEventFunction InNewPlaylistLoadedFunction);

		void SetupPlaylistDelegates(FAvaPlaylistServer* InPlaylistServer, UAvalanchePlaylist* InPlaylist);
		void RemovePlaylistDelegates(const FAvaPlaylistServer* InPlaylistServer, UAvalanchePlaylist* InPlaylist) const;
	};
	
	/** Cached data for page editing commands (GetPages and GetPageDetails). */
	struct FPlaylistEditCommandData : public FPlaylistCache
	{
		/** PageId of the current managed ava blueprint. */
		int32 ManagedPageId = FAvalanchePage::InvalidPageId;
		TSharedPtr<FAvalancheManagedInstance> ManagedInstance;

		~FPlaylistEditCommandData();
		
		/**
		 * Checks if previous RCP was registered.
		 * If so, save modified values to corresponding page.
 		 * This may result in the Playlist to be modified.
 		 * Will also unregister RCP from RC Module if requested.
		 */
		void SaveCurrentRemoteControlPresetToPage(bool bInUnregister);
	};
	FPlaylistEditCommandData PlaylistEditCommandData;

	/** Cached data for playback commands (LoadPlaylist and PageAction). */
	struct FPlaylistPlaybackCommandData: public FPlaylistCache
	{
		~FPlaylistPlaybackCommandData();

		void ClosePlaybackContext();
	};
	FPlaylistPlaybackCommandData PlaylistPlaybackCommandData;
};
