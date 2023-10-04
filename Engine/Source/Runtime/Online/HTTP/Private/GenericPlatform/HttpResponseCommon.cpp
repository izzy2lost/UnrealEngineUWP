// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpResponseCommon.h"
#include "GenericPlatform/GenericPlatformHttp.h"

FHttpResponseCommon::FHttpResponseCommon(const FString& InURL)
	: URL(InURL)
{
}

FString FHttpResponseCommon::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(URL, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

FString FHttpResponseCommon::GetURL() const
{
	return URL;
}
