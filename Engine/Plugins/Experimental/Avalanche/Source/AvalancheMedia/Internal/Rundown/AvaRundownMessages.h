// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Broadcast/Channel/AvaBroadcastMediaOutputInfo.h"
#include "Rundown/AvaRundownPage.h"
#include "Viewport/AvaViewportQualitySettings.h"
#include "AvaRundownMessages.generated.h"

namespace EAvaRundownApiVersion
{
	/**
	 * Defines the protocol version of the Rundown Server API.
	 *
	 * API versioning is used to provide legacy support either on
	 * the client side or server side for non compatible changes.
	 * Clients can request a version of the API that they where implemented against,
	 * if the server can still honor the request it will accept.
	 */
	enum Type
	{
		Unspecified = -1,
		
		Initial = 1,
		/**
		 * The rundown server has been moved to the runtime module.
		 * All message scripts paths moved from AvalancheMediaEditor to AvalancheMedia.
		 * However, all server requests messages have been added to core redirect, so
		 * previous path will still get through, but all response messages will be the new path.
		 * Clients can still issue a ping with the old path and will get a response.
		 */ 
		MoveToRuntime = 2,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

/**
 * Build targets.
 * This will help determine the set of features that are available.
 */
UENUM()
enum class EAvaRundownServerBuildTargetType : uint8
{
	Unknown = 0,
	Editor,
	Game,
	Server,
	Client,
	Program
};

/**
 * An editor build can be launched in different modes but it could also be
 * a dedicated build target. The engine mode combined with the build target
 * will determine the set of functionalities available.
 */
UENUM()
enum class EAvaRundownServerEngineMode : uint8
{
	Unknown = 0,
	Editor,
	Game,
	Server,
	Commandlet,
	Other
};


USTRUCT()
struct FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 RequestId = INDEX_NONE;
};

USTRUCT()
struct FAvaRundownServerMsg : public FAvaRundownMsgBase
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
struct FAvaRundownPing : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	/** True if the request originates from an automatic timer. False if requests originates from user interaction. */
	UPROPERTY()
	bool bAuto = true;

	/**
	 * API Version the client has been implemented against.
	 * If unspecified the server will consider the latest version is requested.
	 */
	UPROPERTY()
	int32 RequestedApiVersion = EAvaRundownApiVersion::Unspecified;
};

/** Response sent by server to client to be discovered. */
USTRUCT()
struct FAvaRundownPong : public FAvaRundownMsgBase
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
	int32 ApiVersion = EAvaRundownApiVersion::Unspecified;

	/** Minimum API Version the server implements. */
	UPROPERTY()
	int32 MinimumApiVersion = EAvaRundownApiVersion::Unspecified;

	/** Latest API Version the server support. */
	UPROPERTY()
	int32 LatestApiVersion = EAvaRundownApiVersion::Unspecified;

	UPROPERTY()
	FString HostName;
};

/**
 * Request the extended server information.
 */
USTRUCT()
struct FAvaRundownGetServerInfo : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

/**
 * Extended server information 
 */
USTRUCT()
struct FAvaRundownServerInfo : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	/** API Version the server will communicate with for this client. */
	UPROPERTY()
	int32 ApiVersion = EAvaRundownApiVersion::Unspecified;

	/** Minimum API Version the server implements. */
	UPROPERTY()
	int32 MinimumApiVersion = EAvaRundownApiVersion::Unspecified;

	/** Latest API Version the server support. */
	UPROPERTY()
	int32 LatestApiVersion = EAvaRundownApiVersion::Unspecified;

	UPROPERTY()
	FString HostName;

	/** Holds the engine version checksum */
	UPROPERTY()
	uint32 EngineVersion = 0;
	
	/** Holds the instance identifier. */
	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	EAvaRundownServerBuildTargetType InstanceBuild = EAvaRundownServerBuildTargetType::Unknown;

	UPROPERTY()
	EAvaRundownServerEngineMode InstanceMode = EAvaRundownServerEngineMode::Unknown;

	/** Holds the identifier of the session that the application belongs to. */
	UPROPERTY()
	FGuid SessionId;
	
	/** The unreal project name this server is running from. */
	UPROPERTY()
	FString ProjectName;
	
	/** The unreal project directory this server is running from. */
	UPROPERTY()
	FString ProjectDir;

	/** Http Server Port of the remote control service. */
	UPROPERTY()
	uint32 RemoteControlHttpServerPort = 0;

	/** WebSocket Server Port of the remote control service. */
	UPROPERTY()
	uint32 RemoteControlWebSocketServerPort = 0;
};

