// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Channel/AvaMediaOutputInfo.h"
#include "Framework/AvalancheInstanceSettings.h"
#include "PixelFormat.h"
#include "Playback/AvalancheRemoteControlValues.h"
#include "Playback/Nodes/Events/Actions/AvalancheAnimations.h"
#include "Viewport/AvaViewportQualitySettings.h"


#include "AvaMediaPlaybackMessages.generated.h"

USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClientName;
};

/**
 * Notes:
 * - ServerName vs HostName
 * The Host name is the physical computer device the playback server runs on.
 * A Server name is what the playback server is called and it is not necessarily the Host name.
 * If there are multiple servers on the same host, then each server must have a different name.
 * The Server name is a unique key to identify a server, while the host name is not.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ServerName;
};

/**
 * Request published by client to discover servers.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackPing : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	/** Indicate if the ping was generated on a timer (auto) or manually sent through a user command. */
	UPROPERTY()
	bool bAutoPing = true;

	/** Defines the interval in seconds between each client's pings. */
	UPROPERTY()
	float PingIntervalSeconds = 0.0f;
};

/**
 *	Response sent by server to client to be discovered.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackPong : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	/** Indicate if this is a reply from an auto ping. */
	UPROPERTY()
	bool bAutoPong = true;

	/** Server may request client info in case of a reconnection event. */
	UPROPERTY()
	bool bRequestClientInfo = false;

	/** Server's project content path. */
	UPROPERTY()
	FString ProjectContentPath;

	/** Server's process id on the current host. */
	UPROPERTY()
	uint32 ProcessId = 0;
};

/**
 *	Replicate server's log messages.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackLog : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Text;

	UPROPERTY()
	uint8 Verbosity = 0;

	UPROPERTY()
	FName Category;

	UPROPERTY()
	double Time = 0.0;
};

/**
 * Request sent by client to replicate it's information on the destination server.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaUpdateClientInfo : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ComputerName;

	UPROPERTY()
	FString ProjectContentPath;

	UPROPERTY()
	uint32 ProcessId = 0;
};

/**
 * Request sent by client to replicate it's user data on the destination server.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaUpdateClientUserData : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> UserDataEntries;
};

/**
 * Request sent by server to replicate it's user data to the client.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaUpdateServerUserData : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> UserDataEntries;
};

USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaStatCommand : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Command;

	/**
	 * If the local client couldn't get a viewport client to run the stat command, the
	 * local state will not be reliable. In this case, the server state will be used.
	 */
	UPROPERTY()
	bool bClientStateReliable = false;
	
	/**
	 * Because the commands are toggles, there may be desync of states between client and server(s).
	 * In order to correct that we also send a list of enabled states on the client so that servers can
	 * ensure they have the same states enabled.
	 */
	UPROPERTY()
	TArray<FString> ClientEnabledRuntimeStats;
};

/**
 * Server's response when a stat command is received.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaStatStatus : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	/**
	 * Replication of the same field from the command.
	 * This is used by the client to replicate the server state in case
	 * the command succeeded on the server and it had an unreliable state.
	 */
	UPROPERTY()
	bool bClientStateReliable = false;
	
	/** Indicate if the server could execute the command. */
	UPROPERTY()
	bool bCommandSucceeded = false;

	/** Resulting enabled stats from the command. */
	UPROPERTY()
	TArray<FString> EnabledRuntimeStats;
};

/**
 * Request sent by client to obtain the device provider data from destination server.
 * The device provider data contains all available devices and their configuration
 * installed on the server.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaDeviceProviderDataRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()
};

/**
 *	Request for the client to replicate it's Motion Design instance settings to the server.
 **/
USTRUCT()
struct AVALANCHEMEDIA_API FAvalancheInstanceSettingsUpdate : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FAvalancheInstanceSettings InstanceSettings;
};

UENUM()
enum class EAvaMediaPlaybackPackageEvent
{
	None,
	PreSave,
	PostSave,
	AssetDeleted
};

