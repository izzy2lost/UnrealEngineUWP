// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkOpenVRSource.h"
#include "HAL/RunnableThread.h"
#include "ILiveLinkClient.h"
#include "LiveLinkOpenVRModule.h"
#include "LiveLinkSubjectSettings.h"
#include "Logging/StructuredLog.h"
#include "Misc/CoreDelegates.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

#include <openvr.h>


#define LOCTEXT_NAMESPACE "LiveLinkOpenVR"


namespace
{
	FMatrix ToFMatrix(const vr::HmdMatrix34_t& tm)
	{
		// Rows and columns are swapped between vr::HmdMatrix34_t and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], 0.0f),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], 0.0f),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], 0.0f),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], 1.0f)
		);
	}

	FMatrix ToFMatrix(const vr::HmdMatrix44_t& tm)
	{
		// Rows and columns are swapped between vr::HmdMatrix44_t and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], tm.m[3][0]),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], tm.m[3][1]),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], tm.m[3][2]),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], tm.m[3][3])
		);
	}
} // namespace


FLiveLinkOpenVRSource::FLiveLinkOpenVRSource(const FLiveLinkOpenVRConnectionSettings& InConnectionSettings)
	: ConnectionSettings(InConnectionSettings)
	, SubjectNames(NAME_None)
{
	SourceStatus = LOCTEXT("SourceStatus_NoData", "No data");
	SourceType = LOCTEXT("SourceType_OpenVR", "OpenVR");
	SourceMachineName = LOCTEXT("Source_MachineName", "Local OpenVR");

	FLiveLinkOpenVRModule& Module = FLiveLinkOpenVRModule::Get();
	vr::IVRSystem* VrSystem = Module.GetVrSystem();
	if (!VrSystem)
	{
		UE_LOG(LogLiveLinkOpenVR, Error, TEXT("LiveLinkOpenVRSource: Couldn't get IVRSystem"));
		return;
	}

	DeferredStartDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveLinkOpenVRSource::Start);
}


FLiveLinkOpenVRSource::~FLiveLinkOpenVRSource()
{
	// This could happen if the object is destroyed before FCoreDelegates::OnEndFrame calls FLiveLinkOpenVRSource::Start
	if (DeferredStartDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	}

	if (Client)
	{
		Client->OnLiveLinkSubjectAdded().Remove(OnSubjectAddedDelegate);
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}


void FLiveLinkOpenVRSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	OnSubjectAddedDelegate = Client->OnLiveLinkSubjectAdded().AddRaw(this, &FLiveLinkOpenVRSource::OnLiveLinkSubjectAdded);
}


void FLiveLinkOpenVRSource::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
	ULiveLinkOpenVRSourceSettings* SourceSettings = Cast<ULiveLinkOpenVRSourceSettings>(Settings);
	if (!ensure(SourceSettings))
	{
		return;
	}

	LocalUpdateRateInHz = SourceSettings->CommonSettings.LocalUpdateRateInHz;
}


bool FLiveLinkOpenVRSource::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread
	const bool bIsSourceValid = !bStopping && (Thread != nullptr);
	return bIsSourceValid;
}


bool FLiveLinkOpenVRSource::RequestSourceShutdown()
{
	Stop();

	return true;
}


TSubclassOf<ULiveLinkSourceSettings> FLiveLinkOpenVRSource::GetSettingsClass() const
{
	return ULiveLinkOpenVRSourceSettings::StaticClass();
}


void FLiveLinkOpenVRSource::Start()
{
	check(DeferredStartDelegateHandle.IsValid());

	FCoreDelegates::OnEndFrame.Remove(DeferredStartDelegateHandle);
	DeferredStartDelegateHandle.Reset();
	
	SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");

	static std::atomic<int32> ReceiverIndex = 0;
	ThreadName = "LiveLinkOpenVR Receiver ";
	ThreadName.AppendInt(++ReceiverIndex);

	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}


void FLiveLinkOpenVRSource::Stop()
{
	bStopping = true;
}


