// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

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
	// @ATG_CHANGE : BEGIN Adding XIM
	virtual bool InitConnectionClass() override;
	// @ATG_CHANGE : END
};
