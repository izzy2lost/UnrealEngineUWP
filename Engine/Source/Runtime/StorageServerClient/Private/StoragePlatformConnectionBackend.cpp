// Copyright Epic Games, Inc. All Rights Reserved.

#include "StoragePlatformConnectionBackend.h"

#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "HAL/PlatformProcess.h"

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogStorageServerPlatformBackend, Log, All);

FStorageConnectionPlatformSocket::FStorageConnectionPlatformSocket(IPlatformHostCommunication* HostCommunication, IPlatformHostSocketPtr InSocket, int32 InProtocolNumber)
	: Communication(HostCommunication)
	, Socket(InSocket)
	, ConnectionBuffer(1024 * 64)
	, ProtocolNumber(InProtocolNumber)
{
}

FStorageConnectionPlatformSocket::~FStorageConnectionPlatformSocket()
{
	Close();
}

bool FStorageConnectionPlatformSocket::Send(const uint8* Data, const uint64 DataSize)
{
	if (!Socket || Socket->GetState() != IPlatformHostSocket::EConnectionState::Connected)
	{
		return false;
	}

	bool bSuccess = Socket->Send(Data, DataSize) == IPlatformHostSocket::EResultNet::Ok;
	return bSuccess;
}

bool FStorageConnectionPlatformSocket::Recv(uint8* Data, const uint64 DataSize, uint64& BytesRead, ESocketReceiveFlags::Type ReceiveFlags)
{
	if (!Socket || Socket->GetState() != IPlatformHostSocket::EConnectionState::Connected)
	{
		return false;
	}

	if (ConnectionBuffer.GetCapacity() < DataSize)
	{
		UE_LOG(LogStorageServerPlatformBackend, Display, TEXT("ConnectionBuffer capacity is lower than requested data read (%d vs %d)"), ConnectionBuffer.GetCapacity(), DataSize);
	}

	BytesRead = 0;
	if (ConnectionBuffer.IsEmpty())
	{
		uint8 Buffer[1024];
		if (Socket->Receive(Buffer, sizeof(Buffer), BytesRead, IPlatformHostSocket::EReceiveFlags::DontWait) != IPlatformHostSocket::EResultNet::Ok)
		{
			return false;
		}

		if (!ConnectionBuffer.Put(Buffer, BytesRead))
		{
			UE_LOG(LogStorageServerPlatformBackend, Display, TEXT("Couldn't fit the received data in the connection buffer"));
		}
	}

	if (ReceiveFlags == ESocketReceiveFlags::Peek)
	{
		ConnectionBuffer.Peek(Data, DataSize, BytesRead);
	}
	else if (ReceiveFlags == ESocketReceiveFlags::WaitAll)
	{
		ConnectionBuffer.Consume(Data, DataSize, BytesRead);
		uint64 TotalBytesRead = BytesRead;

		while (TotalBytesRead < DataSize)
		{
			const uint64 BytesToRead = DataSize - TotalBytesRead;
			if (Socket->Receive(Data + TotalBytesRead, BytesToRead, BytesRead, IPlatformHostSocket::EReceiveFlags::WaitAll) != IPlatformHostSocket::EResultNet::Ok)
			{
				return false;
			}

			TotalBytesRead += BytesRead;

			if (TotalBytesRead > DataSize)
			{
				UE_LOG(LogStorageServerPlatformBackend, Display, TEXT("Exceeded what was supposed to be downloaded"));
			}
		}

		BytesRead = TotalBytesRead;
	}
	else if (ReceiveFlags == ESocketReceiveFlags::None)
	{
		ConnectionBuffer.Consume(Data, DataSize, BytesRead);
	}

	return true;
}

bool FStorageConnectionPlatformSocket::HasPendingData(uint64& PendingDataSize) const
{
	PendingDataSize = ConnectionBuffer.GetSize();
	return PendingDataSize > 0;
}

void FStorageConnectionPlatformSocket::Close()
{
	Communication->CloseConnection(Socket);
}

FStorageServerPlatformConnectionBackend::FStorageServerPlatformConnectionBackend(FStorageServerConnection& InOwner)
	: FStorageConnectionBackend(InOwner)
{
}

FStorageServerPlatformConnectionBackend::~FStorageServerPlatformConnectionBackend()
{
	for (auto& Socket : SocketPool)
	{
		delete Socket;
	}
}

