// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"

#include "CaptureSourceError.h"

struct CAPTURESOURCEFRAMEWORK_API FReachableEvent : public FCaptureEvent
{
	static const FString Name;

	FReachableEvent(bool bInIsReachable)
		: FCaptureEvent(Name)
		, bIsReachable(bInIsReachable)
	{
	}

	bool bIsReachable;
};

namespace UE::CaptureManager::Private
{
class ICaptureSource
{
public:
	
	using FCapabilityMap = TMap<FString, FCaptureSourceCapability*>;

	virtual ~ICaptureSource() = default;

	virtual FCaptureVoidResult Start() = 0;
	virtual FCaptureVoidResult Stop() = 0;

	virtual FCaptureSourceCapability* GetCapability(FString InName) const = 0;
	virtual TArray<FString> GetCapabilities() const = 0;

	virtual const FString& GetCaptureSourceFactoryId() const = 0;
	virtual const FString& GetName() const = 0;

	virtual const TMap<FString, FPropertyValue>& GetCreationParams() const = 0;
};
}

class CAPTURESOURCEFRAMEWORK_API FCaptureSource : 
	public UE::CaptureManager::Private::ICaptureSource,
	public FCaptureEventSource
{
public:

	FCaptureSource(const FString& InCaptureSourceFactoryId, const FString& InName);
	virtual ~FCaptureSource() = default;

	virtual FCaptureSourceCapability* GetCapability(FString InName) const final;
	virtual TArray<FString> GetCapabilities() const final;

	virtual const FString& GetCaptureSourceFactoryId() const final;
	virtual const FString& GetName() const final;

	void SetCreationParams(TMap<FString, FPropertyValue> InCreationParams);
	virtual const TMap<FString, FPropertyValue>& GetCreationParams() const final;

protected:

	void AddCapability(FCaptureSourceCapability* InCapability);

private:

	FCaptureSource(const FCaptureSource& InOther) = delete;
	FCaptureSource& operator=(const FCaptureSource& InOther) = delete;

	const FString CaptureSourceFactoryId;
	FString Name;

	FCapabilityMap CapabilityMap;

	TMap<FString, FPropertyValue> CreationParams;
};