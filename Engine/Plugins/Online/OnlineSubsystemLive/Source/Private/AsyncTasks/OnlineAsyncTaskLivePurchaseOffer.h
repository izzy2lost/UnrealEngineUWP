// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlinePurchaseInterfaceLive.h"
#include "OnlineError.h"

/**
 * Async task to purchase an offer
 */
class FOnlineAsyncTaskLivePurchaseOffer
	: public FOnlineAsyncTaskConcurrencyLiveAsyncAction
{
public:
	FOnlineAsyncTaskLivePurchaseOffer(FOnlineSubsystemLive* const InLiveInterface, const FString& InSignedOffer, Windows::Xbox::System::User^ InLiveUser, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InNetIdLive, const FOnPurchaseCheckoutComplete& InDelegate);
	virtual ~FOnlineAsyncTaskLivePurchaseOffer() = default;

	// FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLivePurchaseOffer"); }

	// FOnlineAsyncTaskConcurrencyLive Interface
	// Start and Process in Online Thread
	virtual Windows::Foundation::IAsyncAction^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<void>& CompletedTask) override;

	// FOnlineAsyncTask Interface
	// Move results and trigger delegates in Game Thread
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

protected:
	FString SignedOffer;
	FUniqueNetIdLive NetIdLive;
	FOnPurchaseCheckoutComplete Delegate;
	FOnlineError ErrorResult;
};