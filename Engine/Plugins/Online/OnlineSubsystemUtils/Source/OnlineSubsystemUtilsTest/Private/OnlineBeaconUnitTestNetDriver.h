// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IpNetDriver.h"

#include "OnlineBeaconUnitTestNetDriver.generated.h"

UCLASS(Transient)
class UOnlineBeaconUnitTestNetDriver : public UIpNetDriver
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UObject Interface.
	virtual void PostInitProperties() override;
	//~ End UObject Interface.

	//~ Begin UNetDriver Interface.
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool IsEncryptionRequired() const override;
	//~ End UNetDriver Interface
};
