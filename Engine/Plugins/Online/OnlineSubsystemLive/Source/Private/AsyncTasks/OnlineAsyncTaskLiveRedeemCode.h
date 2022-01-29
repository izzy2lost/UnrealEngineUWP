// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlinePurchaseInterfaceLive.h"
#include "OnlineError.h"

/**
 * Async task to redeem a code
 */
class FOnlineAsyncTaskLiveRedeemCode
	: public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveRedeemCode(FOnlineSubsystemLive* const InLiveInterface, Windows::Xbox::System::User^ InLiveUser, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InNetIdLive, const FOnPurchaseRedeemCodeComplete& InDelegate);
	virtual ~FOnlineAsyncTaskLiveRedeemCode();

	// FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveRedeemCode"); }

	// FOnlineAsyncItemBasic
	virtual void Initialize() override;

	// FOnlineAsyncTask Interface
	// Move results and trigger delegates in Game Thread
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	void OnApplicationReactivated();

protected:
	Windows::Xbox::System::User^ LocalUser;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;
	FUniqueNetIdLive NetIdLive;
	FOnPurchaseRedeemCodeComplete Delegate;
	FOnlineError ErrorResult;
};