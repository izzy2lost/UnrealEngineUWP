// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputDevices/AvaRenderTargetMediaCapture.h"

#include "AvaRenderTargetUtils.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "OutputDevices/AvaRenderTargetMediaOutput.h"
#include "OutputDevices/AvaRenderTargetMediaUtils.h"
#include "Slate/SceneViewport.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY(LogAvaRenderTargetMedia);

void UAvaRenderTargetMediaCapture::OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture)
{
	FScopeLock ScopeLock(&RenderTargetCriticalSection);

	if (!RenderTarget || !RenderTarget->GetRenderTargetResource())
	{
		return;
	}
	
	const FTextureRHIRef RenderTargetRHIRef = RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture();

	if (RenderTargetRHIRef.IsValid())
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// Note: we possibly don't want to undo this for RGB8_SRGB texture. Need to test.
		constexpr bool bSRGBToLinear = true;

		float Gamma = 1.0f;
		
		if (const FTextureRenderTarget2DResource* Resource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GetResource()))
		{
			// The render targets for rendering the ava channel has bForceLinearGamma = false
			// and TargetGamma = 2.2 for float textures (to force output to gamma space).
			// Those parameters are used to determine the render target's display gamma, so
			// we need to match the same display gamma that was used for rendering in order to invert it.
			// See UAvalanchePlayback::UpdateRenderTarget.			
			const bool bBackupLinearGamma = RenderTarget->bForceLinearGamma;
			const float BackupTargetGamma = RenderTarget->TargetGamma;
			RenderTarget->bForceLinearGamma = false;
			if (AvaRenderTargetUtils::IsFloatFormat(RenderTarget))
			{
				RenderTarget->TargetGamma = GEngine ? GEngine->GetDisplayGamma() : 2.2f;
			}
			// Must invert the gamma done in post process.
			// See GetTonemapperOutputDeviceParameters (PostProcessTonemap.cpp).
			Gamma = Resource ? Resource->GetDisplayGamma()/2.2f : 1.0f;
			RenderTarget->bForceLinearGamma = bBackupLinearGamma;
			RenderTarget->TargetGamma = BackupTargetGamma;
		}

		// This will convert the color space from SCS_FinalColorSDR back to SCS_FinalToneCurveHDR.
		UE::AvaRenderTargetMediaUtils::ConvertTextureRGBGamma(RHICmdList, InTexture, RenderTargetRHIRef, bSRGBToLinear, Gamma);
	}
}

bool UAvaRenderTargetMediaCapture::InitializeCapture()
{
	return true;
}

bool UAvaRenderTargetMediaCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	bool bSuccess = false;
	const FTexture2DRHIRef& BackBuffer = InSceneViewport->GetRenderTargetTexture();
	if (BackBuffer.IsValid())
	{
		const FRHITextureDesc& Desc = BackBuffer->GetDesc();
		bSuccess = StartNewCapture(Desc.Extent, Desc.Format);
	}
	else
	{
		UE_LOG(LogAvaRenderTargetMedia, Error, TEXT("Can't start capture of a scene viewport with no back buffer."));
	}
	return bSuccess;
}

bool UAvaRenderTargetMediaCapture::PostInitializeCaptureRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	return StartNewCapture(FIntPoint(InRenderTarget->SizeX, InRenderTarget->SizeY), InRenderTarget->GetFormat());
}

void UAvaRenderTargetMediaCapture::StopCaptureImpl(bool /*bAllowPendingFrameToBeProcess*/)
{
	TRACE_BOOKMARK(TEXT("UAvaRenderTargetMediaCapture::StopCapture"));
}

bool UAvaRenderTargetMediaCapture::StartNewCapture(const FIntPoint& InSourceTargetSize, EPixelFormat InSourceTargetFormat)
{
	TRACE_BOOKMARK(TEXT("UAvaRenderTargetMediaCapture::StartNewCapture"));
	{
		FScopeLock ScopeLock(&RenderTargetCriticalSection);

		RenderTarget = nullptr;
		
		const UAvaRenderTargetMediaOutput* AvaRenderTargetMediaOutput = CastChecked<UAvaRenderTargetMediaOutput>(MediaOutput);
		if (AvaRenderTargetMediaOutput)
		{
			RenderTarget = AvaRenderTargetMediaOutput->RenderTarget.LoadSynchronous();
			if (RenderTarget)
			{
				SetState(EMediaCaptureState::Capturing);
				return true;
			}
			else
			{
				UE_LOG(LogAvaRenderTargetMedia, Error, TEXT("Missing render target object: \"%s\"."), *AvaRenderTargetMediaOutput->RenderTarget.ToString());
			}
		}
		else
		{
			UE_LOG(LogAvaRenderTargetMedia, Error, TEXT("Internal Error: Media Capture's associated Media Output is not of type \"UAvaRenderTargetMediaOutput\"."));
		}
	}
	SetState(EMediaCaptureState::Error);
	return false;
}
