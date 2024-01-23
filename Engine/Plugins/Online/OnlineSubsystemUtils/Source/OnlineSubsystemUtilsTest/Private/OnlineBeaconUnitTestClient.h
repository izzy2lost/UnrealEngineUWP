// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconClient.h"

#include "OnlineBeaconUnitTestClient.generated.h"

UCLASS(transient, notplaceable)
class AOnlineBeaconUnitTestClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	//~ Begin AOnlineBeaconClient Interface
	virtual void OnConnected() override;
	//~ End AOnlineBeaconClient Interface

	//~ Begin OnlineBeacon Interface
	virtual void OnFailure() override;
	//~ End OnlineBeacon Interface
};
