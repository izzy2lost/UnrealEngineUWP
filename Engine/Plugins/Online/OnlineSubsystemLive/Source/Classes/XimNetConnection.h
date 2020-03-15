#pragma once

#include "IpConnection.h"
#include "XimNetConnection.generated.h"

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

UCLASS( transient, config=Engine )
class UXimNetConnection : public UIpConnection
{
	GENERATED_UCLASS_BODY()

	/** Amount of time that must pass without a XIM send queue alert before the connection relaxes its saturation check */
	UPROPERTY(Config)
	float SaturateResetTime;

private:
	FDelegateHandle XimPlayerLeftHandle;
	FDelegateHandle XimSendQueueAlertHandle;
	uint32 SendQueueBytesAtAlert;
	float SendQueueAlertTime;
	xbox::services::xbox_integrated_multiplayer::xim_player* RemotePlayer;

	void InitRemotePlayer();
	void OnXimSendQueueAlert(xbox::services::xbox_integrated_multiplayer::xim_player* TargetPlayer);
	void OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);

public:
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;

	virtual int32 IsNetReady(bool Saturate) override;

	virtual void Tick() override;
};
