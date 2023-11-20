// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSource.h"
#include "CaptureSourceError.h" 

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceDescriptor
{
public:

	FCaptureSourceDescriptor(const FString& InId, TArray<FPropertyDesc> InCreationParams);

	void SetDiscoverable(bool bInIsDiscoverable);
	bool IsDiscoverable() const;

	void AddCreationParam(FPropertyDesc InParam);

	template<typename ... FArgs>
	void EmplaceCreationParam(FArgs&&... InArgs)
	{
		CreationParams.Emplace(Forward<FArgs>(InArgs)...);
	}

	const TArray<FPropertyDesc>& GetCreationParams() const;

	const FString& GetId() const;

private:

	FString Id;
	TArray<FPropertyDesc> CreationParams;
	bool bIsDiscoverable = true;
};

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceFactory
{
public:

	using FCaptureSourceResult = FCaptureValueResult<TUniquePtr<FCaptureSource>>;

	DECLARE_DELEGATE_OneParam(FDiscoverCallback, TArray<FCaptureSourceResult>);

	virtual ~FCaptureSourceFactory() = default;

	virtual FString GetCaptureSourceFactoryName() const = 0;
	virtual FString GetCaptureSourceFactoryId() const = 0;

	virtual bool IsDiscoverable() const = 0;
	FCaptureVoidResult Discover(FDiscoverCallback InCallback);

	FCaptureSourceResult CreateCaptureSource(const FString& InName, TMap<FString, FPropertyValue> InCreationParamsValue);
	FCaptureSourceDescriptor CreateCaptureSourceDescriptor() const;

private:

	virtual void DiscoverImpl(FDiscoverCallback InCallback, FCaptureVoidResult& OutResult) {}

	virtual FCaptureSourceResult CreateCaptureSourceImpl(const FString& InName, const TMap<FString, FPropertyValue>& InCreationParamsValue) = 0;
	virtual void CreateCaptureSourceDescriptorImpl(FCaptureSourceDescriptor& OutDescriptor) const = 0;
};