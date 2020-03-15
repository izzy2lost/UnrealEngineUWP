// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemLivePrivatePCH.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "OnlineSubsystemLiveTypes.h"

class FOnlineSubsystemLive;

class FOnlinePurchaseLive
	: public IOnlinePurchase
	, public TSharedFromThis<FOnlinePurchaseLive, ESPMode::ThreadSafe>
{
public:
	FOnlinePurchaseLive(FOnlineSubsystemLive* InLiveSubsystem);
	virtual ~FOnlinePurchaseLive();

public:
	//~ Begin IOnlinePurchase Interface
	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override;
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
	virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
	virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
	//~ End IOnlinePurchase Interface

PACKAGE_SCOPE:
	void RegisterLivePurchaseHooks();
	void UnregisterLivePurchaseHooks();

PACKAGE_SCOPE:
	/** Pointer back to our parent subsystem */
	FOnlineSubsystemLive* LiveSubsystem;
	/** Cached receipts/inventory per user */
	TMap<FUniqueNetIdLive, TArray<FPurchaseReceipt> > UserCachedReceipts;
	/** Endpoint to pass during XSTS generation for claiming inventory items */
	FString ReceiptsXSTSEndpoint;
	/** Is there a purchase in progress currently? */
	bool bIsCurrentyInCheckout;
	/** Soft-Fail Receipt-Checks (return successful but with an Error Message/Code) if any entitlement succeeds */
	bool bSoftFailReceiptFailures;

private:
	/** Delegate handle for the "User Purchased Something" delegate */
	Windows::Foundation::EventRegistrationToken LiveEventProductPurchaseHandle;
};

typedef TSharedPtr<FOnlinePurchaseLive, ESPMode::ThreadSafe> FOnlinePurchaseLivePtr;
