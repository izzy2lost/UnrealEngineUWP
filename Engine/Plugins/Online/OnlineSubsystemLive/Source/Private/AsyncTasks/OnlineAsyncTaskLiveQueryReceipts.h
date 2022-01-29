// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlinePurchaseInterfaceLive.h"
#include "OnlineError.h"

class FOnlineSubsystemLive;

/**
 * Async item used to query a specific user's receipts
 */
class FOnlineAsyncTaskLiveQueryReceipts
	: public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveQueryReceipts(FOnlineSubsystemLive* const InLiveInterface, const FUniqueNetIdLive& InUserId, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const bool bInReceiptsExpected, const FOnQueryReceiptsComplete& InQueryReceiptsDelegate);
	FOnlineAsyncTaskLiveQueryReceipts(FOnlineSubsystemLive* const InLiveInterface, const FUniqueNetIdLive& InUserId, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const bool bInReceiptsExpected, const FOnPurchaseCheckoutComplete& InPurchaseCheckoutDelegate);

	virtual ~FOnlineAsyncTaskLiveQueryReceipts();

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryReceipts");}
	
	//~ Called on Game Thread
	void Initialize();
	//~ Called on Online Thread
	void Tick();

	//~ Called on Game Thread
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	void FailTask(const bool bSoftFail);
	void FailTask(const FText ErrorMessage, const bool bSoftFail);
	void FailTaskCode(const int32 ErrorCode, const bool bSoftFail);

private:
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;
	FUniqueNetIdLive UserId;
	const bool bReceiptsExpected;

	/** If true, instead of hard-failing on request failures, return success with an error message; this will replace our cached items with partial results */
	bool bSoftFailReceiptFailures;

	Concurrency::task<Windows::Xbox::System::GetTokenAndSignatureResult^> XSTSTokenTask;
	TArray<Concurrency::task<Microsoft::Xbox::Services::Marketplace::InventoryItemsResult^>> InventoryTaskList;

	TArray<FPurchaseReceipt> ReceiptData;
	FOnlineError ErrorResponse;

	const FOnQueryReceiptsComplete QueryReceiptsDelegate;
	const FOnPurchaseCheckoutComplete PurchaseCheckoutDelegate;
};