/**
 *	Message sent by the client to inform servers that a local package has been modified by an event.
 **/
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackPackageEvent : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FName PackageName;

	UPROPERTY()
	EAvaMediaPlaybackPackageEvent Event = EAvaMediaPlaybackPackageEvent::None;
};

/**
 * Request by a client to obtain the status of an asset with the given path.
 * Note: since this is an asset on disk, there is no channel name.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackAssetStatusRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()
	
	UPROPERTY()
	FSoftObjectPath AssetPath;
	
	/**
	 * If this is true, the server will perform a full asset status compare with the client.
	 * Otherwise, the server may use locally cached status for the given asset.
	 */
	UPROPERTY()
	bool bForceRefresh = false;
};

/** Response from the server to a playback asset status request. */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackAssetStatus : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()
	
	UPROPERTY()
	FSoftObjectPath AssetPath;
	
	UPROPERTY()
	EAvaMediaPlaybackAssetStatus Status = EAvaMediaPlaybackAssetStatus::Unknown;
};

UENUM()
enum class EAvaMediaPlaybackAction
{
	/** No op */
	None,
	/** Load the given asset. Used for pre-loading assets. */
	Load,
	/** Start (i.e. start ticking world and rendering) the given asset, loading it if not pre-loaded. */
	Start,
	/** Stop ticking and rendering the world. */
	Stop,
	/** Unload the given asset, i.e. destroy the world, etc. */
	Unload,
	/** Request the status of the asset. No action is actually performed on the asset. */
	Status,
	/** Request to set the user data of the playback instance.*/
	SetUserData,
	/** Request the user data of the playback instance. No action is actually performed on the asset. */
	GetUserData,
};

/**
 * Playback command for a given asset.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackCommand
{
	GENERATED_BODY()

	/** Playback's instance Id. */
	UPROPERTY()
	FGuid InstanceId;

	/** Reference to the asset to playback. */
	UPROPERTY()
	FSoftObjectPath AssetPath;

	/** Channel to Play, only considered when the Asset in question is an Motion Design Playable (Blueprint or Level). */
	UPROPERTY()
	FString ChannelName;
	
	/** Action to do: Load, Play, Stop, Unload, etc. */
	UPROPERTY()
	EAvaMediaPlaybackAction Action = EAvaMediaPlaybackAction::None;

	/** Command additional arguments. */
	UPROPERTY()
	FString Arguments;
};

/**
 * Request by a client to execute a batch playback of commands on the connected servers.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FAvaMediaPlaybackCommand> Commands;
};

/** Response from the server to a playback request. */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackStatus : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	/** Playback's instance Id. */
	UPROPERTY()
	FGuid InstanceId;
	
	UPROPERTY()
	FSoftObjectPath AssetPath;

	UPROPERTY()
	FString ChannelName;
	
	UPROPERTY()
	EAvaMediaPlaybackStatus Status = EAvaMediaPlaybackStatus::Unknown;
	
	UPROPERTY()
	bool bValidUserData = false;
	
	UPROPERTY()
	FString UserData;
};

/**
 * Message batching the status of many assets per channel/host.
 * Used to reduce message overhead.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlaybackStatuses : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString ChannelName;

	/** Playback's instance Id. */
	UPROPERTY()
	TArray<FGuid> InstanceIds;
	
	UPROPERTY()
	TArray<FSoftObjectPath> AssetPaths;

	/** Batches all assets with the same status. */
	UPROPERTY()
	EAvaMediaPlaybackStatus Status = EAvaMediaPlaybackStatus::Unknown;
};

USTRUCT()
struct FAvaMediaAnimActionInfo
{
	GENERATED_BODY()
    
	UPROPERTY()
	FString AnimationName;

	UPROPERTY()
	EAvaMediaAnimAction AnimationAction = EAvaMediaAnimAction::None;
};

/**
 * Request by a client to execute an animation on a playback asset
 * on a connected server.
 */
