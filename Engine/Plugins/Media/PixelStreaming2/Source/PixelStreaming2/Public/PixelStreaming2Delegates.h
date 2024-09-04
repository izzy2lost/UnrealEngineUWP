// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Containers/UnrealString.h"
#include "PixelStreaming2Delegates.generated.h"

UCLASS()
class PIXELSTREAMING2_API UPixelStreaming2Delegates : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * A new connection has been made to the session.
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNewConnection, FString, StreamerId, FString, PlayerId);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FNewConnection OnNewConnection;
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FNewConnectionNative, FString /* Streamer id*/, FString /* Peer id */);
	FNewConnectionNative OnNewConnectionNative;

	/**
	 * A connection to a player was lost.
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FClosedConnection, FString, StreamerId, FString, PlayerId);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FClosedConnection OnClosedConnection;
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FClosedConnectionNative, FString /* Streamer id */, FString /* Peer id */);
	FClosedConnectionNative OnClosedConnectionNative;

	/**
	 * All connections have closed and nobody is viewing or interacting with
	 * the app. This is an opportunity to reset the app.
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAllConnectionsClosed, FString, StreamerId);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FAllConnectionsClosed OnAllConnectionsClosed;
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_OneParam(FAllConnectionsClosedNative, FString);
	FAllConnectionsClosedNative OnAllConnectionsClosedNative;

	/**
	 * A new data track has been opened
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDataTrackOpen, FString, StreamerId, FString, PlayerId);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FDataTrackOpen OnDataTrackOpen;
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FDataTrackOpenNative, FString /* Streamer id */, FString /* Peer id */);
	FDataTrackOpenNative OnDataTrackOpenNative;

	/**
	 * An existing data track has been closed
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDataTrackClosed, FString, StreamerId, FString, PlayerId);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FDataTrackClosed OnDataTrackClosed;
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FDataTrackClosedNative, FString /* Streamer id */, FString /* Peer id */);
	FDataTrackClosedNative OnDataTrackClosedNative;

	/**
	 * A new video track has been opened
	 */
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FVideoTrackOpenNative, FString /* Streamer id */, FString /* Peer id */, bool /* bIsRemote */);
	FVideoTrackOpenNative OnVideoTrackOpenNative;

	/**
	 * An existing video track has been closed
	 */
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FVideoTrackClosedNative, FString /* Streamer id */, FString /* Peer id */, bool /* bIsRemote */);
	FVideoTrackClosedNative OnVideoTrackClosedNative;

	/**
	 * A new audio track has been opened
	 */
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FAudioTrackOpenNative, FString /* Streamer id */, FString /* Peer id */, bool /* bIsRemote */);
	FAudioTrackOpenNative OnAudioTrackOpenNative;

	/**
	 * An existing audio track has been closed
	 */
	// C++ Delegate
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FAudioTrackClosedNative, FString /* Streamer id */, FString /* Peer id */, bool /* bIsRemote */);
	FAudioTrackClosedNative OnAudioTrackClosedNative;

	/**
	 * A pixel streaming stat has changed
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStatsChanged, FString, PlayerId, FName, StatName, float, StatValue);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FStatsChanged OnStatChanged;
	// C++ Delegate
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FStatChangedNative, FString /* Peer id */, FName /* Stat name */, float);
	FStatChangedNative OnStatChangedNative;

	/**
	 * The GPU ran out of available HW encoders and fell back to software encoders
	 */
	// BP Delegate
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFallbackToSoftwareEncoding);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Delegates")
	FFallbackToSoftwareEncoding OnFallbackToSoftwareEncoding;
	// C++ Delegate
	DECLARE_MULTICAST_DELEGATE(FFallbackToSoftwareEncodingNative);
	FFallbackToSoftwareEncodingNative OnFallbackToSoftwareEncodingNative;

	/**
	 * Create the singleton.
	 */
	static UPixelStreaming2Delegates* Get()
	{
		if (Singleton == nullptr && !IsEngineExitRequested())
		{
			Singleton = NewObject<UPixelStreaming2Delegates>();
			Singleton->AddToRoot();
		}
		return Singleton;
	}

	virtual ~UPixelStreaming2Delegates()
	{
		Singleton = nullptr;
	}

private:
	// The singleton object.
	static inline UPixelStreaming2Delegates* Singleton = nullptr;
};
