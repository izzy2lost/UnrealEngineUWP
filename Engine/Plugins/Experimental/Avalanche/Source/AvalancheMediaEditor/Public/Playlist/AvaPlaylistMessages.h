// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/AvalanchePage.h"

#include "AvaPlaylistMessages.generated.h"

namespace EPlaylistApiVersion
{
	/**
	 * Defines the protocol version of the Playlist Server API.
	 *
	 * API versioning is used to provide legacy support either on
	 * the client side or server side for non compatible changes.
	 * Clients can request a version of the API that they where implemented against,
	 * if the server can still honor the request it will accept.
	 */
	enum Type
	{
		Initial = 1,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}


USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 RequestId = -1;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistServerMsg : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Verbosity;

	UPROPERTY()
	FString Text;
};

/** Request published by client to discover servers. */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPing : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	/** True if the request originates from an automatic timer. False if requests originates from user interaction. */
	UPROPERTY()
	bool bAuto = true;

	/**
	 * API Version the client has been implemented against.
	 * If none (-1) the server will consider the initial version is requested.
	 */
	UPROPERTY()
	int32 RequestedApiVersion = -1;
};

/** Response sent by server to client to be discovered. */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPong : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

public:
	/** True if it is a reply to an auto ping. Mirrors the bAuto flag from Ping message. */
	UPROPERTY()
	bool bAuto = true;

	/**
	 * API Version the server will communicate with for this client.
	 * The server may honor the requested version if possible.
	 * Versions newer than server implementation will obviously not be honored either.
	 * Clients should expect an older server to reply with an older version.
	 */
	UPROPERTY()
	int32 ApiVersion = -1;

	/** Minimum API Version the server implements. */
	UPROPERTY()
	int32 MinimumApiVersion = -1;

	/** Latest API Version the server support. */
	UPROPERTY()
	int32 LatestApiVersion = -1;

	UPROPERTY()
	FString HostName;
};

/**
 *	Request list of playlist that can be opened on the current server.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetPlaylists : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
};

/**
 *	List of all playlists.
 *	Expected Response from FAvaPlaylistGetPlaylists.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPlaylists : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<FString> Playlists;
};

/**
 *	Request that the given playlist be opened.
 *	Only one playlist can be opened at a time. If another playlist
 *	is opened, it will be closed and all currently playing pages stopped.
 *	If the path is empty, nothing will be done and the server will reply with
 *	a FAvaPlaylistLoadedPlaylist message indicating which playlist is currently loaded.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistLoadPlaylist : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	FString Playlist;
};

/**
 * Request the list of pages from the given playlist.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetPages : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Playlist;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistCreatePage : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 TemplateId = FAvalanchePage::InvalidPageId;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistDeletePage : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistCreateTemplate : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistDeleteTemplate : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChangeTemplateBP : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Playlist;
	
	UPROPERTY()
	int32 TemplateId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	FString AssetPath;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPage
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	FString PageName;

	UPROPERTY()
	bool IsTemplate = false;

	UPROPERTY()
	int32 TemplateId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	TArray<int32> CombinedTemplateIds;

	UPROPERTY()
	FSoftObjectPath AssetPath;

	UPROPERTY()
	TArray<FAvalanchePageStatus> Statuses;

	UPROPERTY()
	FString TransitionLayerName;
	
	UPROPERTY()
	FString OutputChannel;

	UPROPERTY()
	bool bIsEnabled = false;

	UPROPERTY()
	bool bIsPlaying = false;
};

/*
 * List of pages from the current playlist.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPages : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaPlaylistPage> Pages;
};

/**
 * Request the page details from the given playlist.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetPageDetails : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	/** This will request that a managed asset instance gets loaded to be
	 * accessible through WebRC. */
	UPROPERTY()
	bool bLoadRemoteControlPreset = false;
};

/**
 *	Server response to FAvaPlaylistGetPageDetails request.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageDetails : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	FAvaPlaylistPage PageInfo;

	UPROPERTY()
	FAvalancheRemoteControlValues RemoteControlValues;

	/** Name of the remote control preset to resolve through WebRC API. */
	UPROPERTY()
	FString RemoteControlPresetName;

	UPROPERTY()
	FString RemoteControlPresetId;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPagesStatuses : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	FAvaPlaylistPage PageInfo;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageListChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	/** See EAvaPageListChange flags. */
	UPROPERTY()
	uint8 ChangeType = 0;

	UPROPERTY();
	TArray<int32> AffectedPages;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageBlueprintChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;
	
	UPROPERTY()
	FString BlueprintPath;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageChannelChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;
	
	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageAnimSettingsChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageChangeChannel : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Playlist;

	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

/** This is a request to save the managed RCP back to the corresponding page. */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistUpdatePageFromRCP : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

public:
	/** Unregister the Remote Control Preset from the WebRC. */
	UPROPERTY()
	bool bUnregister = false;
};

/** Supported Page actions for playback. */
UENUM()
enum class EAvaPlaylistPageActions
{
	None,
	Load,
	Unload,
	Play,
	PlayNext,
	Stop,
	ForceStop,
	Continue,
	UpdateValues,
	TakeToProgram
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageAction : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	EAvaPlaylistPageActions Action = EAvaPlaylistPageActions::None;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPagePreviewAction : public FAvaPlaylistPageAction
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the playlist's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

/**
 * Command to execute an action on multiple pages at the same time.
 * This is necessary for pages to be part of the same transition.
 */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageActions : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<int32> PageIds;

	UPROPERTY()
	EAvaPlaylistPageActions Action = EAvaPlaylistPageActions::None;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPagePreviewActions : public FAvaPlaylistPageActions
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the playlist's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

UENUM()
enum class EAvaPlaylistPageEvents
{
	None,
	AnimStarted,
	AnimPaused,
	AnimFinished
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistPageEvent : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvalanchePage::InvalidPageId;

	UPROPERTY()
	EAvaPlaylistPageEvents Event = EAvaPlaylistPageEvents::None;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistOutputDeviceItem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	/**
	 * Raw Json string representing a serialized UMediaOutput.
	 */
	UPROPERTY()
	FString Data;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistOutputClassItem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FAvaPlaylistOutputDeviceItem> Devices;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistDevicesList : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaPlaylistOutputClassItem> DeviceClasses;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetChannel : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetChannels : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChannel
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FAvaPlaylistOutputDeviceItem> Devices;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaBroadcastChannelListChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAvaPlaylistChannel> Channels;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChannelResponse : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FAvaPlaylistChannel Channel;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChannels : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaPlaylistChannel> Channels;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistAssetsChanged : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetName;
};

// Channel actions
UENUM()
enum class EAvaPlaylistChannelActions
{
	None,
	Start,
	Stop
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChannelAction : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	EAvaPlaylistChannelActions Action = EAvaPlaylistChannelActions::None;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetDevices : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistAddChannelDevice : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistEditChannelDevice : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;

	UPROPERTY()
	FString Data;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistRemoveChannelDevice : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;
};

/* No difference from FAvaPlaylistOutputDeviceItem except this is meant to return a single device response. */
USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistOutputDeviceItemResponse : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Data;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistGetChannelImage : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct AVALANCHEMEDIAEDITOR_API FAvaPlaylistChannelImage : public FAvaPlaylistMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<uint8> ImageData;
};