/**
 *	Request list of rundown that can be opened on the current server.
 */
USTRUCT()
struct FAvaRundownGetRundowns : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

/**
 *	List of all rundowns.
 *	Expected Response from FAvaRundownGetRundowns.
 */
USTRUCT()
struct FAvaRundownRundowns : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> Rundowns;
};

/**
 *	Request that the given rundown be loaded for playback.
 *	This will also open an associated playback context.
 *	Only one rundown can be opened for playback at a time by the rundown server.
 *	If another rundown is opened, the previous one will be closed and all currently playing pages stopped,
 *	unless the rundown editor is opened. The rundown editor will keep the playback context alive.
 *	
 *	If the path is empty, nothing will be done and the server will reply with
 *	a FAvaRundownServerMsg message indicating which rundown is currently loaded.
 */
USTRUCT()
struct FAvaRundownLoadRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/**
	 * Rundown asset path: [PackagePath]/[AssetName].[AssetName]
	 */
	UPROPERTY()
	FString Rundown;
};

/**
 * Request to create a new rundown asset.
 *
 * The full package name is going to be: [PackagePath]/[AssetName] 
 * The full asset path is going to be: [PackagePath]/[AssetName].[AssetName]
 * For all other requests, the rundown reference is the full asset path.
 */
USTRUCT()
struct FAvaRundownCreateRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Package path (excluding the package name) */
	UPROPERTY()
	FString PackagePath;

	/** Asset Name. */
	UPROPERTY()
	FString AssetName;

	/**
	 * Create the rundown as a transient object.
	 * @remark For game builds, the created rundown will always be transient, regardless of this flag. 
	 */
	UPROPERTY()
	bool bTransient = true;
};

/**
 * Request a previously created rundown to be deleted or at least no longer managed (if transient only). 
 */
USTRUCT()
struct FAvaRundownDeleteRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Rundown asset path: [PackagePath]/[AssetName].[AssetName] */
	UPROPERTY()
	FString Rundown;
};

/**
 * Import rundown from json data or file.
 */
USTRUCT()
struct FAvaRundownImportRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Rundown asset path: [PackagePath]/[AssetName].[AssetName] */
	UPROPERTY()
	FString Rundown;

	/**
	 * If specified, this is a server local path to a json file from which the rundown will be imported.
	 */
	UPROPERTY()
	FString RundownFile;

	/**
	 * If specified, json data containing the rundown to import.
	 */
	UPROPERTY()
	FString RundownData;
};

/**
 * Export a rundown to json data or file.
 * This command is supported in game build.
 */
USTRUCT()
struct FAvaRundownExportRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Rundown asset path: [PackagePath]/[AssetName].[AssetName] */
	UPROPERTY()
	FString Rundown;

	/** Optional path to a server local file where the rundown will be saved. */
	UPROPERTY()
	FString RundownFile;
};

/**
 * Server reply to FAvaRundownExportRundown containing the exported rundown.
 */
USTRUCT()
struct FAvaRundownExportedRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Rundown asset path: [PackagePath]/[AssetName].[AssetName] */
	UPROPERTY()
	FString Rundown;

	/** Exported rundown in json format. */
	UPROPERTY()
	FString RundownData;
};

/**
 * Request that the given rundown be saved to disk.
 * The rundown asset must have been loaded, either by an edit command
 * or playback, prior to this command.
 * Unloaded assets will not be loaded by this command.
 * This command is not supported in game builds.
 */
USTRUCT()
struct FAvaRundownSaveRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	bool bOnlyIfIsDirty = false;
};

/**
 * Rundown specific events broadcast by the server to help status display or related contexts in control applications.
 */
