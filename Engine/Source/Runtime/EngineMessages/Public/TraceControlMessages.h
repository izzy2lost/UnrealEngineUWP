// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Containers/BitArray.h"
#include "TraceControlMessages.generated.h"

/* Trace messages
 *****************************************************************************/

/**
 * Request a status update
 */
USTRUCT()
struct FTraceControlStatusPing
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Response for FTraceControlStatusPing. Meant to be fairly high
 * frequency.
 */
USTRUCT()
struct FTraceControlStatus
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	FString Endpoint;
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionGuid;
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid TraceGuid;

	UPROPERTY(EditAnywhere, Category="Message")
	uint64 BytesSent;

	UPROPERTY(EditAnywhere, Category="Message")
	uint64 BytesTraced;

	UPROPERTY(EditAnywhere, Category="Message")
	uint64 MemoryUsed;

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 CacheAllocated; 

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 CacheUsed;

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 CacheWaste;
};

/**
 * Request update of settings
 */
USTRUCT()
struct FTraceControlSettingsPing
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Response for FTraceControlSettingsPing containing
 * TraceLog and system settings.
 */
USTRUCT()
struct FTraceControlSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	bool bUseWorkerThread;
	
	UPROPERTY(EditAnywhere, Category="Message")
	bool bUseImportantCache;

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 TailSizeBytes;
	
	UPROPERTY(EditAnywhere, Category="Message")
	bool bStatNamedEvents;
};

/**
 * Request update of channels and status
 */
USTRUCT()
struct FTraceControlChannelsPing
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 KnownChannelCount;
};

/**
 * Response for FTraceControlChannelsPing with information
 * about available channels.
 */
USTRUCT()
struct FTraceControlChannelsDesc
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FString> Channels;

	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint32> Ids;
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FString> Descriptions;
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint32> ReadOnlyIds;
};

/**
 * Response for FTraceControlChannelsPing with information about
 * enabled channels.
 */
USTRUCT()
struct FTraceControlChannelsStatus
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint32> EnabledIds;
};


/**
 * Command to set the state of channels
 */
USTRUCT()
struct FTraceControlChannelsSet
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint32> ChannelIdsToEnable;
	
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint32> ChannelIdsToDisable;
};

/**
 * Message sent when first connecting to a session. Expected to return
 * FTraceControlPong message.
 */
USTRUCT()
struct FTraceControlDiscoveryPing
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;
};

/**
 * Response for FTraceControlPing. A status message with additional
 * information about session instance and properties that doesn't change
 * over the course of the session lifetime.
 */
USTRUCT()
struct FTraceControlDiscovery : public FTraceControlStatus
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;
	
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid InstanceId;
};

/**
 * Stop current trace.
 */
USTRUCT()
struct FTraceControlStop
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Shared properties for starting a trace. Never use this message directly.
 */
USTRUCT()
struct FTraceControlStartCommon
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FString Channels;

	UPROPERTY(EditAnywhere, Category="Message")
	bool bExcludeTail;
};

/**
 * Start tracing to a host
 */
USTRUCT()
struct FTraceControlSend : public FTraceControlStartCommon
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	FString Host;
};

/**
 * Start tracing to a file (on the receiver)
 */
USTRUCT()
struct FTraceControlFile : public FTraceControlStartCommon
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FString File;
	
	UPROPERTY(EditAnywhere, Category="Message")
	bool bTruncateFile;
};

/**
 * Pause all tracing (i.e. mute all channels)
 */
USTRUCT()
struct FTraceControlPause
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Resume all tracing (from pause)
 */
USTRUCT()
struct FTraceControlResume
{
	GENERATED_USTRUCT_BODY()
};

/**
 * Start tracing to a host
 */
USTRUCT()
struct FTraceControlSnapshotSend
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FString Host;
};

/**
 * Start tracing to a host
 */
USTRUCT()
struct FTraceControlSnapshotFile
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FString File;
};

/**
 * Start tracing to a host
 */
USTRUCT()
struct FTraceControlBookmark
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	FString Label;
};

/**
 * Start tracing to a host
 */
USTRUCT()
struct FTraceControlScreenshot
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category="Message")
	FString Name;
	
	UPROPERTY(EditAnywhere, Category="Message")
	bool bShowUI;
};
