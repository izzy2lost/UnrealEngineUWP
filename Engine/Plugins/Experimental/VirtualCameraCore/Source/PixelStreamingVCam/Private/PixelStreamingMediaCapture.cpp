// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaCapture.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelCaptureInputFrameRHI.h"
#include "Slate/SceneViewport.h"
#include "PixelStreamingVCamLog.h"
#include "PixelStreamingCodec.h"
#include "IPixelStreamingModule.h"

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

void UPixelStreamingMediaCapture::OnFrameCaptured_RenderingThread(
		const FCaptureBaseData& InBaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
		void* InBuffer,
		int32 Width,
		int32 Height,
		int32 BytesPerRow)
{
	// Todo: implement this if we want to support cpu readback captures
}

bool UPixelStreamingMediaCapture::InitializeCapture()
{
	UE_LOG(LogPixelStreamingVCam, Log, TEXT("Initializing media capture for Pixel Streaming VCam."));
	bViewportResized = false;
	bDoGPUCopy = true;

	ConfigureThreadCaptureMode(SupportsAnyThreadCapture());

	SetState(EMediaCaptureState::Capturing);

	return true;
}

void UPixelStreamingMediaCapture::ConfigureThreadCaptureMode(bool bForceRenderThread)
{
	char ForceRenderThreadBit = bForceRenderThread ? 0 : 1;

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
}

void UPixelStreamingMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	// Todo: Any cleanup on capture stop should happen here.
}

// This will activate the _AnyThread method calls when true.
bool UPixelStreamingMediaCapture::SupportsAnyThreadCapture() const
{
	EPixelStreamingCodec SelectedCodec = IPixelStreamingModule::Get().GetCodec();
	// If we are using VP8 or VP9 we want to ensure capture happens on the render thread as we do our capture/convert to I420 there
	bool bForceRenderThread = SelectedCodec == EPixelStreamingCodec::VP8 || SelectedCodec == EPixelStreamingCodec::VP9;
	return bForceRenderThread == false;
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