USTRUCT()
struct FAvaRundownPlaybackContextChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/**
	 * Previous rundown (can be empty).
	 */
	UPROPERTY()
	FString PreviousRundown;
	
	/**
	 * New current rundown (can be empty).
	 */
	UPROPERTY()
	FString NewRundown;
};

/**
 * Request the list of pages from the given rundown.
 */
USTRUCT()
struct FAvaRundownGetPages : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Rundown;
};

USTRUCT()
struct FAvaRundownCreatePage : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownDeletePage : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownCreateTemplate : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;
};

USTRUCT()
struct FAvaRundownDeleteTemplate : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownChangeTemplateBP : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString AssetPath;
};

USTRUCT()
struct FAvaRundownPageInfo
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString PageName;

	UPROPERTY()
	FString PageSummary;

	UPROPERTY()
	FString FriendlyName;

	UPROPERTY()
	bool IsTemplate = false;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	TArray<int32> CombinedTemplateIds;

	UPROPERTY()
	FSoftObjectPath AssetPath;

	UPROPERTY()
	TArray<FAvaRundownChannelPageStatus> Statuses;

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
 * List of pages from the current rundown.
 */
USTRUCT()
struct FAvaRundownPages : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownPageInfo> Pages;
};

/**
 * Request the page details from the given rundown.
 */
USTRUCT()
struct FAvaRundownGetPageDetails : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	/** This will request that a managed asset instance gets loaded to be
	 * accessible through WebRC. */
	UPROPERTY()
	bool bLoadRemoteControlPreset = false;
};

/**
 *	Server response to FAvaRundownGetPageDetails request.
 */
USTRUCT()
struct FAvaRundownPageDetails : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	FAvaRundownPageInfo PageInfo;

	UPROPERTY()
	FAvaPlayableRemoteControlValues RemoteControlValues;

	/** Name of the remote control preset to resolve through WebRC API. */
	UPROPERTY()
	FString RemoteControlPresetName;

	UPROPERTY()
	FString RemoteControlPresetId;
};

USTRUCT()
struct FAvaRundownPagesStatuses : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	FAvaRundownPageInfo PageInfo;
};

USTRUCT()
struct FAvaRundownPageListChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	EAvaRundownPageListType ListType = EAvaRundownPageListType::Instance;

	UPROPERTY()
	FGuid SubListId;
	
	/** See EAvaPageListChange flags. */
	UPROPERTY()
	uint8 ChangeType = 0;

	UPROPERTY();
	TArray<int32> AffectedPages;
};

USTRUCT()
struct FAvaRundownPageBlueprintChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString BlueprintPath;
};

USTRUCT()
struct FAvaRundownPageChannelChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownPageAnimSettingsChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownPageChangeChannel : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

/** This is a request to save the managed RCP back to the corresponding page. */
USTRUCT()
struct FAvaRundownUpdatePageFromRCP : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Unregister the Remote Control Preset from the WebRC. */
	UPROPERTY()
	bool bUnregister = false;
};

/** Supported Page actions for playback. */
UENUM()
enum class EAvaRundownPageActions
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
struct FAvaRundownPageAction : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	EAvaRundownPageActions Action = EAvaRundownPageActions::None;
};

USTRUCT()
struct FAvaRundownPagePreviewAction : public FAvaRundownPageAction
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the rundown's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

/**
 * Command to execute an action on multiple pages at the same time.
 * This is necessary for pages to be part of the same transition.
 */
USTRUCT()
struct FAvaRundownPageActions : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<int32> PageIds;

	UPROPERTY()
	EAvaRundownPageActions Action = EAvaRundownPageActions::None;
};

USTRUCT()
struct FAvaRundownPagePreviewActions : public FAvaRundownPageActions
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the rundown's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

UENUM()
enum class EAvaRundownPageEvents
{
	None,
	AnimStarted,
	AnimPaused,
	AnimFinished
};

USTRUCT()
struct FAvaRundownPageEvent : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	EAvaRundownPageEvents Event = EAvaRundownPageEvents::None;
};

