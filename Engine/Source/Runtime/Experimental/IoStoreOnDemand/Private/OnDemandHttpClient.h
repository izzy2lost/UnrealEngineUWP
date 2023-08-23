// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreHttp/Client.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

class FConnectionPool;

namespace UE::IO::IAS
{

class FOnDemandHttpClient
{
public:
	using FGetCallback = TFunction<void(TIoStatusOr<FIoBuffer>, uint64 DurationMs)>;

	FOnDemandHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount = 8);
	~FOnDemandHttpClient() = default;

	const FString& ServiceUrl() const 
	{
		return SvcsUrl;
	}

	int32 MaxConnectionCount() const
	{
		return MaxConnections;
	}

	void Get(FAnsiStringView Url, FGetCallback&& Callback);
	void Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FGetCallback&& Callback);

	/** @return True if the client has pending work otherwise false. */
	bool Tick(bool Block = false);

private:
	void Issue(FAnsiStringView Url, FGetCallback&& Callback, FIoOffsetAndLength Range = FIoOffsetAndLength());

	FString SvcsUrl;
	int32 MaxConnections;
	HTTP::FEventLoop EventLoop;
	TUniquePtr<HTTP::FConnectionPool> ConnectionPool;
};

} // namespace UE::IO::IAS
