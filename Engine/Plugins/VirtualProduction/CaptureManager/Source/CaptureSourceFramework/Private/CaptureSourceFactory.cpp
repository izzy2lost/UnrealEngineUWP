// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceFactory.h"

FCaptureSourceDescriptor::FCaptureSourceDescriptor(const FString& InId, TArray<FPropertyDesc> InCreationParams)
	: Id(InId)
	, CreationParams(MoveTemp(InCreationParams))
{
}

void FCaptureSourceDescriptor::SetDiscoverable(bool bInIsDiscoverable)
{
	bIsDiscoverable = bInIsDiscoverable;
}

bool FCaptureSourceDescriptor::IsDiscoverable() const
{
	return bIsDiscoverable;
}

void FCaptureSourceDescriptor::AddCreationParam(FPropertyDesc InParam)
{
	CreationParams.Push(MoveTemp(InParam));
}

const TArray<FPropertyDesc>& FCaptureSourceDescriptor::GetCreationParams() const
{
	return CreationParams;
}

const FString& FCaptureSourceDescriptor::GetId() const
{
	return Id;
}

FCaptureVoidResult FCaptureSourceFactory::Discover(FDiscoverCallback InCallback)
{
	if (!IsDiscoverable())
	{
		InCallback.ExecuteIfBound(TArray<FCaptureSourceResult>());

		return MakeError(FCaptureSourceError(ECaptureSourceError::NotSupported));
	}

	FCaptureVoidResult Result = MakeValue();

	DiscoverImpl(MoveTemp(InCallback), Result);

	return Result;
}

FCaptureSourceFactory::FCaptureSourceResult FCaptureSourceFactory::CreateCaptureSource(const FString& InName, TMap<FString, FPropertyValue> InCreationParamsValue)
{
	FCaptureSourceResult CaptureSourceResult = CreateCaptureSourceImpl(InName, InCreationParamsValue);

	if (CaptureSourceResult.IsValid())
	{
		CaptureSourceResult.GetValue()->SetCreationParams(MoveTemp(InCreationParamsValue));
	}

	return CaptureSourceResult;
}

FCaptureSourceDescriptor FCaptureSourceFactory::CreateCaptureSourceDescriptor() const
{
	TArray<FPropertyDesc> MandatoryCreationParams;
	FCaptureSourceDescriptor Descriptor(GetCaptureSourceFactoryId(), MoveTemp(MandatoryCreationParams));

	CreateCaptureSourceDescriptorImpl(Descriptor);

	return Descriptor;
}