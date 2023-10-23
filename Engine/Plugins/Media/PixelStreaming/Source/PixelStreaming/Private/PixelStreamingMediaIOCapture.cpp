// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaIOCapture.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelCaptureInputFrameRHI.h"
#include "Slate/SceneViewport.h"
#include "PixelStreamingPrivate.h"
#include "PixelStreamingCodec.h"
#include "PixelStreamingModule.h"

void UPixelStreamingMediaIOCapture::OnRHIResourceCaptured_RenderingThread(
	const FCaptureBaseData& InBaseData,
	TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
	FTextureRHIRef InTexture)
{
	TSharedPtr<FPixelStreamingVideoInput> VideoInputPtr = VideoInput.Pin();
	if (VideoInputPtr)
	{
		VideoInputPtr->OnFrame(FPixelCaptureInputFrameRHI(InTexture));
	}
}

void UPixelStreamingMediaIOCapture::OnRHIResourceCaptured_AnyThread(
	const FCaptureBaseData & InBaseData,
	TSharedPtr<FMediaCaptureUserData,ESPMode::ThreadSafe> InUserData,
	FTextureRHIRef InTexture)
{
	TSharedPtr<FPixelStreamingVideoInput> VideoInputPtr = VideoInput.Pin();
	if (VideoInputPtr)
	{
		VideoInputPtr->OnFrame(FPixelCaptureInputFrameRHI(InTexture));
	}
}

void UPixelStreamingMediaIOCapture::OnFrameCaptured_RenderingThread(
		const FCaptureBaseData& InBaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
		void* InBuffer,
		int32 Width,
		int32 Height,
		int32 BytesPerRow)
{
	// Todo: implement this if we want to support cpu readback captures
}

bool UPixelStreamingMediaIOCapture::InitializeCapture()
{
	UE_LOG(LogPixelStreaming, Log, TEXT("Initializing media capture for Pixel Streaming VCam."));
	bViewportResized = false;
	bDoGPUCopy = true;

	ConfigureThreadCaptureMode(SupportsAnyThreadCapture());

	SetState(EMediaCaptureState::Capturing);

	return true;
}

void UPixelStreamingMediaIOCapture::ConfigureThreadCaptureMode(bool bForceRenderThread)
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
		CVarExperimentalScheduling->Set(true, EConsoleVariableFlags::ECVF_SetByCode);
	}
}

void UPixelStreamingMediaIOCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	// Todo: Any cleanup on capture stop should happen here.
}

// This will activate the _AnyThread method calls when true.
bool UPixelStreamingMediaIOCapture::SupportsAnyThreadCapture() const
{
	EPixelStreamingCodec SelectedCodec = IPixelStreamingModule::Get().GetCodec();
	// If we are using VP8 or VP9 we want to ensure capture happens on the render thread as we do our capture/convert to I420 there
	bool bForceRenderThread = SelectedCodec == EPixelStreamingCodec::VP8 || SelectedCodec == EPixelStreamingCodec::VP9;
	return bForceRenderThread == false;
}

bool UPixelStreamingMediaIOCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	SceneViewport = TWeakPtr<FSceneViewport>(InSceneViewport);
	OnCaptureViewportInitialized.Broadcast();

	// Listen for viewport resize events as resizes invalidate media capture, so we want to know when to reset capture
	InSceneViewport->ViewportResizedEvent.AddUObject(this, &UPixelStreamingMediaIOCapture::ViewportResized);

	return true;
}

void UPixelStreamingMediaIOCapture::ViewportResized(FViewport* Viewport, uint32 ResizeCode)
{
	bViewportResized = true;
}
