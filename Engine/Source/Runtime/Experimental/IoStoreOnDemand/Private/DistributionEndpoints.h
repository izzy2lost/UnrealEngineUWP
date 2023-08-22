// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "HttpFwd.h"

namespace UE::IO
{

class FDistributionEndpoints
{
public:
	using FOnEndpointResolved = TFunction<void(const FString&, TConstArrayView<FString>)>;

	FDistributionEndpoints() = default;
	~FDistributionEndpoints();

#if IS_PROGRAM || WITH_EDITOR
	bool Flush(double TimeOut);
#endif //IS_PROGRAM || WITH_EDITOR

	void ResolveEndpoints(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved);
	void ResolveDeferredEndpoints();

private:
	struct FResolvedEndpoint
	{
		TArray<FString> ServiceUrls;
	};

	struct FResolveRequest
	{
		FString DistributionUrl;
		FHttpRequestPtr HttpRequest;
		TArray<FOnEndpointResolved> Callbacks;
		int32 RetryCount = 0;
	};

	void IssueEndpointRequests();
	void CancelEndpointRequests();
	void CompleteEndpointRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse);

	TMap<FString, TUniquePtr<FResolvedEndpoint>> ResolvedEndpoints;
	TMap<FString, TUniquePtr<FResolveRequest>> PendingRequests;

	FRWLock Lock;
	bool bInitialized = false;
};

} // namespace UE::IO