USTRUCT()
struct FAvaMediaAnimPlaybackRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	/** Playback's instance Id. */
	UPROPERTY()
	FGuid InstanceId;
	
	UPROPERTY()
	FSoftObjectPath AssetPath;
	
	/** Channel to Play, only considered when the Asset in question is an Motion Design Blueprint */
	UPROPERTY()
	FString ChannelName;
	
	UPROPERTY()
	TArray<FAnimPlaySettings> AnimPlaySettings;

	UPROPERTY()
	TArray<FAvaMediaAnimActionInfo> AnimActionInfos;
};

/** Server replication of playback sequence events. */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaPlayableSequenceEvent : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	/** Playback's instance Id. */
	UPROPERTY()
	FGuid InstanceId;
	
	UPROPERTY()
	FSoftObjectPath AssetPath;

	UPROPERTY()
	FString ChannelName;
	
	UPROPERTY()
	FString SequenceName;

	UPROPERTY()
	EAvalanchePlayableSequenceEventType EventType = EAvalanchePlayableSequenceEventType::None;
};

USTRUCT()
struct FAvaMediaRemoteControlUpdateRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	/** Playback's instance Id. */
	UPROPERTY()
	FGuid InstanceId;
	
	UPROPERTY()
	FSoftObjectPath AssetPath;
	
	/** Channel to Play, only considered when the Asset in question is an Motion Design Blueprint. */
	UPROPERTY()
	FString ChannelName;
	
	UPROPERTY()
	FAvalancheRemoteControlValues RemoteControlValues;
};

USTRUCT()
struct FAvaMediaTransitionStartRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FGuid TransitionId;
	
	UPROPERTY()
	TArray<FGuid> EnterInstanceIds;

	UPROPERTY()
	TArray<FGuid> PlayingInstanceIds;

	UPROPERTY()
	TArray<FGuid> ExitInstanceIds;

	UPROPERTY()
	TArray<FAvalancheRemoteControlValues> EnterValues;

	UPROPERTY()
	bool bUnloadDiscardedInstances = false;

	/** See EAvalanchePlayableTransitionFlags. */
	UPROPERTY()
	uint8 TransitionFlags = 0;

	EAvalanchePlayableTransitionFlags GetTransitionFlags() const { return static_cast<EAvalanchePlayableTransitionFlags>(TransitionFlags);}
	void SetTransitionFlags(EAvalanchePlayableTransitionFlags InTransitionFlags) { TransitionFlags = static_cast<uint8>(InTransitionFlags);}

};

USTRUCT()
struct FAvaMediaTransitionStopRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FGuid TransitionId;
	
	UPROPERTY()
	bool bUnloadDiscardedInstances = false;
};

USTRUCT()
struct FAvaMediaPlayableTransitionEvent : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FGuid TransitionId;
	
	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	uint8 EventFlags = static_cast<uint8>(EAvalanchePlayableTransitionEventFlags::None);

	// Because the enum is used as flags, we need to convert to uint8 manually. (Can't use TEnumAsByte for this apparently.)
	EAvalanchePlayableTransitionEventFlags GetEventFlags() const { return static_cast<EAvalanchePlayableTransitionEventFlags>(EventFlags);}
	void SetEventFlags(EAvalanchePlayableTransitionEventFlags InFlags) { EventFlags = static_cast<uint8>(InFlags);}
};

UENUM()
enum class EAvaMediaBroadcastAction
{
	None,
	Start,
	Stop,
	UpdateConfig,
	DeleteChannel
};

class UMediaOutput;

/**
 * Encapsulates a UMediaOutput object in binary form to be able to serialize it
 * nested within the broadcast request.
 */
USTRUCT()
struct FAvaMediaOutputData
{
	GENERATED_BODY()

	UPROPERTY()
	FAvaMediaOutputInfo OutputInfo;

	UPROPERTY()
	TSoftClassPtr<UMediaOutput> MediaOutputClass;
	
	UPROPERTY()
	TArray<uint8> SerializedData;
	
	UPROPERTY()
	uint64 ObjectFlags = 0;

	EObjectFlags GetObjectFlags() const { return static_cast<EObjectFlags>(ObjectFlags); }
};

