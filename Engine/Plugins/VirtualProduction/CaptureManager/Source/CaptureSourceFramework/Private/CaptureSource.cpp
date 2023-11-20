// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSource.h"

const FString FReachableEvent::Name = "Reachable";

FCaptureSource::FCaptureSource(const FString& InCaptureSourceFactoryId, const FString& InName)
	: CaptureSourceFactoryId(InCaptureSourceFactoryId)
	, Name(InName)
{
	// Register event
	RegisterEvent(FReachableEvent::Name);
}

FCaptureSourceCapability* FCaptureSource::GetCapability(FString InName) const
{
	if (CapabilityMap.Contains(InName))
	{
		return CapabilityMap[InName];
	}
	
	return nullptr;
}

TArray<FString> FCaptureSource::GetCapabilities() const
{
	TArray<FString> Capabilities;

	CapabilityMap.GetKeys(Capabilities);

	return Capabilities;
}

const FString& FCaptureSource::GetCaptureSourceFactoryId() const
{
	return CaptureSourceFactoryId;
}

const FString& FCaptureSource::GetName() const
{
	return Name;
}

void FCaptureSource::SetCreationParams(TMap<FString, FPropertyValue> InCreationParams)
{
	CreationParams = MoveTemp(InCreationParams);
}

const TMap<FString, FPropertyValue>& FCaptureSource::GetCreationParams() const
{
	return CreationParams;
}

void FCaptureSource::AddCapability(FCaptureSourceCapability* InCapability)
{
	FString CapabilityName = InCapability->GetName();
	CapabilityMap.Emplace(MoveTemp(CapabilityName), InCapability);
}