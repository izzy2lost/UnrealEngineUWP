// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IpConnection.h"

#include "OnlineBeaconUnitTestNetConnection.generated.h"

UCLASS(Transient, Config=Engine)
class UOnlineBeaconUnitTestNetConnection : public UIpConnection
{
	GENERATED_UCLASS_BODY()

public:
//~ Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead) override;
	virtual float GetTimeoutValue() override;
//~ End NetConnection Interface
};
