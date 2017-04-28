// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Task to do title-measured QoS and upload it as part of matchmaking

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "OnlineAsyncTaskLiveMeasureAndUploadQos.h"
#include "OnlineAsyncTaskLiveGameSessionReady.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;
using namespace Platform::Collections;

FOnlineAsyncTaskLiveMeasureAndUploadQos::FOnlineAsyncTaskLiveMeasureAndUploadQos(
	FOnlineSessionLive* InLiveInterface,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FNamedOnlineSession* InNamedSession,
	class FOnlineSubsystemLive* Subsystem,
	int RetryCount,
	int32 InQosTimeoutMs,
	int32 InQosProbeCount)
	: FOnlineAsyncTaskLive(Subsystem, 0)
	, SessionInterface(InLiveInterface)
	, NamedSession(InNamedSession)
	, LiveContext(InContext)
	, LiveSession(nullptr)
	, LiveInfo(nullptr)
	, InitialRetryCount(RetryCount)
	, AddressDeviceTokenMap()
	, MeasurementResults(nullptr)
	, QosTimeoutMs(InQosTimeoutMs)
	, QosProbeCount(InQosProbeCount)
{
	LocalUsersInSession = ref new Vector<Windows::Xbox::System::User^>();
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::OnFailed()
{
	bWasSuccessful = false;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::Initialize()
{
	LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	check(LiveInfo.IsValid());
	
	// @v2live: Should we get the latest session from Live in case someone joined after we scheduled the
	// task? I'm not sure that can happen during the first run of initialization, but maybe it can if
	// users are joining a game in progress.
	LiveSession = LiveInfo->GetLiveMultiplayerSession();
	check(LiveSession);
	
	TSet<FString> UniqueAddresses;

	// Identify local and remote members, get addresses for remote ones
	for (auto Member : LiveSession->Members )
	{
		FUniqueNetIdLive MemberId(Member->XboxUserId);
		auto MemberUser = Subsystem->GetIdentityLive()->GetUserForUniqueNetId(MemberId);
		
		if (MemberUser != nullptr)
		{
			LocalUsersInSession->Append(MemberUser);
		}
		else
		{
			Platform::String^ Address = Member->SecureDeviceAddressBase64;
			if (!Address->IsEmpty())
			{
				UniqueAddresses.Add(Address->Data());
				AddressDeviceTokenMap.Add(Address->Data(), Member->DeviceToken->Data());
			}
		}
	}

	if(LocalUsersInSession->Size <= 0)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionLive::MeasureQualityOfService - didn't find any local session members for upload"));
		OnFailed();
		return;
	}

	auto Addresses = ref new Vector<SecureDeviceAddress^>();
	for (FString& AddressString : UniqueAddresses)
	{
		Addresses->Append(SecureDeviceAddress::FromBase64String(ref new Platform::String(*AddressString)));
	}

	auto Metrics = ref new Vector<Windows::Xbox::Networking::QualityOfServiceMetric>();
	Metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthDown);
	Metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthUp);
	Metrics->Append(Windows::Xbox::Networking::QualityOfServiceMetric::LatencyAverage);

	if (Addresses->Size > 0)
	{
		concurrency::create_task(
			Windows::Xbox::Networking::QualityOfService::MeasureQualityOfServiceAsync(
				Addresses, 
				Metrics, 
				QosTimeoutMs,
				QosProbeCount))
		.then([this](task<MeasureQualityOfServiceResult^> Task)
		{
			try
			{
				// Extract all the measurements and put them in the right format for upload
				auto Result = Task.get();

				UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::MeasureQualityOfService - got %d QoS measurements"), Result->Measurements->Size);

				if (Result->Measurements->Size > 0)
				{
					TMap<FString, TMap<int32, uint32>> ResultMap;

					for (auto Measurement : Result->Measurements)
					{
						UE_LOG_ONLINE(Log, TEXT("  Measurement result for address: %s"), Measurement->SecureDeviceAddress->GetBase64String()->Data());
						Platform::String^ Address = Measurement->SecureDeviceAddress->GetBase64String();
						FString DeviceToken = AddressDeviceTokenMap[Address->Data()];

						auto Status = Measurement->Status;
						UE_LOG_ONLINE(Log, TEXT("    Result status: %s"), Status.ToString()->Data());
						if (Status == Windows::Xbox::Networking::QualityOfServiceMeasurementStatus::PartialResults || Status == Windows::Xbox::Networking::QualityOfServiceMeasurementStatus::Success)
						{
							UE_LOG_ONLINE(Log,
								TEXT("    Metric: %s / Value: %s"),
								Measurement->Metric.ToString()->Data(),
								Measurement->MetricValue->ToString()->Data());

							auto& PeerMeasurement = ResultMap.FindOrAdd(DeviceToken);
							PeerMeasurement.Add((int32)(Measurement->Metric), Measurement->MetricValue->GetUInt32());
						}
					}

					MeasurementResults = ref new Vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>();
					for (auto MapElement : ResultMap)
					{
						FString& DeviceToken = MapElement.Key;
						if (DeviceToken != TEXT(""))
						{
							auto PeerMeasurement = MapElement.Value;

							TimeSpan Latency;
							Latency.Duration = PeerMeasurement[(int32)Windows::Xbox::Networking::QualityOfServiceMetric::LatencyAverage] * TICKS_PER_MILLISECOND;

							MultiplayerQualityOfServiceMeasurements^ QosMeasurement 
								= ref new MultiplayerQualityOfServiceMeasurements(
									ref new Platform::String(*DeviceToken),
									Latency,
									PeerMeasurement[(int32)Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthDown],
									PeerMeasurement[(int32)Windows::Xbox::Networking::QualityOfServiceMetric::BandwidthUp],
									L"{}"
								);

							MeasurementResults->Append(QosMeasurement);
						}
					}

					RetryUpload(LocalUsersInSession, nullptr, InitialRetryCount);
				}
				else
				{
					UE_LOG_ONLINE(Error, TEXT("FOnlineSessionLive::MeasureQualityOfService - failed to get QoS measurements"));
					OnFailed();
				}
			}
			catch(Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineSessionLive::MeasureQualityOfService - measuring QoS failed with 0x%0.8X"), Ex->HResult);
				OnFailed();
			}
		});
	}
	else
	{
		// QoS request with no other members is valid
		UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::MeasureQualityOfService - didn't find any other session members, will upload empty results"));
		
		MeasurementResults = ref new Vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements^>();
		RetryUpload(LocalUsersInSession, nullptr, InitialRetryCount);
	}
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::RetryUpload(
	Platform::Collections::Vector<Windows::Xbox::System::User^>^ LocalUsers,
	MultiplayerSession^ UserSession,
	int32 RemainingRetries)
{
	if ( LocalUsers->Size == 0)
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::RetryUpload - QoS results uploaded for all users"));

		bWasSuccessful = true;
		bIsComplete = true;
		return;
	}

	if ( RemainingRetries <= 0 )
	{
		OnFailed();
		return;
	}
	--RemainingRetries;
	
	if (UserSession == nullptr)
	{
		auto CurrentUser = LocalUsers->GetAt(LocalUsers->Size - 1);
		auto UserContext = Subsystem->GetLiveContext(CurrentUser);
		auto GetSessionOp = UserContext->MultiplayerService->GetCurrentSessionAsync(LiveInfo->GetLiveMultiplayerSessionRef());
		create_task(GetSessionOp).then([this, CurrentUser, LocalUsers, RemainingRetries](task<MultiplayerSession^> SessionTask)
		{
			try
			{
				MultiplayerSession^ LatestSession = SessionTask.get();
			
				TryWriteSession(LocalUsers, LatestSession, RemainingRetries);
			}
			catch(Platform::Exception^ Ex)
			{
				if (Ex->HResult == HTTP_E_STATUS_NOT_FOUND)
				{
					RetryUpload(LocalUsers, nullptr, RemainingRetries);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionLive::RetryUpload - Failed to get session with 0x%0.8X"), Ex->HResult);
					OnFailed();
				}
			}
		});
	}
	else
	{
		TryWriteSession(LocalUsers, UserSession, RemainingRetries);
	}
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::TryWriteSession(
	Platform::Collections::Vector<Windows::Xbox::System::User^>^ LocalUsers,
	MultiplayerSession^ UserSession,
	int32 RemainingRetries)
{
	// Session may be null if it timed out after the SessionReference was stored
	if (UserSession == nullptr)
	{
		OnFailed();
		return;
	}

	UserSession->SetCurrentUserQualityOfServiceMeasurements(MeasurementResults->GetView());

	auto CurrentUser = LocalUsers->GetAt(LocalUsers->Size - 1);
	auto UserContext = Subsystem->GetLiveContext(CurrentUser);
	auto WriteOp = UserContext->MultiplayerService->TryWriteSessionAsync(UserSession, MultiplayerSessionWriteMode::SynchronizedUpdate);
	create_task(WriteOp).then([this, CurrentUser, LocalUsers, RemainingRetries](task<WriteSessionResult^> Task)
	{
		try
		{
			WriteSessionResult^ Result = Task.get();
			MultiplayerSession^ LatestSession = Result->Session;

			if (Result->Succeeded)
			{
				UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::RetryUpload - QoS results uploaded for user"));
					
				if (LatestSession->CurrentUser->XboxUserId == LiveSession->CurrentUser->XboxUserId)
				{
					// We're in the context of the user that started the task. They need to keep track of the
					// updated session.
					LiveSession = LatestSession;
				}

				// Upload for remaining users recursively
				LocalUsers->RemoveAtEnd();
				RetryUpload(LocalUsers, nullptr, InitialRetryCount);
			}
			else if (Result->Status == WriteSessionStatus::OutOfSync)
			{
				RetryUpload(LocalUsers, LatestSession, RemainingRetries);
			}
			else
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineSessionLive::RetryUpload - failed to write session: %s"), Result->Session->MultiplayerCorrelationId->Data());
				OnFailed();
			}
		}
		catch(Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Error, TEXT("FOnlineSessionLive::RetryUpload - failed to write session with 0x%0.8X"), Ex->HResult);
			OnFailed();
		}
	});
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::Finalize()
{
	if (bWasSuccessful/* && LiveInfo.IsValid()*/)
	{
		// Update with the new session since we wrote to it
		Subsystem->RefreshLiveInfo(NamedSession->SessionName, LiveSession);
	}
}

void FOnlineAsyncTaskLiveMeasureAndUploadQos::TriggerDelegates()
{
	if(!bWasSuccessful)
	{
		FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = Subsystem->GetMatchmakingInterfaceLive();
		MatchmakingInterface->SetTicketState(NamedSession->SessionName, EOnlineLiveMatchmakingState::None);
		MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, false);
	}
	// On success, don't trigger anything--the change handler in OnlineSessionInterfaceLive will see
	// when QoS finishes for the session and process the results.
}
