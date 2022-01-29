// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include <collection.h>

class FOnlineSubsystemLive;

/** 
 * Async task used to measure QoS to peers during session initialization, then upload the results to
 * the session document.
 */
class FOnlineAsyncTaskLiveMeasureAndUploadQos : public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveMeasureAndUploadQos(
		class FOnlineSessionLive* InLiveInterface,
		Microsoft::Xbox::Services::XboxLiveContext^ InContext,
		class FNamedOnlineSession* InNamedSession,
		class FOnlineSubsystemLive* Subsystem,
		int32 RetryCount,
		int32 InQosTimeoutMs,
		int32 InQosProbeCount);

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("MeasureAndUploadQosAsync");}
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	void OnFailed();
	void UploadMeasurements();
	void RetryUpload(
		Platform::Collections::Vector<Windows::Xbox::System::User^>^ LocalUsers,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ UserSession,
		int32 RemainingRetries);
	void TryWriteSession(
		Platform::Collections::Vector<Windows::Xbox::System::User^>^ LocalUsers,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ UserSession,
		int32 RemainingRetries);

	class FOnlineSessionLive* SessionInterface;
	class FNamedOnlineSession* NamedSession;
	class FOnlineSubsystemLive* LiveSubsystem;

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;

	// Saved values used across functions
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;
	FOnlineSessionInfoLivePtr LiveInfo;
	TMap<FString, FString> AddressDeviceTokenMap;
	Platform::Collections::Vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>^ MeasurementResults;
	Platform::Collections::Vector<Windows::Xbox::System::User^>^ LocalUsersInSession;

	int32 InitialRetryCount;
	int32 QosTimeoutMs;
	int32 QosProbeCount;

	static const int32 TICKS_PER_MILLISECOND = 10 * 1000;
};
