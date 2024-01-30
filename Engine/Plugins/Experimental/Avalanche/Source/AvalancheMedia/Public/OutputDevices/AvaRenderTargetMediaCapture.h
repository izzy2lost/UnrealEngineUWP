// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"
#include "HAL/CriticalSection.h"

#include "AvaRenderTargetMediaCapture.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvaRenderTargetMedia, Log, All);

UCLASS()
class AVALANCHEMEDIA_API UAvaRenderTargetMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

protected:
	virtual bool ShouldCaptureRHIResource() const override { return true; }
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) override;

	virtual bool InitializeCapture() override;
	virtual bool PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool PostInitializeCaptureRenderTarget(UTextureRenderTarget2D* InRenderTarget) override;
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

private:
	bool StartNewCapture(const FIntPoint& InSourceTargetSize, EPixelFormat InSourceTargetFormat);
	
	FCriticalSection RenderTargetCriticalSection;
};