IStorageConnectionSocket* FStorageServerPlatformConnectionBackend::AcquireNewSocket(float TimeoutSeconds)
{
	FScopeLock Lock(&SocketPoolCS);

	int32 FirstAvailableProtocolNumber = UsedSockets.Find(false);
	if (FirstAvailableProtocolNumber == -1)
	{
		SocketPoolFreeConditionVariable.Wait(SocketPoolCS);
		return AcquireSocketFromPool();
	}

	IPlatformHostSocketPtr PlatformSocket = Communication->OpenConnection(FirstAvailableProtocolNumber, *FString::Printf(TEXT("PlatformSocket %d"), FirstAvailableProtocolNumber));
	if (!PlatformSocket)
	{
		return nullptr;
	}

	float WaitingFor = 0.f;
	const float SleepTime = 0.01f;
	IPlatformHostSocket::EConnectionState ConnectionState = PlatformSocket->GetState();
	while (ConnectionState == IPlatformHostSocket::EConnectionState::Created)
	{
		if ((TimeoutSeconds != -1.f) && (WaitingFor > TimeoutSeconds))
		{
			Communication->CloseConnection(PlatformSocket);
			UE_LOG(LogStorageServerPlatformBackend, Error, TEXT("Platform connection timed out"));
			return nullptr;
		}

		FPlatformProcess::Sleep(SleepTime);
		WaitingFor += SleepTime;
		ConnectionState = PlatformSocket->GetState();
	}

	if (ConnectionState != IPlatformHostSocket::EConnectionState::Connected)
	{
		Communication->CloseConnection(PlatformSocket);
		return nullptr;
	}

	FStorageConnectionPlatformSocket* NewSocket = new FStorageConnectionPlatformSocket(Communication, PlatformSocket, FirstAvailableProtocolNumber);
	UsedSockets[FirstAvailableProtocolNumber] = true;

	return NewSocket;
}

IStorageConnectionSocket* FStorageServerPlatformConnectionBackend::AcquireSocketFromPool()
{
	FScopeLock Lock(&SocketPoolCS);

	if (!SocketPool.IsEmpty())
	{
		return SocketPool.Pop(EAllowShrinking::No);
	}

	return nullptr;
}

void FStorageServerPlatformConnectionBackend::ReleaseSocket(IStorageConnectionSocket* InSocket, bool bKeepAlive)
{
	FScopeLock Lock(&SocketPoolCS);

	if (bKeepAlive)
	{
		uint64 PendingDataSize;
		if (!InSocket->HasPendingData(PendingDataSize))
		{
			SocketPool.Push(InSocket);
			SocketPoolFreeConditionVariable.NotifyOne();
			return;
		}
		UE_LOG(LogStorageServerPlatformBackend, Error, TEXT("Socket was not fully drained"));
	}

	int32 ProtocolNumber = static_cast<FStorageConnectionPlatformSocket*>(InSocket)->GetProtocolNumber();
	UsedSockets[ProtocolNumber] = false;
	SocketPoolFreeConditionVariable.NotifyOne();
	InSocket->Close();
	delete InSocket;
}

bool FStorageServerPlatformConnectionBackend::InitializeInternal(TArrayView<const FString> InHostAddresses, int32 Port)
{
	Communication = &FPlatformMisc::GetPlatformHostCommunication();
	if (!Communication)
	{
		return false;
	}

	UsedSockets.Init(false, 10);

	int32 ServerVersion = HandshakeRequest(InHostAddresses);
	if (ServerVersion != 1)
	{
		return false;
	}

	return true;
}

int32 FStorageServerPlatformConnectionBackend::HandshakeRequest(TArrayView<const FString> HostAddresses)
{
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(BaseURI);

	for (const FString& Addr : HostAddresses)
	{
		Hostname.Reset();
		Hostname.Append(*Addr);

		UE_LOG(LogStorageServerPlatformBackend, Display, TEXT("Trying to handshake with Zen at '%s'"), *Addr);
		const float ConnectionTimeoutSeconds = 5.0f;

		FStorageServerRequest Request("GET", *ResourceBuilder, Hostname);
		if (IStorageConnectionSocket* Socket = Request.Send(Owner, false))
		{
			FStorageServerResponse Response(Owner, *Socket);

			if (Response.IsOk())
			{
				FCbObject ResponseObj = Response.GetResponseObject();
				return 1;
			}
			else
			{
				UE_LOG(LogStorageServerPlatformBackend, Fatal, TEXT("Failed to handshake with Zen at %s. '%s'"), *Addr, *Response.GetErrorMessage());
			}
		}
	}

	UE_LOG(LogStorageServerPlatformBackend, Fatal, TEXT("Failed to handshake with Zen at any of host addresses."));

	return -1;
}

#endif // !UE_BUILD_SHIPPING