uint32 FLiveLinkOpenVRSource::Run()
{
	FLiveLinkOpenVRModule& Module = FLiveLinkOpenVRModule::Get();

	TStaticArray<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> Poses;
	TStringBuilder<256> StringBuilder;

	TMap<FName, FTransform> SubjectPoses;
	double LastFrameTimeSec = -DBL_MAX;
	while (!bStopping)
	{
		// Send new poses at the user specified update rate
		const double FrameIntervalSec = 1.0 / LocalUpdateRateInHz;
		const double TimeNowSec = FPlatformTime::Seconds();
		if (TimeNowSec >= (LastFrameTimeSec + FrameIntervalSec))
		{
			LastFrameTimeSec = TimeNowSec;
			SubjectPoses.Reset();

			vr::IVRSystem* VrSystem = Module.GetVrSystem();
			VrSystem->GetDeviceToAbsoluteTrackingPose(
				vr::ETrackingUniverseOrigin::TrackingUniverseStanding,
				0.0f,
				Poses.GetData(),
				Poses.Num()
			);

			for (int32 DeviceIdx = 0; DeviceIdx < Poses.Num(); ++DeviceIdx)
			{
				const vr::TrackedDevicePose_t& Pose = Poses[DeviceIdx];
				if (!Pose.bDeviceIsConnected)
				{
					continue;
				}

				FName SubjectName = SubjectNames[DeviceIdx];

				// If we don't have a name, it's a new subject.
				if (SubjectName == NAME_None)
				{
					StringBuilder.Reset();

					const vr::ETrackedDeviceClass DeviceClass = VrSystem->GetTrackedDeviceClass(DeviceIdx);
					switch (DeviceClass)
					{
						case vr::TrackedDeviceClass_HMD:               StringBuilder << TEXT("HMD"); break;
						case vr::TrackedDeviceClass_Controller:        StringBuilder << TEXT("Controller"); break;
						case vr::TrackedDeviceClass_GenericTracker:    StringBuilder << TEXT("Tracker"); break;
						case vr::TrackedDeviceClass_TrackingReference: StringBuilder << TEXT("TrackingRef"); break;
						default:                                       StringBuilder << TEXT("Other"); break;
					}

					StringBuilder << TEXT("_");

					char SerialNumBuf[128] = { 0 };
					VrSystem->GetStringTrackedDeviceProperty(DeviceIdx, vr::Prop_SerialNumber_String, SerialNumBuf, sizeof(SerialNumBuf));
					StringBuilder << SerialNumBuf;

					SubjectName = SubjectNames[DeviceIdx] = FName(StringBuilder.ToString());

					// If the LiveLink client already knows about this subject, then it must have been added via a preset
					// Only new subjects should be set to rebroadcast by default. Presets should respect the existing settings
					if (!Client->GetSubjects(true, true).Contains(FLiveLinkSubjectKey(SourceGuid, SubjectName)))
					{
						SubjectsToRebroadcast.Add(SubjectName);
					}

					FLiveLinkStaticDataStruct StaticData(FLiveLinkTransformStaticData::StaticStruct());
					Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticData));
				}

				// We might have static data, but not frame data.
				if (!Pose.bPoseIsValid)
				{
					continue;
				}

				// Transpose and decompose.
				const FMatrix PoseMatrix = ToFMatrix(Pose.mDeviceToAbsoluteTracking);
				const FQuat PoseOrientation(PoseMatrix);
				const FVector PosePosition(PoseMatrix.M[3][0], PoseMatrix.M[3][1], PoseMatrix.M[3][2]);

				// Handedness/basis change/scale.
				const FTransform PoseTransform(
					FQuat(-PoseOrientation.Z, PoseOrientation.X, PoseOrientation.Y, -PoseOrientation.W),
					FVector(-PosePosition.Z, PosePosition.X, PosePosition.Y) * 100.0f
				);

				FLiveLinkFrameDataStruct FrameData(FLiveLinkTransformFrameData::StaticStruct());
				FLiveLinkTransformFrameData* TransformFrameData = FrameData.Cast<FLiveLinkTransformFrameData>();
				TransformFrameData->Transform = PoseTransform;

				Send(&FrameData, SubjectName);
			}
		}

		FPlatformProcess::Sleep(0.001f);
	}
	
	return 0;
}


void FLiveLinkOpenVRSource::Send(FLiveLinkFrameDataStruct* FrameDataToSend, FName SubjectName)
{
	if (bStopping || (Client == nullptr))
	{
		return;
	}

	Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(*FrameDataToSend));
}


void FLiveLinkOpenVRSource::OnLiveLinkSubjectAdded(FLiveLinkSubjectKey InSubjectKey)
{
	// Set rebroadcast to true for any new subjects
	if (SubjectsToRebroadcast.Contains(InSubjectKey.SubjectName))
	{
		ULiveLinkSubjectSettings* SubjectSettings = Cast<ULiveLinkSubjectSettings>(Client->GetSubjectSettings(InSubjectKey));
		if (SubjectSettings)
		{
			SubjectSettings->bRebroadcastSubject = true;
		}
	}
}


#undef LOCTEXT_NAMESPACE
