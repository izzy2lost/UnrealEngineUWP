// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "Networking/SignalingServerLifecycle.h"
#include "Notifications/MissingSignallingServerNotifier.h"

#include "VCamPixelStreamingSubsystem.generated.h"

class FPixelStreamingLiveLinkSource;
class UVCamPixelStreamingSession;

/**
 * Keeps track of which UVCamPixelStreamingSessions are active and manages systems related to the list of active sessions.
 */
UCLASS()
class PIXELSTREAMINGVCAM_API UVCamPixelStreamingSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	
	/** Convenience function for accessing the subsystem */
	static UVCamPixelStreamingSubsystem* Get();
	
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface
	
	void RegisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider);
	void UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider);
	
	/** Get the LiveLinkSource if it already exists or attempt to create one.*/
	TSharedPtr<FPixelStreamingLiveLinkSource> TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider);

	void LaunchSignallingServerIfNeeded(UVCamPixelStreamingSession& Session);
	void StopSignallingServerIfNeeded(UVCamPixelStreamingSession& Session);

	const TArray<TWeakObjectPtr<UVCamPixelStreamingSession>>& GetRegisteredSessions() const { return RegisteredSessions; }
	
private:
	
	/** An associated Live Link Source shared by all output providers. */
	TSharedPtr<FPixelStreamingLiveLinkSource> LiveLinkSource;

	/** The active sessions. */
	TArray<TWeakObjectPtr<UVCamPixelStreamingSession>> RegisteredSessions;

	/** Tells the user when the server needs manual launching. */
	TUniquePtr<UE::PixelStreamingVCam::FMissingSignallingServerNotifier> MissingSignallingServerNotifier;
	/** Manages the lifecycle of the signalling server. */
	TUniquePtr<UE::PixelStreamingVCam::FSignalingServerLifecycle> SignalingServerLifecycle;
};
