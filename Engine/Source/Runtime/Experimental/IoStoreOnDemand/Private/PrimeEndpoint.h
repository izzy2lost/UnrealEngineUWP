// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (IS_PROGRAM || WITH_EDITOR)

#include "Containers/StringFwd.h"
#include "IO/IoStatus.h"

namespace UE::IO::IAS
{

struct FPrimeEndpointArgs
{
	FPrimeEndpointArgs() = delete;

	FPrimeEndpointArgs(FStringView InDistributionUrl, FStringView InTocPath)
		: DistributionUrl(InDistributionUrl)
		, TocPath(InTocPath)
	{
	}

	FPrimeEndpointArgs(FString&& InDistributionUrl, FString&& InTocPath)
		: DistributionUrl(MoveTemp(InDistributionUrl))
		, TocPath(MoveTemp(InTocPath))
	{
	}

	~FPrimeEndpointArgs() = default;

	FString DistributionUrl;
	FString TocPath;
};

/** Derives FPrimeEndpointArgs based on the provided ini file */
FIoStatus PrimeEndpointInternal(const FString& IoStoreOnDemandIniPath);

FIoStatus PrimeEndpointInternal(const FPrimeEndpointArgs& Args);

} //namespace UE::IO::IAS

#endif // (IS_PROGRAM || WITH_EDITOR)
