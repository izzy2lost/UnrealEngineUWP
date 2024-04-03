// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StorageServerConnection.h"
#include "Experimental/Async/ConditionVariable.h"
#include "GenericPlatform/GenericPlatformHostCommunication.h"
#include "GenericPlatform/GenericPlatformHostSocket.h"

class FConnectionCircularBuffer
{
public:
	explicit FConnectionCircularBuffer(uint64 InCapacity)
		: CapacityMask(FMath::RoundUpToPowerOfTwo(InCapacity) - 1)
	{
		Allocation = new uint8[GetCapacity()];
	}

	~FConnectionCircularBuffer()
	{
		delete[] Allocation;
	}

	uint8 operator[](uint64 Index) const
	{
		return const_cast<FConnectionCircularBuffer*>(this)->operator[](Index);
	}

	uint8& operator[](uint64 Index)
	{
		check(Index < GetSize());
		uint64 DataOffset = (Index + Tail) & (CapacityMask);
		return Allocation[DataOffset];
	}

	uint64 GetSize() const
	{
		return Size;
	}

	uint64 GetCapacity() const
	{
		return CapacityMask + 1;
	}

	bool IsEmpty() const
	{
		return Size == 0;
	}

	void Clear()
	{
		Tail = 0;
		Head = 0;
		Size = 0;
	}

	uint64 SpaceLeft() const
	{
		return GetCapacity() - Size;
	}

	void Peek(uint8* Data, const uint64 DataSize, uint64& OutSize)
	{
		OutSize = FMath::Min(DataSize, Size);

		if (OutSize + Tail > GetCapacity())
		{
			uint64 LenToEnd = GetCapacity() - Tail;
			uint64 LenFromBegin = OutSize - LenToEnd;
			FMemory::Memcpy(Data, Allocation + Tail, LenToEnd);
			FMemory::Memcpy(Data + LenToEnd, Allocation, LenFromBegin);
		}
		else
		{
			FMemory::Memcpy(Data, Allocation + Tail, OutSize);
		}
	}

	void Consume(uint8* Data, const uint64 DataSize, uint64& OutSize)
	{
		Peek(Data, DataSize, OutSize);
		Tail = (Tail + OutSize) & CapacityMask;

		Size -= OutSize;

		if (Size == 0)
		{
			Head = 0;
			Tail = 0;
		}
	}

	bool Put(uint8* Data, uint64 DataSize)
	{
		if (DataSize > SpaceLeft())
		{
			return false;
		}

		if (DataSize + Head > GetCapacity())
		{
			uint64 LenToEnd = GetCapacity() - Head;
			uint64 LenFromBegin = DataSize - LenToEnd;
			FMemory::Memcpy(Allocation + Head, Data, LenToEnd);
			FMemory::Memcpy(Allocation, Data + LenToEnd, LenFromBegin);
		}
		else
		{
			FMemory::Memcpy(Allocation + Head, Data, DataSize);
		}

		Head = (Head + DataSize) & (CapacityMask);

		Size += DataSize;

		return true;
	}

private:
	uint8* Allocation{ nullptr };
	uint64 CapacityMask{ 0 };
	uint64 Size{ 0 };
	uint64 Head{ 0 };
	uint64 Tail{ 0 };
};

class FStorageConnectionPlatformSocket : public IStorageConnectionSocket
{
public:
	FStorageConnectionPlatformSocket(IPlatformHostCommunication* HostCommunication, IPlatformHostSocketPtr InSocket, int32 InProtocolNumber);
	~FStorageConnectionPlatformSocket() override;

	bool Send(const uint8* Data, const uint64 DataSize) override;
	bool Recv(uint8* Data, const uint64 DataSize, uint64& BytesRead, ESocketReceiveFlags::Type ReceiveFlags) override;
	bool HasPendingData(uint64& PendingDataSize) const override;
	void Close() override;

	int32 GetProtocolNumber() const { return ProtocolNumber; }

private:
	IPlatformHostCommunication* Communication;
	IPlatformHostSocketPtr Socket;
	FConnectionCircularBuffer ConnectionBuffer;
	int32 ProtocolNumber;
};

class FStorageServerPlatformConnectionBackend : public FStorageConnectionBackend
{
public:
	FStorageServerPlatformConnectionBackend(FStorageServerConnection& InOwner);
	~FStorageServerPlatformConnectionBackend() override;

	IStorageConnectionSocket* AcquireNewSocket(float TimeoutSeconds) override;
	IStorageConnectionSocket* AcquireSocketFromPool() override;
	void ReleaseSocket(IStorageConnectionSocket* Socket, bool bKeepAlive) override;

protected:
	bool InitializeInternal(TArrayView<const FString> InHostAddresses, int32 Port) override;

private:
	int32 HandshakeRequest(TArrayView<const FString> HostAddresses);

private:
	IPlatformHostCommunication* Communication{ nullptr };

	FCriticalSection SocketPoolCS;
	UE::FConditionVariable SocketPoolFreeConditionVariable;
	TArray<IStorageConnectionSocket*> SocketPool;
	TBitArray<> UsedSockets; // bitset to keep track of used sockets in the pool

	TAnsiStringBuilder<1024> Hostname;
};