// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Misc/Guid.h"

struct FTraceStatus
{
	/**
	 * Update types
	 */
	enum class EUpdateType : uint8
	{
		Status			= 1 << 0,
		Settings		= 1 << 1,
		ChannelsDesc	= 1 << 2,
		ChannelsStatus	= 1 << 3,
		All				= Status|Settings|ChannelsDesc|ChannelsStatus
	};
	
	struct FSettings
	{
		/** If worker thread is used or TraceLog is pumped on end frame. */
		bool bUseWorkerThread;
		/** If important cache is enabled */
		bool bUseImportantCache;
		/** Size of tail buffer */
		uint32 TailSizeBytes;
		/** If stats are emitted as named events */
		bool bStatNamedEvents;
	};

	struct FChannel
	{
		FString Name;
		FString Description;
		uint32 Id;
		bool bEnabled;
		bool bReadOnly;
	};

	struct FStats
	{
		/** Number of bytes sent to server or file. */
		uint64 BytesSent;
		/** Number of (uncompressed) bytes traced from process. */
		uint64 BytesTraced;
		/** Total memory used by TraceLog */
		uint64 MemoryUsed;
		/** Allocated memory for important events */
		uint32 CacheAllocated;
		/** Memory used for important events */
		uint32 CacheUsed;
		/** Wasted space for important events cache */
		uint32 CacheWaste;
	};
	
	/** Session id of the process we're controlling */
	FGuid SessionId;
	/** Instance id of the process we're controlling */
	FGuid InstanceId;
	/** If tracing is active */
	bool bIsTracing = false;
	/** Endpoint of active trace */
	FString Endpoint;
	/** Session identifier for the trace. */
	FGuid SessionGuid;
	/** Unique identifier for the trace */
	FGuid TraceGuid;
	/** Settings */
	FSettings Settings;
	/** State of channels */
	TMap<uint32, FChannel> Channels;
	/** Stats */
	FStats Stats;
};

ENUM_CLASS_FLAGS(FTraceStatus::EUpdateType);

/**
 * Interface to control other sessions tracing.
 */
class ITraceController
{
public:
	
	virtual ~ITraceController() = default;

	/**
	 * Enables or disables channels by name
	 * @param ChannelsToEnable  List of channels to enable
	 * @param ChannelsToDisable List of channels to disable
	 */
	virtual void SetChannels(TConstArrayView<FStringView> ChannelsToEnable, TConstArrayView<FStringView> ChannelsToDisable) = 0;

	/**
	 * Enables or disables channels by name
	 * @param ChannelsToEnable  List of channels to enable
	 * @param ChannelsToDisable List of channels to disable
	 */
	virtual void SetChannels(TConstArrayView<FString> ChannelsToEnable, TConstArrayView<FString> ChannelsToDisable) = 0;
	
	/**
	 * Start a trace on selected instances to the provided host, using a set of channels.
	 * @param Host Host to send the trace to
	 * @param Channels Comma separated list of channels to enable
	 * @param bExcludeTail If the tail (circular buffer of recent events) should be included
	 */
	virtual void Send(FStringView Host, FStringView Channels, bool bExcludeTail = false) = 0;

	/**
	 * Start a trace on selected instances to a file on the instance, using a set of channels.
	 * @param File Path on the instance. ".utrace" will be appended
	 * @param Channels Comma separated list of channels to enable
	 * @param bExcludeTail If the tail (circular buffer of recent events) should be included
	 * @param bTruncateFile If the file should be truncated (if already exists)
	 */
	virtual void File(FStringView File, FStringView Channels, bool bExcludeTail = false, bool bTruncateFile = false) = 0;

	/**
	 * On selected instances, make a snapshot of the tail (circular buffer of recent events)
	 * and send to the provided host.
	 * @param Host Host to send the trace to
	 */
	virtual void SnapshotSend(FStringView Host) = 0;
	
	/**
	 * On selected instances, make a snapshot of the tail (circular buffer of recent events)
	 * and save to a file.
	 * @param File Path on the instance. ".utrace" will be appended
	 */
	virtual void SnapshotFile(FStringView File) = 0;

	/**
	 * Pause tracing by muting all (non-readonly) channels.
	 */
	virtual void Pause() = 0;

	/**
	 * Resume tracing (from paused) by enabling the previously enabled channels.
	 */
	virtual void Resume() = 0;

	/**
	 * Stop active trace.
	 */
	virtual void Stop() = 0;

	/**
	 * Insert bookmark into the trace.
	 * @param Label Label of bookmark
	 */
	virtual void Bookmark(FStringView Label) = 0;

	/**
	 * Insert screenshot into the trace.
	 * @param Name Name of the screenshot
	 * @param bShowUI If the UI should be visible in the image
	 */
	virtual void Screenshot(FStringView Name, bool bShowUI) = 0;

	/**
	 * Request update of the status from all sessions and instances.
	 */
	virtual void SendStatusUpdateRequest() = 0;

	/**
	 * Request update of channel status from all sessions and instances.
	 */
	virtual void SendChannelUpdateRequest() = 0;

	/**
	 * Request update of setting state from all sessions and instances.
	 */
	virtual void SendSettingsUpdateRequest() = 0;

	/**
	 * Generic event for updates of status
	 */
	DECLARE_EVENT_TwoParams(ITraceController, FStatusRecievedEvent, const FTraceStatus&, FTraceStatus::EUpdateType);

	/**
	 * Event triggered whenever the FTraceStatus for the selected instance (in session manager)
	 * is updated. A reference to the status and what has changed is provided.
	 */
	virtual FStatusRecievedEvent& OnStatusReceived() = 0;
	
};
