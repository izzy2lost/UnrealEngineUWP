// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaCapture.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelCaptureInputFrameRHI.h"
#include "Slate/SceneViewport.h"
#include "PixelStreamingVCamLog.h"

void UPixelStreamingMediaCapture::OnRHIResourceCaptured_RenderingThread(
	const FCaptureBaseData& InBaseData,
	TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
	FTextureRHIRef InTexture)
{
	TSharedPtr<FPixelStreamingVideoInputVCam> VideoInputPtr = VideoInput.Pin();
	if (VideoInputPtr)
	{
		VideoInputPtr->OnFrame(FPixelCaptureInputFrameRHI(InTexture));
	}
}

void UPixelStreamingMediaCapture::OnRHIResourceCaptured_AnyThread(
	const FCaptureBaseData & InBaseData,
	TSharedPtr<FMediaCaptureUserData,ESPMode::ThreadSafe> InUserData,
	FTextureRHIRef InTexture)
{
	TSharedPtr<FPixelStreamingVideoInputVCam> VideoInputPtr = VideoInput.Pin();
	if (VideoInputPtr)
	{
		VideoInputPtr->OnFrame(FPixelCaptureInputFrameRHI(InTexture));
	}
}

bool UPixelStreamingMediaCapture::InitializeCapture()
{
	UE_LOG(LogPixelStreamingVCam, Log, TEXT("Initializing media capture for Pixel Streaming VCam."));
	bViewportResized = false;
	SetState(EMediaCaptureState::Capturing);

	// The following CVars condontionally force the MediaCapture capture/readback to be completed on the render thread (or any thread).
	static bool bForceRenderThread = false;
	static char ForceRenderThreadBit = bForceRenderThread ? 0 : 1;

	// Whether to wait for resource readback in a separate thread. (Experimental)
	IConsoleVariable* CVarScheduleAnyThread = IConsoleManager::Get().FindConsoleVariable(TEXT("MediaIO.ScheduleOnAnyThread"));
	if (CVarScheduleAnyThread)
	{
		CVarScheduleAnyThread->Set(ForceRenderThreadBit, EConsoleVariableFlags::ECVF_SetByCode);
	}

	// Whether to send out frame  in a separate thread. (Experimental)
	IConsoleVariable* CVarExperimentalScheduling = IConsoleManager::Get().FindConsoleVariable(TEXT("MediaIO.EnableExperimentalScheduling"));
	if (CVarExperimentalScheduling)
	{
		CVarExperimentalScheduling->Set(ForceRenderThreadBit, EConsoleVariableFlags::ECVF_SetByCode);
	}

	return true;
}

void UPixelStreamingMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	// Todo: Any cleanup on capture stop should happen here.
}

bool UPixelStreamingMediaCapture::SupportsAnyThreadCapture() const
{
	// This will activate the _AnyThread method calls when true.
	return true;
}

bool UPixelStreamingMediaCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	SceneViewport = TWeakPtr<FSceneViewport>(InSceneViewport);
	OnCaptureViewportInitialized.Broadcast();

	// Listen for viewport resize events as resizes invalidate media capture, so we want to know when to reset capture
	InSceneViewport->ViewportResizedEvent.AddUObject(this, &UPixelStreamingMediaCapture::ViewportResized);

	return true;
}

void UPixelStreamingMediaCapture::ViewportResized(FViewport* Viewport, uint32 ResizeCode)
{
	bViewportResized = true;
}
