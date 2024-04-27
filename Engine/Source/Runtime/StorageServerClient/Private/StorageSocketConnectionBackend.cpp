// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageSocketConnectionBackend.h"

#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogStorageSocketBackend, Log, All);

static TArray<TSharedPtr<FInternetAddr>> GetAddressFromString(ISocketSubsystem& SocketSubsystem, TArrayView<const FString> HostAddresses, const int32 Port)
{
	TArray<TSharedPtr<FInternetAddr>> InternetAddresses;
	FString ModifiedHostAddr;

	for (const FString& HostAddr : HostAddresses)
	{
		// Numeric IPV6 addresses can be enclosed in brackets, and must have the brackets stripped before calling GetAddressFromString
		const FString* EffectiveHostAddr = &HostAddr;
		if (!HostAddr.IsEmpty() && HostAddr[0] == TEXT('[') && HostAddr[HostAddr.Len() - 1] == TEXT(']'))
		{
#if PLATFORM_HAS_BSD_SOCKETS && !PLATFORM_HAS_BSD_IPV6_SOCKETS
			// If the platform doesn't have IPV6 BSD Sockets, then handle an attempt at conversion of loopback addresses, and skip and warn about other addresses
			if (HostAddr == TEXT("[::1]"))
			{
				// Substitute IPV4 loopback for IPV6 loopback
				ModifiedHostAddr = TEXT("127.0.0.1");
			}
			else
			{
				UE_LOG(LogStorageSocketBackend, Warning, TEXT("Ignoring storage server host IPV6 address on platform that doesn't support IPV6: %s"), *HostAddr);
				continue;
			}
#else
			ModifiedHostAddr = FStringView(HostAddr).Mid(1, HostAddr.Len() - 2);
#endif
			EffectiveHostAddr = &ModifiedHostAddr;
		}
		TSharedPtr<FInternetAddr> Addr = SocketSubsystem.GetAddressFromString(*EffectiveHostAddr);

		if (!Addr.IsValid() || !Addr->IsValid())
		{
			FAddressInfoResult GAIRequest = SocketSubsystem.GetAddressInfo(**EffectiveHostAddr, nullptr, EAddressInfoFlags::Default, NAME_None);
			if (GAIRequest.ReturnCode == SE_NO_ERROR && GAIRequest.Results.Num() > 0)
			{
				Addr = GAIRequest.Results[0].Address;
			}
		}

		if (Addr.IsValid() && Addr->IsValid())
		{
			Addr->SetPort(Port);
			InternetAddresses.Emplace(MoveTemp(Addr));
		}
	}

	return InternetAddresses;
}

FStorageConnectionSocketFSocket::FStorageConnectionSocketFSocket(FSocket* InSocket)
	: Socket(InSocket)
{
}

FStorageConnectionSocketFSocket::~FStorageConnectionSocketFSocket()
{
	if (Socket)
	{
		Socket->Close();
		delete Socket;
		Socket = nullptr;
	}
}

bool FStorageConnectionSocketFSocket::Send(const uint8* Data, const uint64 DataSize)
{
	if (!Socket)
	{
		return false;
	}

	int32 BytesSent;
	return Socket->Send(Data, DataSize, BytesSent);
}

bool FStorageConnectionSocketFSocket::Recv(uint8* Data, const uint64 DataSize, uint64& BytesRead, ESocketReceiveFlags::Type ReceiveFlags)
{
	if (!Socket)
	{
		return false;
	}

	int32 ReadBytes = 0;
	if (!Socket->Recv(Data, DataSize, ReadBytes, ReceiveFlags))
	{
		return false;
	}

	BytesRead = ReadBytes;
	return true;
}

bool FStorageConnectionSocketFSocket::HasPendingData(uint64& PendingDataSize) const
{
	uint32 PendingData;
	bool bRes = Socket->HasPendingData(PendingData);

	PendingDataSize = PendingData;
	return bRes;
}

void FStorageConnectionSocketFSocket::Close()
{
	Socket->Close();
}

