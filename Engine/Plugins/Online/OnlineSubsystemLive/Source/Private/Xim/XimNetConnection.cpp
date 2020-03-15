#include "../OnlineSubsystemLivePrivatePCH.h"

#include "XimNetConnection.h"
#include "InternetAddrXim.h"
#include "XimMessageRouter.h"
#include "OnlineSubsystemLive.h"

#if USE_XIM
#include <xboxintegratedmultiplayer.h>
#endif

UXimNetConnection::UXimNetConnection(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	SendQueueBytesAtAlert(UINT_MAX),
	RemotePlayer(nullptr),
	SendQueueAlertTime(0.0f),
	SaturateResetTime(0.0f)
{
}

void UXimNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

#if USE_XIM
	FOnlineSubsystemLive* LiveSubsystem = static_cast<FOnlineSubsystemLive*>(IOnlineSubsystem::Get(LIVE_SUBSYSTEM));
	XimSendQueueAlertHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimSendQueueAlertDelegate_Handle(FOnXimSendQueueAlertDelegate::CreateUObject(this, &UXimNetConnection::OnXimSendQueueAlert));
	XimPlayerLeftHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerLeftDelegate_Handle(FOnXimPlayerLeftDelegate::CreateUObject(this, &UXimNetConnection::OnXimPlayerLeft));
#endif
}

void UXimNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	InitRemotePlayer();
}

void UXimNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
	InitRemotePlayer();
}

void UXimNetConnection::InitRemotePlayer()
{
#if USE_XIM
	if (RemoteAddr.IsValid())
	{
		FOnlineSubsystemLive* LiveSubsystem = static_cast<FOnlineSubsystemLive*>(IOnlineSubsystem::Get(LIVE_SUBSYSTEM));
		RemotePlayer = LiveSubsystem->GetXimMessageRouter()->GetXimPlayerForNetId(StaticCastSharedPtr<FInternetAddrXim>(RemoteAddr)->PlayerId);
	}
#endif
}

void UXimNetConnection::OnXimSendQueueAlert(xbox::services::xbox_integrated_multiplayer::xim_player* TargetPlayer)
{
#if USE_XIM
	using namespace xbox::services::xbox_integrated_multiplayer;

	if (RemotePlayer == TargetPlayer)
	{
		const xim_network_path_information* NetInfo = RemotePlayer->network_path_information();
		SendQueueBytesAtAlert = FMath::Min(SendQueueBytesAtAlert, NetInfo->send_queue_size_in_bytes);

		// Regardless of whether or not we updated our size limit estimate we know that the send queue 
		// is still under pressure
		SendQueueAlertTime = FPlatformTime::Seconds();
	}
#endif
}

void UXimNetConnection::OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer)
{
#if USE_XIM
	if (RemotePlayer == XimPlayer)
	{
		RemotePlayer = nullptr;
		Close();
	}
#endif
}

int32 UXimNetConnection::IsNetReady(bool Saturate)
{
	// Check that total of bytes queued within XIM and bytes queued by UE is below the
	// level that we know is problematic right now.
	//uint32 SendQueueBytes = RemotePlayer ? RemotePlayer->network_path_information()->send_queue_size_in_bytes : 0;
	uint32 SendQueueBytes = 0;
	return (SendQueueBytes + SendBuffer.GetNumBytes() < SendQueueBytesAtAlert) ? 1 : 0;
}

void UXimNetConnection::Tick()
{
	using namespace xbox::services::xbox_integrated_multiplayer;

	Super::Tick();

#if USE_XIM
	// Check to see whether it's been long enough since the last alert that
	// we can safely relax our saturation checks.
	if (FPlatformTime::Seconds() - SendQueueAlertTime > SaturateResetTime)
	{
		SendQueueBytesAtAlert = UINT_MAX;
	}
#endif
}
