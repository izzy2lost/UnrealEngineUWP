// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StorageServerConnection.h"

#if !UE_BUILD_SHIPPING

class FStorageConnectionSocketFSocket : public IStorageConnectionSocket
{
public:
	explicit FStorageConnectionSocketFSocket(FSocket* InSocket);
	~FStorageConnectionSocketFSocket() override;

	bool Send(const uint8* Data, const uint64 DataSize) override;
	bool Recv(uint8* Data, const uint64 DataSize, uint64& BytesRead, ESocketReceiveFlags::Type ReceiveFlags) override;
	bool HasPendingData(uint64& PendingDataSize) const override;
	void Close() override;

private:
	FSocket* Socket;
};

class FStorageSocketConnectionBackend : public FStorageConnectionBackend
{
public:
	FStorageSocketConnectionBackend(FStorageServerConnection& InOwner, ISocketSubsystem& InSocketSubsystem)
		: FStorageConnectionBackend(InOwner)
		, SocketSubsystem(InSocketSubsystem)
	{
	}

	~FStorageSocketConnectionBackend() override;

	IStorageConnectionSocket* AcquireSocketFromPool() override;
	IStorageConnectionSocket* AcquireNewSocket(float TimeoutSeconds) override;
	void ReleaseSocket(IStorageConnectionSocket* Socket, bool bKeepAlive) override;

	FString GetHostName() override
	{
		return Hostname.ToString();
	}

private:
	bool InitializeInternal(TArrayView<const FString> InHostAddresses, int32 InPort) override;

private:
	int32 HandshakeRequest(TArrayView<const TSharedPtr<FInternetAddr>> HostAddresses);
	void SortHostAddressesByLocalSubnet(TArrayView<const TSharedPtr<FInternetAddr>> HostAddresses, TArray<TSharedPtr<FInternetAddr>>& SortedHostAddresses);

private:
	ISocketSubsystem& SocketSubsystem;
	TSharedPtr<FInternetAddr> ServerAddr;
	TAnsiStringBuilder<1024> Hostname;
	TArray<IStorageConnectionSocket*> SocketPool;
	FCriticalSection SocketPoolCS;
};

#endif // !UE_BUILD_SHIPPING