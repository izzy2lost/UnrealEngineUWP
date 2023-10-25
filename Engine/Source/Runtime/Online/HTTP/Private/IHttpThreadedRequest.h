// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/HttpRequestCommon.h"

class IHttpThreadedRequest : public FHttpRequestCommon
{
public:
	// Called on http thread
	virtual bool StartThreadedRequest() = 0;
	virtual bool IsThreadedRequestComplete() = 0;
	virtual void TickThreadedRequest(float DeltaSeconds) = 0;

	// Can be called on game thread or http thread depend on the delegate thread policy
	virtual void FinishRequest() = 0;

protected:
	/**
	 * Finish the request when it's not in http manager
	 */
	void FinishRequestNotInHttpManager();
};
