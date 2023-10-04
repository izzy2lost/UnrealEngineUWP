// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpResponse.h"

/**
 * Contains implementation of some common functions that don't vary between implementations of different platforms
 */
class FHttpResponseCommon : public IHttpResponse
{
public:
	HTTP_API FHttpResponseCommon(const FString& InURL);

	// IHttpBase
	HTTP_API virtual FString GetURLParameter(const FString& ParameterName) const override;
	HTTP_API virtual FString GetURL() const override;

protected:
	FString URL;
};
