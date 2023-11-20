// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceHandle.h"
#include "CaptureSourceFactory.h"

#include "Async/CallbackSynchronizer.h"

using FCaptureSourceId = int32;

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceManager
{
public:

	using FCaptureSourceResult = FCaptureValueResult<FCaptureSourceId>;

	DECLARE_DELEGATE_OneParam(FDiscoverCallback, TArray<FCaptureSourceResult>);
	
	FCaptureSourceManager();

	FCaptureSourceHandle GetCaptureSource(FCaptureSourceId InCaptureSourceId);
	TArray<FCaptureSourceId> GetCaptureSourceIds() const;

	void RemoveCaptureSource(FCaptureSourceId InCaptureSourceId);

	void DiscoverCaptureSources(FDiscoverCallback InCallback);
	void DiscoverCaptureSourcesForFactory(FString InCaptureSourceFactoryId, FDiscoverCallback InCallback);

	void RegisterCaptureSourceFactory(TUniquePtr<FCaptureSourceFactory> InCaptureSourceFactory);

	FCaptureSourceDescriptor GetCaptureSourceFactory(FString InCaptureSourceFactoryId);
	TArray<FCaptureSourceDescriptor> GetCaptureSourceFactoryList();
	FCaptureSourceResult CreateCaptureSource(FString InCaptureSourceFactoryId, FString InName, TMap<FString, FPropertyValue> InCreationParamsValue);

	FString GetCaptureSourceFactoryById(FCaptureSourceId InCaptureSourceId);

private:

	FCaptureSourceManager(const FCaptureSourceManager& InOther) = delete;
	FCaptureSourceManager& operator=(const FCaptureSourceManager& InOther) = delete;

	TMap<FCaptureSourceId, TUniquePtr<FCaptureSource>> CaptureSources;
	TMap<FString, TUniquePtr<FCaptureSourceFactory>> CaptureSourceFactoryList;

	std::atomic<FCaptureSourceId> CurrentCaptureSourceId;
};