FStorageSocketConnectionBackend::~FStorageSocketConnectionBackend()
{
	for (IStorageConnectionSocket* Socket : SocketPool)
	{
		delete Socket;
	}
}

IStorageConnectionSocket* FStorageSocketConnectionBackend::AcquireSocketFromPool()
{
	FScopeLock Lock(&SocketPoolCS);

	if (!SocketPool.IsEmpty())
	{
		return SocketPool.Pop(EAllowShrinking::No);
	}

	return nullptr;
}

IStorageConnectionSocket* FStorageSocketConnectionBackend::AcquireNewSocket(float TimeoutSeconds)
{
	FSocket* Socket = SocketSubsystem.CreateSocket(NAME_Stream, TEXT("StorageServer"), ServerAddr->GetProtocolType());

	if (TimeoutSeconds > 0.0f)
	{
		Socket->SetNonBlocking(true);
		ON_SCOPE_EXIT
		{
			Socket->SetNonBlocking(false);
		};

		if (Socket->Connect(*ServerAddr) && Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromSeconds(TimeoutSeconds)))
		{
			return new FStorageConnectionSocketFSocket(Socket);
		}
	}
	else
	{
		if (Socket->Connect(*ServerAddr))
		{
			return new FStorageConnectionSocketFSocket(Socket);
		}
	}

	delete Socket;
	return nullptr;
}

void FStorageSocketConnectionBackend::ReleaseSocket(IStorageConnectionSocket* Socket, bool bKeepAlive)
{
	if (bKeepAlive)
	{
		uint64 PendingDataSize;
		if (!Socket->HasPendingData(PendingDataSize))
		{
			FScopeLock Lock(&SocketPoolCS);
			SocketPool.Push(Socket);
			return;
		}
		UE_LOG(LogStorageSocketBackend, Fatal, TEXT("Socket was not fully drained"));
	}
	Socket->Close();
	delete Socket;
}

bool FStorageSocketConnectionBackend::InitializeInternal(TArrayView<const FString> InHostAddresses, int32 InPort)
{
	TArray<TSharedPtr<FInternetAddr>> HostAddresses = GetAddressFromString(SocketSubsystem, InHostAddresses, InPort);

	if (!HostAddresses.Num())
	{
		UE_LOG(LogStorageSocketBackend, Fatal, TEXT("No valid Zen store host address specified"));
		return false;
	}

	const int32 ServerVersion = HandshakeRequest(HostAddresses);
	if (ServerVersion != 1)
	{
		return false;
	}

	return true;
}

int32 FStorageSocketConnectionBackend::HandshakeRequest(TArrayView<const TSharedPtr<FInternetAddr>> HostAddresses)
{
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath);

	TArray<TSharedPtr<FInternetAddr>> SortedAddresses;
	SortHostAddressesByLocalSubnet(HostAddresses, SortedAddresses);

	for (const TSharedPtr<FInternetAddr>& Addr : SortedAddresses)
	{
		Hostname.Reset();
		Hostname.Append(TCHAR_TO_ANSI(*Addr->ToString(false)));
		ServerAddr = Addr;

		UE_LOG(LogStorageSocketBackend, Display, TEXT("Trying to handshake with Zen at '%s'"), *Addr->ToString(true));

		// Handshakes are done with a limited connection timeout so that we can find out if the destination is unreachable
		// in a timely manner.
		const float ConnectionTimeoutSeconds = 5.0f;
		IStorageConnectionSocket* ConnectSocket = AcquireNewSocket(ConnectionTimeoutSeconds);
		if (!ConnectSocket)
		{
			continue;
		}
		ReleaseSocket(ConnectSocket, true);

		FStorageServerRequest Request("GET", *ResourceBuilder, Hostname);
		if (IStorageConnectionSocket* Socket = Request.Send(Owner, false))
		{
			FStorageServerResponse Response(Owner, *Socket);

			if (Response.IsOk())
			{
				FCbObject ResponseObj = Response.GetResponseObject();

				// we currently don't have any concept of protocol versioning, if
				// we succeed in communicating with the endpoint we're good since
				// any breaking API change would need to be done in a backward
				// compatible manner

				return 1;
			}
			else
			{
				UE_LOG(LogStorageSocketBackend, Error, TEXT("Failed to handshake with Zen at %s. '%s'"), *ServerAddr->ToString(true), *Response.GetErrorMessage());
			}
		}
		else
		{
			UE_LOG(LogStorageSocketBackend, Warning, TEXT("Failed to send handshake request to Zen at %s."), *ServerAddr->ToString(true));
		}
	}

	UE_LOG(LogStorageSocketBackend, Error, TEXT("Failed to handshake with Zen at any of host addresses."));

	Hostname.Reset();
	ServerAddr.Reset();

	return -1;
}

