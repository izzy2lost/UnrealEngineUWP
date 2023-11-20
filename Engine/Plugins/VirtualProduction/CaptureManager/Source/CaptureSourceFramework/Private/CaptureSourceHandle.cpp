// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceHandle.h"

FCaptureSourceHandle::FCaptureSourceHandle(FCaptureSource* InCaptureSource)
	: CaptureSource(InCaptureSource)
{
}

FCaptureVoidResult FCaptureSourceHandle::Start()
{
	return CaptureSource->Start();
}

FCaptureVoidResult FCaptureSourceHandle::Stop()
{
	return CaptureSource->Stop();
}

FCaptureSourceCapability* FCaptureSourceHandle::GetCapability(FString InName) const
{
	return CaptureSource->GetCapability(MoveTemp(InName));
}

TArray<FString> FCaptureSourceHandle::GetCapabilities() const
{
	return CaptureSource->GetCapabilities();
}

const FString& FCaptureSourceHandle::GetCaptureSourceFactoryId() const
{
	return CaptureSource->GetCaptureSourceFactoryId();
}

const FString& FCaptureSourceHandle::GetName() const
{
	return CaptureSource->GetName();
}

TArray<FString> FCaptureSourceHandle::GetAvailableEvents() const
{
	return CaptureSource->GetAvailableEvents();
}

void FCaptureSourceHandle::SubscribeToEvent(const FString& InEventName, FCaptureEventHandler InHandler)
{
	CaptureSource->SubscribeToEvent(InEventName, MoveTemp(InHandler));
}

void FCaptureSourceHandle::UnsubscribeAll()
{
	CaptureSource->UnsubscribeAll();
}

const TMap<FString, FPropertyValue>& FCaptureSourceHandle::GetCreationParams() const
{
	return CaptureSource->GetCreationParams();
}