// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelStreamingMediaIOCapture.h"
#include "PixelStreamingVideoInputMediaCapture.generated.h"

UCLASS(BlueprintType)
class UPixelStreamingMediaIOOutput : public UMediaOutput
{
	GENERATED_BODY()

public:
	virtual FIntPoint GetRequestedSize() const override { return UMediaOutput::RequestCaptureSourceSize; }
	virtual EPixelFormat GetRequestedPixelFormat() const override { return EPixelFormat::PF_B8G8R8A8; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UPixelStreamingMediaIOCapture> MediaCapture = nullptr;
};

/*
 * Use this if you want to send VCam output as video input.
 */
class PIXELSTREAMING_API FPixelStreamingVideoInputMediaCapture : public FPixelStreamingVideoInputRHI, public TSharedFromThis<FPixelStreamingVideoInputMediaCapture>
{
public:
	static TSharedPtr<FPixelStreamingVideoInputMediaCapture> Create();
	virtual ~FPixelStreamingVideoInputMediaCapture();

	virtual FString ToString() override;

protected:
	virtual TSharedPtr<FPixelCaptureCapturer> CreateCapturer(int32 FinalFormat, float FinalScale) override;
	
private:
	FPixelStreamingVideoInputMediaCapture();
	void StartCapture();
	void OnCaptureStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<UPixelStreamingMediaIOOutput> MediaOutput = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UPixelStreamingMediaIOCapture> MediaCapture = nullptr;
};