void FStorageSocketConnectionBackend::SortHostAddressesByLocalSubnet(TArrayView<const TSharedPtr<FInternetAddr>> HostAddresses, TArray<TSharedPtr<FInternetAddr>>& SortedHostAddresses)
{
	//no sorting needed cases
	if (HostAddresses.Num() == 0)
		return;

	if (HostAddresses.Num() == 1)
	{
		SortedHostAddresses.Push(HostAddresses[0]);
		return;
	}

	//Sorting logic:
	//1 only on desktop, if it's an IPV6 address loopback (ends with ":1")
	//2 only on desktop, if it's and IPV4 address loopback (starts with "127.0.0")
	//3 if the host IPV4 subnet match the client subnet (xxx.xxx.xxx)
	//4 remaining addresses
	bool bCanBindAll = false;
	bool bAppendPort = false;
	TSharedPtr<FInternetAddr> localAddr = SocketSubsystem.GetLocalHostAddr(*GLog, bCanBindAll);
	FString localAddrStringSubnet = localAddr->ToString(bAppendPort);

	int32 localLastDotPos = INDEX_NONE;
	if (localAddrStringSubnet.FindLastChar(TEXT('.'), localLastDotPos))
	{
		localAddrStringSubnet = localAddrStringSubnet.LeftChop(localAddrStringSubnet.Len() - localLastDotPos);
	}

	TArray<TSharedPtr<FInternetAddr>> IPV6Loopback;
	TArray<TSharedPtr<FInternetAddr>> IPV4Loopback;
	TArray<TSharedPtr<FInternetAddr>> RegularAddresses;

	for (const TSharedPtr<FInternetAddr>& Addr : HostAddresses)
	{
		FString tempAddrStringSubnet = Addr->ToString(bAppendPort);

#if PLATFORM_DESKTOP || PLATFORM_ANDROID
		if (Addr->GetProtocolType() == FNetworkProtocolTypes::IPv6)
		{
			if (tempAddrStringSubnet.EndsWith(":1"))
			{
				IPV6Loopback.Push(Addr);
				continue;
			}
		}
		else
		{
			if (tempAddrStringSubnet.StartsWith("127.0.0."))
			{
				IPV4Loopback.Push(Addr);
				continue;
			}
		}
#endif
		int32 LastDotPos = INDEX_NONE;
		if (tempAddrStringSubnet.FindLastChar(TEXT('.'), LastDotPos))
		{
			tempAddrStringSubnet = tempAddrStringSubnet.LeftChop(tempAddrStringSubnet.Len() - LastDotPos);
		}

		if (localAddrStringSubnet.Equals(tempAddrStringSubnet))
			RegularAddresses.Insert(Addr, 0);
		else
			RegularAddresses.Push(Addr);
	}


	for (const TSharedPtr<FInternetAddr>& Addrv6lb : IPV6Loopback)
	{
		SortedHostAddresses.Push(Addrv6lb);
	}

	for (const TSharedPtr<FInternetAddr>& Addrv4lb : IPV4Loopback)
	{
		SortedHostAddresses.Push(Addrv4lb);
	}

	for (const TSharedPtr<FInternetAddr>& RegularAddr : RegularAddresses)
	{
		SortedHostAddresses.Push(RegularAddr);
	}
}

#endif // !UE_BUILD_SHIPPING