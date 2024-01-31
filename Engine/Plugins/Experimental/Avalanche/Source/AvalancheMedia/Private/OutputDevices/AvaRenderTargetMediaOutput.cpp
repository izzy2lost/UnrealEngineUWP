// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputDevices/AvaRenderTargetMediaOutput.h"

#include "Engine/TextureRenderTarget2D.h"
#include "OutputDevices/AvaRenderTargetMediaCapture.h"

bool UAvaRenderTargetMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!Super::Validate(OutFailureReason))
	{
		return false;
	}

	const UTextureRenderTarget2D* LoadedRenderTarget = RenderTarget.LoadSynchronous();
	if (!LoadedRenderTarget)
	{
		OutFailureReason = FString::Printf(TEXT("Missing render target object: \"%s\"."), *RenderTarget.ToString());
		return false;
	}
	return true;
}

FIntPoint UAvaRenderTargetMediaOutput::GetRequestedSize() const
{
	const UTextureRenderTarget2D* LoadedRenderTarget = RenderTarget.LoadSynchronous();
	return (LoadedRenderTarget) ? FIntPoint(LoadedRenderTarget->SizeX, LoadedRenderTarget->SizeY) : RequestCaptureSourceSize;
}


EPixelFormat UAvaRenderTargetMediaOutput::GetRequestedPixelFormat() const
{
	const UTextureRenderTarget2D* LoadedRenderTarget = RenderTarget.LoadSynchronous();
	return (LoadedRenderTarget) ? LoadedRenderTarget->GetFormat() : EPixelFormat::PF_Unknown;
}

EMediaCaptureConversionOperation UAvaRenderTargetMediaOutput::GetConversionOperation(EMediaCaptureSourceType /*InSourceType*/) const
{
	return bInvertKeyOutput ? EMediaCaptureConversionOperation::INVERT_ALPHA : EMediaCaptureConversionOperation::NONE;
}

#if WITH_EDITOR
void UAvaRenderTargetMediaOutput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// The source name is used to display as the "device name" in the broadcast editor.
	SourceName = RenderTarget.GetAssetName();
	
	// Fallback in case the path is not set.
	if (SourceName.IsEmpty())
	{
		SourceName = GetFName().ToString();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

UMediaCapture* UAvaRenderTargetMediaOutput::CreateMediaCaptureImpl()
{
	UMediaCapture* Result = NewObject<UAvaRenderTargetMediaCapture>();
	if (Result)
	{
		UE_LOG(LogAvaRenderTargetMedia, Log, TEXT("Created Motion Design Render Target Media Capture"));
		Result->SetMediaOutput(this);
	}
	return Result;
}
