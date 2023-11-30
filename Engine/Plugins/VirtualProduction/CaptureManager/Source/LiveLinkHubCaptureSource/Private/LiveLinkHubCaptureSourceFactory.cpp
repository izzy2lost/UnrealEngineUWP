// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubCaptureSourceFactory.h"

#include "LiveLinkHubCaptureSource.h"

const FString FLiveLinkHubCaptureSourceFactory::Id = TEXT("LiveLinkHubCaptureSource");
const FString FLiveLinkHubCaptureSourceFactory::Name = TEXT("LiveLink Hub Capture Source");

FLiveLinkHubCaptureSourceFactory::FLiveLinkHubCaptureSourceFactory() = default;

FString FLiveLinkHubCaptureSourceFactory::GetCaptureSourceFactoryName() const
{
	return Name;
}

FString FLiveLinkHubCaptureSourceFactory::GetCaptureSourceFactoryId() const
{
	return Id;
}

bool FLiveLinkHubCaptureSourceFactory::IsDiscoverable() const
{
	return false;
}

FLiveLinkHubCaptureSourceFactory::FCaptureSourceResult FLiveLinkHubCaptureSourceFactory::CreateCaptureSourceImpl(const FString& InName, 
																												 const TMap<FString, FPropertyValue>& InCreationParamsValue)
{
	return MakeValue(MakeUnique<FLiveLinkHubCaptureSource>(InName));
}

void FLiveLinkHubCaptureSourceFactory::CreateCaptureSourceDescriptorImpl(FCaptureSourceDescriptor& OutDescriptor) const
{
	OutDescriptor.SetDiscoverable(IsDiscoverable());
}