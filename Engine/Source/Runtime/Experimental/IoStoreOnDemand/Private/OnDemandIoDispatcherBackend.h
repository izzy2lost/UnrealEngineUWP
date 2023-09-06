// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/SharedPointer.h"

class IIasCache;
struct FAnalyticsEventAttribute;

namespace UE::IO::IAS
{

enum class EOnDemandEndpointType
{
	CDN = 1,
	ZEN
};

struct FOnDemandEndpoint
{
	EOnDemandEndpointType EndpointType;
	FString DistributionUrl;
	FString ServiceUrl;
	FString TocPath;

	bool IsValid() const
	{
		return (DistributionUrl.Len() > 0 || ServiceUrl.Len() > 0) && TocPath.Len() > 0;
	}
};

class IOnDemandIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandIoDispatcherBackend() = default;

	virtual void Mount(const FOnDemandEndpoint& Endpoint) = 0;
	virtual void SetBulkOptionalEnabled(bool bInEnabled) = 0;
	virtual void SetEnabled(bool bInEnabled) = 0;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const = 0;
};

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TSharedPtr<IIasCache> Cache);

} // namespace UE::IO::IAS
