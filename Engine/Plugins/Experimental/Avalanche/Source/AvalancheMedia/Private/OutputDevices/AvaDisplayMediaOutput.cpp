// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputDevices/AvaDisplayMediaOutput.h"
#include "OutputDevices/AvaDisplayMediaCapture.h"

UAvaDisplayMediaOutput::UAvaDisplayMediaOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAvaDisplayMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!Super::Validate(OutFailureReason))
	{
		return false;
	}

	return true;
}

FIntPoint UAvaDisplayMediaOutput::GetRequestedSize() const
{
	return OutputConfiguration.MediaConfiguration.MediaMode.Resolution;
}

EPixelFormat UAvaDisplayMediaOutput::GetRequestedPixelFormat() const
{
	return PF_A2B10G10R10;
}

EMediaCaptureConversionOperation UAvaDisplayMediaOutput::GetConversionOperation(EMediaCaptureSourceType /*InSourceType*/) const
{
	return EMediaCaptureConversionOperation::NONE;
}

UMediaCapture* UAvaDisplayMediaOutput::CreateMediaCaptureImpl()
{
	UMediaCapture* Result = NewObject<UAvaDisplayMediaCapture>();
	if (Result)
	{
		UE_LOG(LogAvaDisplayMedia, Log, TEXT("Created Motion Design Display Media Capture"));
		Result->SetMediaOutput(this);
	}
	return Result;
}