USTRUCT()
struct FAvaRundownGetProfiles : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FAvaRundownProfiles : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	/** List of all profiles. */
	UPROPERTY()
	TArray<FString> Profiles;

	/** Current Active Profile. */
	UPROPERTY()
	FString CurrentProfile;
};

/**
 * Creates a new empty profile with the given name.
 * Fails if the profile already exist.
 */
USTRUCT()
struct FAvaRundownCreateProfile : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ProfileName;

	/**
	 * If true the created profile is make "current".
	 */
	UPROPERTY()
	bool bMakeCurrent = true;
};

/**
 * Duplicates an existing profile.
 * Fails if the new profile name already exist.
 * Fails if the source profile does not exist.
 */
USTRUCT()
struct FAvaRundownDuplicateProfile : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString SourceProfileName;

	UPROPERTY()
	FString NewProfileName;

	/**
	 * If true the created profile is make "current".
	 */
	UPROPERTY()
	bool bMakeCurrent = true;
};

USTRUCT()
struct FAvaRundownRenameProfile : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString OldProfileName;

	UPROPERTY()
	FString NewProfileName;
};

/**
 * Delete the specified profile.
 * Fails if profile to be deleted is the current profile.
 */
USTRUCT()
struct FAvaRundownDeleteProfile : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ProfileName;
};

/**
 * Specified profile is made "current".
 * The current profile becomes the context for all other broadcasts commands.
 * Fails if some channels are currently broadcasting.
 */
USTRUCT()
struct FAvaRundownSetCurrentProfile : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ProfileName;
};

USTRUCT()
struct FAvaRundownOutputDeviceItem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FAvaBroadcastMediaOutputInfo OutputInfo;
	
	UPROPERTY()
	EAvaBroadcastOutputState OutputState = EAvaBroadcastOutputState::Invalid;
	
	UPROPERTY()
	EAvaBroadcastIssueSeverity IssueSeverity = EAvaBroadcastIssueSeverity::None;

	UPROPERTY()
	TArray<FString> IssueMessages;

	/**
	 * Raw Json string representing a serialized UMediaOutput.
	 */
	UPROPERTY()
	FString Data;
};

USTRUCT()
struct FAvaRundownOutputClassItem
{
	GENERATED_BODY()
public:
	/** Class name */
	UPROPERTY()
	FString Name;

	/**
	 * Name of the playback server this class was seen on.
	 * The name will be empty for the "local process" device.
	 */
	UPROPERTY()
	FString Server;

	UPROPERTY()
	TArray<FAvaRundownOutputDeviceItem> Devices;
};

USTRUCT()
struct FAvaRundownDevicesList : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownOutputClassItem> DeviceClasses;
};

USTRUCT()
struct FAvaRundownGetChannel : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownGetChannels : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FAvaRundownChannel
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	EAvaBroadcastChannelType Type = EAvaBroadcastChannelType::Program;

	UPROPERTY()
	EAvaBroadcastChannelState State = EAvaBroadcastChannelState::Offline;

	UPROPERTY()
	EAvaBroadcastIssueSeverity IssueSeverity = EAvaBroadcastIssueSeverity::None;

	UPROPERTY()
	TArray<FAvaRundownOutputDeviceItem> Devices;
};

USTRUCT()
struct FAvaRundownChannelListChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAvaRundownChannel> Channels;
};

USTRUCT()
struct FAvaRundownChannelResponse : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FAvaRundownChannel Channel;
};

USTRUCT()
struct FAvaRundownChannels : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownChannel> Channels;
};

/**
 * Generic asset event
 */
UENUM()
enum class EAvaRundownAssetEvent : uint8
{
	Unknown = 0,
	Added,
	Removed,
	//Saved, // todo
	//Modified // todo
};

/**
 * Event broadcast when an asset event occurs on the server.
 */
USTRUCT()
struct FAvaRundownAssetsChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	/** Asset name only, without the package path. (Keeping for legacy) */
	UPROPERTY()
	FString AssetName;

	/** Full asset path: /PackagePath/PackageName.AssetName */
	UPROPERTY()
	FString AssetPath;

	/** Full asset class path. */
	UPROPERTY()
	FString AssetClass;

	/** true if the asset is a "playable" asset, i.e. an asset that can be set in a page's asset. */
	UPROPERTY()
	bool bIsPlayable = false;

	UPROPERTY()
	EAvaRundownAssetEvent EventType = EAvaRundownAssetEvent::Unknown;
};

