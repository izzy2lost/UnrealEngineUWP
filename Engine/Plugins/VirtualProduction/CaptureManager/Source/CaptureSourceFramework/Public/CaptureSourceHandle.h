// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSource.h"

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceHandle final :
	public UE::CaptureManager::Private::ICaptureSource,
	public ICaptureEventSource
{
public:

	FCaptureSourceHandle(const FCaptureSourceHandle& InOther) = delete;
	FCaptureSourceHandle(FCaptureSourceHandle&& InOther) = delete;
	FCaptureSourceHandle& operator=(const FCaptureSourceHandle& InOther) = delete;
	FCaptureSourceHandle& operator=(FCaptureSourceHandle&& InOther) = delete;

	virtual FCaptureVoidResult Start() final;
	virtual FCaptureVoidResult Stop() final;

	virtual FCaptureSourceCapability* GetCapability(FString InName) const final;
	virtual TArray<FString> GetCapabilities() const final;

	virtual const FString& GetCaptureSourceFactoryId() const final;
	virtual const FString& GetName() const final;

	virtual TArray<FString> GetAvailableEvents() const final;
	virtual void SubscribeToEvent(const FString& InEventName, FCaptureEventHandler InHandler) final;
	virtual void UnsubscribeAll() final;

	virtual const TMap<FString, FPropertyValue>& GetCreationParams() const final;

private:

	FCaptureSource* CaptureSource;

	FCaptureSourceHandle(FCaptureSource* InCaptureSource);

	friend class FCaptureSourceManager;
};