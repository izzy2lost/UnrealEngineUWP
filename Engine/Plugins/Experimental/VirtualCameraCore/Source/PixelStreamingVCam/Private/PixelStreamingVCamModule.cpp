// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVCamModule.h"
#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "IDecoupledOutputProviderModule.h"
#include "VCamPixelStreamingSessionLogic.h"
#include "Modules/ModuleManager.h"

namespace UE::PixelStreamingVCam::Private
{
	void FPixelStreamingVCamModule::StartupModule()
	{
		ConfigurePixelStreaming();

		BeaconReceiver.SetIsStreamingReady(false);
		BeaconReceiver.Startup();
	}

	void FPixelStreamingVCamModule::ShutdownModule()
	{
		// DecoupledOutputProvider will also be destroyed in a moment (part of same plugin) so we do not have to call IDecoupledOutputProviderModule::UnregisterLogicFactory.
		// In fact doing so would not even work because UVCamPixelStreamingSession::StaticClass() would return garbage.

		BeaconReceiver.Shutdown();
	}

	void FPixelStreamingVCamModule::AddActiveSession(const TWeakObjectPtr<UVCamPixelStreamingSession>& Session)
	{
		ActiveSessions.Add(Session);
		UpdateBeaconReceiverStreamReadiness();
	}

	void FPixelStreamingVCamModule::RemoveActiveSession(const TWeakObjectPtr<UVCamPixelStreamingSession>& Session)
	{
		ActiveSessions.Remove(Session);
		UpdateBeaconReceiverStreamReadiness();
	}

	FPixelStreamingVCamModule& FPixelStreamingVCamModule::Get()
	{
		return FModuleManager::Get().GetModuleChecked<FPixelStreamingVCamModule>("PixelStreamingVcam");
	}

	void FPixelStreamingVCamModule::ConfigurePixelStreaming()
	{
		using namespace DecoupledOutputProvider;
		IDecoupledOutputProviderModule& DecouplingModule = IDecoupledOutputProviderModule::Get();
		DecouplingModule.RegisterLogicFactory(
			UVCamPixelStreamingSession::StaticClass(),
			FOutputProviderLogicFactoryDelegate::CreateLambda([](const FOutputProviderLogicCreationArgs& Args)
				{
					return MakeShared<FVCamPixelStreamingSessionLogic>();
				})
		);

		// Set the some VCam specific CVars in Pixel Streaming

		// (Luke.Bermingham): If this set of configurations shows promising results
		// then consider how we want to expose these to the user? Or which ones we should expose?

		// Enabling the legacy audio device seems to result in better AV sync.
		if (IConsoleVariable* UseLegacyAudioDevice = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.UseLegacyAudioDevice")))
		{
			UseLegacyAudioDevice->Set(true);
		}

		// Couple engine's render rate and streaming rate, results in better video sync between UE and LiveLink VCam app
		if (IConsoleVariable* DecoupleFramerateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.DecoupleFramerate")))
		{
			DecoupleFramerateCVar->Set(false);
		}

		// Set the rate at which we will stream
		if (IConsoleVariable* StreamFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.Fps")))
		{
			StreamFPS->Set(60);
		}

		// Not relevant for VCam, but explicitly set it in case this codepath changes in the future.
		if (IConsoleVariable* CaptureUseFence = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.CaptureUseFence")))
		{
			CaptureUseFence->Set(false);
		}

		// Disable keyframes interval
		if (IConsoleVariable* KeyframeInterval = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.KeyframeInterval")))
		{
			KeyframeInterval->Set(0);
		}

		// Set a fixed target bitrate (this way quality and network transmission should be bounded).
		// We want this network transmission to be as consistent as possible so that the jitter buffer on the recv side
		// stays well bounded and shrinks to its optimal value and stays there.
		if (IConsoleVariable* TargetBitrate = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.TargetBitrate")))
		{
			TargetBitrate->Set(10000000);
		}

		// Enable filler data in the encoding (while wasteful this ensure stable bitrate which helps with latency estimations)
		// In short this a tradeoff between increased bitrate for better latency.
		if (IConsoleVariable* EnableFillerData = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.EnableFillerData")))
		{
			EnableFillerData->Set(true);
		}

		// Disable the frame dropper so we can stream as very consistent FPS.
		// This is required for when we are setting a target bitrate (ignoring WebRTC) and feeding in a fixed FPS.
		// Without this we get frames dropped due overshooting bitrate targets (which is fine on LAN).
		if (IConsoleVariable* DisableFrameDropper = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.DisableFrameDropper")))
		{
			DisableFrameDropper->Set(true);
		}

		// When bitrate is set high AND frame dropper is disabled this means the pacer will end up with a lot of packets in it.
		// We need to set a lenient value in the pacer so it too doesn't start dropping frames in fear of congestion control.
		// Again, this is fine to do on LAN where we know the link won't become congested at the bitrates and FPS we are targeting.
		if (IConsoleVariable* VideoPacingFactor = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.VideoPacing.Factor")))
		{
			VideoPacingFactor->Set(100.0f);
		}
	}

	void FPixelStreamingVCamModule::UpdateBeaconReceiverStreamReadiness()
	{
		BeaconReceiver.SetIsStreamingReady(!ActiveSessions.IsEmpty());
	}
}

IMPLEMENT_MODULE(UE::PixelStreamingVCam::Private::FPixelStreamingVCamModule, PixelStreamingVCam);
