// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class FOnlineAsyncTaskLiveCancelMatchmaking : public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveCancelMatchmaking(
	class FOnlineSubsystemLive* InLiveSubsystem,
		Microsoft::Xbox::Services::XboxLiveContext^ InUserContext,
		FName InSessionName,
		FOnlineMatchTicketInfoPtr InTicketInfo);

	virtual ~FOnlineAsyncTaskLiveCancelMatchmaking();

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("CancelMatchmaking");}
	virtual void TriggerDelegates() override;


private:
	FName SessionName;
	Microsoft::Xbox::Services::XboxLiveContext^ UserContext;
	float Timeout;
	bool	bCancelInProgress;

	FOnlineMatchTicketInfoPtr TicketInfo;

};

//------------------------------- End of file ---------------------------------
