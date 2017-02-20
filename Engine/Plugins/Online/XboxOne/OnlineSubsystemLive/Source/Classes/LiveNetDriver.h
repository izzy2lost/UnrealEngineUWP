// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//
// Xbox One based implementation of the net driver
//

#pragma once
#include "IpNetDriver.h"
#include "LiveNetDriver.generated.h"


UCLASS( transient, config=Engine )
class ULiveNetDriver : public UIpNetDriver
{
	GENERATED_UCLASS_BODY()

	virtual int GetClientPort() override;
};
