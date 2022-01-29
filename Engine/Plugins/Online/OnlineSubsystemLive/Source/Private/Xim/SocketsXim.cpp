#include "../OnlineSubsystemLivePrivatePCH.h"

#include "SocketsXim.h"
#include "SocketSubsystemXim.h"
#include "XimMessageRouter.h"

#if USE_XIM
#include <xboxintegratedmultiplayer.h>

using namespace xbox::services::xbox_integrated_multiplayer;

FSocketXim::FSocketXim(FOnlineSubsystemLive* InSubsystem, const FString& InSocketDescription) :
	FSocket(SOCKTYPE_Datagram, InSocketDescription),
	LiveSubsystem(InSubsystem)
{
}

void FSocketXim::OnXimPlayerDataReceived(xbox::services::xbox_integrated_multiplayer::xim_player* FromPlayer, xbox::services::xbox_integrated_multiplayer::xim_player* ToPlayer, TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data)
{
	if (XimPlayer == ToPlayer)
	{
		TSharedPtr<FSocketXim::Datagram, ESPMode::ThreadSafe> ReceivedMessage = MakeShareable(new FSocketXim::Datagram());
		bool ValidSourceAddress;
		ReceivedMessage->Data = Data;
		ReceivedMessage->Source.SetIp(FromPlayer->xbox_user_id(), ValidSourceAddress);
		ReceivedMessage->ReadOffset = 0;
		RecvQueue.Enqueue(ReceivedMessage);
	}
}

void FSocketXim::OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* LeavingPlayer)
{
	if (LeavingPlayer == XimPlayer)
	{
		Close();
	}
}

bool FSocketXim::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	FXimMessageRouterPtr MessageRouter = LiveSubsystem->GetXimMessageRouter();
	xim_player* TargetPlayer = MessageRouter->GetXimPlayerForNetId(static_cast<const FInternetAddrXim&>(Destination).PlayerId);
	if (!XimPlayer || !TargetPlayer)
	{
		return false;
	}
	check(XimPlayer->local());
	XimPlayer->local()->send_data_to_other_players(Count, Data, 1, &TargetPlayer, xim_send_type::best_effort_and_nonsequential);
	BytesSent = Count;
	return true;
}

bool FSocketXim::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	if ((Flags & ESocketReceiveFlags::WaitAll) != 0)
	{
		return false;
	}

	bool bSuccess = true;
	FInternetAddrXim& XimAddr = static_cast<FInternetAddrXim&>(Source);

	TSharedPtr<Datagram, ESPMode::ThreadSafe> FirstMessage;
	if (!RecvQueue.Peek(FirstMessage))
	{
		BytesRead = 0;
		return false;
	}

	int32 BytesToRead = FMath::Min(BufferSize, FirstMessage->Data->Num() - FirstMessage->ReadOffset);
	FMemory::Memcpy(Data, FirstMessage->Data->GetData(), BytesToRead);
	XimAddr.PlayerId = FirstMessage->Source.PlayerId;
	if ((Flags & ESocketReceiveFlags::Peek) == 0)
	{
		FirstMessage->ReadOffset += BytesToRead; 
		if (FirstMessage->ReadOffset >= FirstMessage->Data->Num())
		{
			RecvQueue.Dequeue(FirstMessage);
		}
	}

	BytesRead = BytesToRead;

	return true;
}

bool FSocketXim::Close()
{
	FXimMessageRouterPtr MessageRouter = LiveSubsystem->GetXimMessageRouter();
	if (XimPlayerDataReceivedHandle.IsValid())
	{
		MessageRouter->ClearOnXimPlayerDataReceivedDelegate_Handle(XimPlayerDataReceivedHandle);
	}
	if (XimPlayerLeftHandle.IsValid())
	{
		MessageRouter->ClearOnXimPlayerLeftDelegate_Handle(XimPlayerLeftHandle);
	}
	XimPlayer = nullptr;
	return true;
}

bool FSocketXim::Bind(const FInternetAddr& Addr)
{
	const FInternetAddrXim& XimAddr = static_cast<const FInternetAddrXim&>(Addr);
	FXimMessageRouterPtr MessageRouter = LiveSubsystem->GetXimMessageRouter();
	XimPlayer = MessageRouter->GetXimPlayerForNetId(XimAddr.PlayerId);
	if (XimPlayer != nullptr)
	{
		check(XimPlayer->local());
		XimPlayerDataReceivedHandle = MessageRouter->AddOnXimPlayerDataReceivedDelegate_Handle(FOnXimPlayerDataReceivedDelegate::CreateRaw(this, &FSocketXim::OnXimPlayerDataReceived));
		XimPlayerLeftHandle = MessageRouter->AddOnXimPlayerLeftDelegate_Handle(FOnXimPlayerLeftDelegate::CreateRaw(this, &FSocketXim::OnXimPlayerLeft));
	}
	return XimPlayer != nullptr;
}

bool FSocketXim::Connect(const FInternetAddr& Addr)
{
	return false;
}

bool FSocketXim::Listen(int32 MaxBacklog)
{
	return false;
}

bool FSocketXim::WaitForPendingConnection(bool &bHasPendingConnection, const FTimespan& WaitTime)
{
	return false;
}

bool FSocketXim::HasPendingData(uint32& PendingDataSize)
{
	TSharedPtr<Datagram, ESPMode::ThreadSafe> FirstMessage;
	if (!RecvQueue.Peek(FirstMessage))
	{
		return false;
	}

	PendingDataSize = FirstMessage->Data->Num() - FirstMessage->ReadOffset;
	return PendingDataSize > 0;
}

FSocket* FSocketXim::Accept(const FString& SocketDescription)
{
	return nullptr;
}

FSocket* FSocketXim::Accept(FInternetAddr& OutAddr, const FString& SocketDescription)
{
	return nullptr;
}

bool FSocketXim::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	return false;
}

ESocketConnectionState FSocketXim::GetConnectionState()
{
	return SCS_NotConnected;
}

void FSocketXim::GetAddress(FInternetAddr& OutAddr)
{
	bool ValidAddress = false;
	if (XimPlayer)
	{
		OutAddr.SetIp(XimPlayer->xbox_user_id(), ValidAddress);
	}
}

bool FSocketXim::GetPeerAddress(FInternetAddr& OutAddr)
{
	return false;
}

bool FSocketXim::SetNonBlocking(bool bIsNonBlocking)
{
	return true;
}

bool FSocketXim::SetBroadcast(bool bAllowBroadcast)
{
	return true;
}

bool FSocketXim::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	return false;
}

bool FSocketXim::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	return false;
}

bool FSocketXim::SetMulticastLoopback(bool bLoopback)
{
	return false;
}

bool FSocketXim::SetMulticastTtl(uint8 TimeToLive)
{
	return false;
}

bool FSocketXim::SetReuseAddr(bool bAllowReuse)
{
	return true;
}

bool FSocketXim::SetLinger(bool bShouldLinger, int32 Timeout)
{
	return true;
}

bool FSocketXim::SetRecvErr(bool bUseErrorQueue)
{
	return true;
}

bool FSocketXim::SetSendBufferSize(int32 Size, int32& NewSize)
{
	return true;
}

bool FSocketXim::SetReceiveBufferSize(int32 Size, int32& NewSize)
{
	return true;
}

int32 FSocketXim::GetPortNo()
{
	return 0;
}
#endif
