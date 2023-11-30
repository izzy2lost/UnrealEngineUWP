// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceFactory.h"

class LIVELINKHUBCAPTURESOURCE_API FLiveLinkHubCaptureSourceFactory final : public FCaptureSourceFactory
{
public:

	static const FString Id;
	static const FString Name;

	FLiveLinkHubCaptureSourceFactory();

	virtual FString GetCaptureSourceFactoryName() const;
	virtual FString GetCaptureSourceFactoryId() const;

	virtual bool IsDiscoverable() const;

private:

	virtual FCaptureSourceResult CreateCaptureSourceImpl(const FString& InName, const TMap<FString, FPropertyValue>& InCreationParamsValue);
	virtual void CreateCaptureSourceDescriptorImpl(FCaptureSourceDescriptor& OutDescriptor) const;

};