/**
 * Channel actions
 */
UENUM()
enum class EAvaRundownChannelActions
{
	None,
	Start,
	Stop
};

USTRUCT()
struct FAvaRundownChannelAction : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	EAvaRundownChannelActions Action = EAvaRundownChannelActions::None;
};

UENUM()
enum class EAvaRundownChannelEditActions
{
	None,
	Add,
	Remove
};

USTRUCT()
struct FAvaRundownChannelEditAction : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	EAvaRundownChannelEditActions Action = EAvaRundownChannelEditActions::None;
};

USTRUCT()
struct FAvaRundownRenameChannel : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString OldChannelName;

	UPROPERTY()
	FString NewChannelName;
};

/**
 * Request a list of devices from the rundown server.
 * The server will reply with FAvaRundownDevicesList containing
 * the devices that can be enumerated from the local host and all connected hosts
 * through the motion design playback service.
 */
USTRUCT()
struct FAvaRundownGetDevices : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	/**
	 * If true, listing all media output classes on the server, even if they don't have a device provider.
	 */
	UPROPERTY()
	bool bShowAllMediaOutputClasses = false;
};

/**
 * Add an enumerated device to the given channel.
 * This command will fail if the channel is live.
 */
USTRUCT()
struct FAvaRundownAddChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	/**
	 * The specified name is one of the enumerated device from FAvaRundownDevicesList,
	 * FAvaRundownOutputDeviceItem::Name.
	 */
	UPROPERTY()
	FString MediaOutputName;

	UPROPERTY()
	bool bSaveBroadcast = true;
};

/**
 * Modify an existing device in the given channel.
 * This command will fail if the channel is live.
 */
USTRUCT()
struct FAvaRundownEditChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	/**
	 * The specified name is one of the enumerated device from FAvaRundownChannel::Devices,
	 * FAvaRundownOutputDeviceItem::Name field.
	 * Must be the instanced devices from either FAvaRundownChannels, FAvaRundownChannelResponse
	 * or FAvaRundownChannelListChanged. These names are not the same as when adding a device.
	 */
	UPROPERTY()
	FString MediaOutputName;

	UPROPERTY()
	FString Data;

	UPROPERTY()
	bool bSaveBroadcast = true;
};

/**
 * Remove an existing device from the given channel.
 * This command will fail if the channel is live.
 */
USTRUCT()
struct FAvaRundownRemoveChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	/**
	 * The specified name is one of the enumerated device from FAvaRundownChannel::Devices,
	 * FAvaRundownOutputDeviceItem::Name field.
	 * Must be the instanced devices from either FAvaRundownChannels, FAvaRundownChannelResponse
	 * or FAvaRundownChannelListChanged. These names are not the same as when adding a device.
	 */
	UPROPERTY()
	FString MediaOutputName;

	UPROPERTY()
	bool bSaveBroadcast = true;
};

USTRUCT()
struct FAvaRundownGetChannelImage : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownChannelImage : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<uint8> ImageData;
};

/**
 * Queries the given channel's quality settings.
 * Response message is FAvaRundownChannelQualitySettings.
 */
USTRUCT()
struct FAvaRundownGetChannelQualitySettings : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

/** Response to FAvaRundownGetChannelQualitySettings. */
USTRUCT()
struct FAvaRundownChannelQualitySettings : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	/** Advanced viewport client engine features indexed by FEngineShowFlags names. */
	UPROPERTY()
	TArray<FAvaViewportQualitySettingsFeature> Features;
};

/** Sets the given channel's quality settings. */
USTRUCT()
struct FAvaRundownSetChannelQualitySettings : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	/** Advanced viewport client engine features indexed by FEngineShowFlags names. */
	UPROPERTY()
	TArray<FAvaViewportQualitySettingsFeature> Features;
};

/** Save current broadcast configuration to file on the server. */
USTRUCT()
struct FAvaRundownSaveBroadcast : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};