/** Request by a client to execute an action on broadcast channel(s). */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()
	
	/** Profile to select for broadcast. */
	UPROPERTY()
	FString Profile;

	/** Channel name. If empty, command is for all channels. */
	UPROPERTY()
	FString Channel;

	UPROPERTY()
	TArray<FAvaMediaOutputData> MediaOutputs;

	/** Action to do on channel: Start, Stop, Disable, etc. */
	UPROPERTY()
	EAvaMediaBroadcastAction Action = EAvaMediaBroadcastAction::None;
};

/** Request by a client to update a broadcast channel's settings. */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastChannelSettingsUpdate : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()
	
	/** Profile to select for broadcast. */
	UPROPERTY()
	FString Profile;

	/** Channel name. If empty, command is for all channels. */
	UPROPERTY()
	FString Channel;

	UPROPERTY()
	FAvaViewportQualitySettings QualitySettings;
};

/**
 * Client sends this request to have a status of broadcast.
 *
 * This is used by the client when it connects to a server which
 * might be already broadcasting. This happens if the client
 * disconnects and reconnects. It needs to query the full
 * status of the server upon connection.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastStatusRequest : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIncludeMediaOutputData = false;
};

USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastSettings
{
	GENERATED_BODY()

	/** Specifies the background clear color for the channel. */
	UPROPERTY()
	FLinearColor ChannelClearColor = FLinearColor::Black;

	/** Pixel format used if no media output has specific format requirement. */
	UPROPERTY()
	TEnumAsByte<EPixelFormat> ChannelDefaultPixelFormat = EPixelFormat::PF_B8G8R8A8;

	/** Resolution used if no media output has specific resolution requirement. */
	UPROPERTY()
	FIntPoint ChannelDefaultResolution = FIntPoint(1920, 1080);

	/**
	 * Enables drawing the placeholder widget when there is no avalanche asset playing.
	 * If false, the channel is cleared to the background color.
	 */
	UPROPERTY()
	bool bDrawPlaceholderWidget = false;
	
	/** Specify a place holder widget to render when no avalanche asset is playing. */
	UPROPERTY()
	FSoftObjectPath PlaceholderWidgetClass;
};

/**
 *	Request for the client to replicate it's broadcast settings to the server.
 **/
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastSettingsUpdate : public FAvaMediaPlaybackClientMessageBase
{
	GENERATED_BODY()

	UPROPERTY()
	FAvaMediaBroadcastSettings BroadcastSettings;
};

/**
 * Encapsulate the complete status of a media output.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaOutputStatus
{
	GENERATED_BODY()

	UPROPERTY()
	EAvaMediaOutputState MediaOutputState = EAvaMediaOutputState::Invalid;
	
	UPROPERTY()
	EAvaMediaIssueSeverity MediaIssueSeverity = EAvaMediaIssueSeverity::None;

	UPROPERTY()
	TArray<FString> MediaIssueMessages;
};

/**
 * Server's response when channel's broadcast status changes
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvaMediaBroadcastStatus : public FAvaMediaPlaybackServerMessageBase
{
	GENERATED_BODY()
	
	/** Profile to select for broadcast. */
	UPROPERTY()
	FString Profile;
	
	/** Channel name. If empty, command is for all channels of the profile. */
	UPROPERTY()
	FString ChannelName;

	/** Index of the channel in the profile. */
	UPROPERTY()
	int32 ChannelIndex = 0;

	/** Number of channels in the profile. */
	UPROPERTY()
	int32 NumChannels = 0;

	UPROPERTY()
	EAvaChannelState ChannelState = EAvaChannelState::Idle;

	UPROPERTY()
	EAvaMediaIssueSeverity ChannelIssueSeverity = EAvaMediaIssueSeverity::None;

	UPROPERTY()
	TMap<FGuid, FAvaMediaOutputStatus> MediaOutputStatuses;
	
	/**
	 * This is required to know if the MediaOutputs is empty because the data
	 * was not included or if it is because the data was included and there is
	 * actually no outputs defined on that channel.
	 */
	UPROPERTY()
	bool bIncludeMediaOutputData = false;
	
	/** Remark: this is only included if bIncludeMediaOutputData == true in the request. */
	UPROPERTY()
	TArray<FAvaMediaOutputData> MediaOutputs;
};
