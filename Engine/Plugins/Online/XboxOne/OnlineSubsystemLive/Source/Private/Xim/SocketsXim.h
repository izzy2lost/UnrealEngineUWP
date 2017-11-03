#pragma once

#include "Sockets.h"
#include "InternetAddrXim.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLivePackage.h"

namespace xbox
{
	namespace services
	{
		namespace xbox_integrated_multiplayer
		{
			class xim_player;
		}
	}
}

class FSocketXim :
	public FSocket
{
private:

	class FOnlineSubsystemLive* LiveSubsystem;

	FDelegateHandle XimPlayerDataReceivedHandle;
	FDelegateHandle XimPlayerLeftHandle;

	void OnXimPlayerDataReceived(xbox::services::xbox_integrated_multiplayer::xim_player* FromPlayer, xbox::services::xbox_integrated_multiplayer::xim_player* ToPlayer, TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data);
	void OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* LeavingPlayer);

PACKAGE_SCOPE:

	struct Datagram
	{
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data;
		FInternetAddrXim Source;
		int32 ReadOffset;
	};

	TQueue<TSharedPtr<Datagram, ESPMode::ThreadSafe>> RecvQueue;
	xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer;

public:
	FSocketXim(class FOnlineSubsystemLive* InSubsystem, const FString& InSocketDescription);

	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination) override;
	virtual bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;

	virtual bool Close() override;
	virtual bool Bind(const FInternetAddr& Addr) override;
	virtual bool Connect(const FInternetAddr &Addr) override;
	virtual bool Listen(int32 MaxBacklog) override;
	virtual bool WaitForPendingConnection(bool &bHasPendingConnection, const FTimespan& WaitTime) override;
	virtual bool HasPendingData(uint32& PendingDataSize) override;
	virtual class FSocket* Accept(const FString& SocketDescription) override;
	virtual class FSocket* Accept(FInternetAddr& OutAddr, const FString& SocketDescription) override;
	virtual bool Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime) override;
	virtual ESocketConnectionState GetConnectionState() override;
	virtual void GetAddress(FInternetAddr& OutAddr) override;
	virtual bool GetPeerAddress(FInternetAddr& OutAddr) override;
	virtual bool SetNonBlocking(bool bIsNonBlocking = true) override;
	virtual bool SetBroadcast(bool bAllowBroadcast = true) override;
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress) override;
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress) override;
	virtual bool SetMulticastLoopback(bool bLoopback) override;
	virtual bool SetMulticastTtl(uint8 TimeToLive) override;
	virtual bool SetReuseAddr(bool bAllowReuse = true) override;
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0) override;
	virtual bool SetRecvErr(bool bUseErrorQueue = true) override;
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize) override;
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize) override;
	virtual int32 GetPortNo() override;
};