// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlinePurchaseInterfaceLive.h"
#include "OnlineError.h"

class FOnlineSubsystemLive;

/**
 * Async item used to marshal store offer results from the Live system thread to the game thread.
 */
class FOnlineAsyncTaskLiveQueryOffers : public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Marketplace::CatalogItemDetails^>^>
{
public:
	FOnlineAsyncTaskLiveQueryOffers(FOnlineSubsystemLive* InLiveSubsystem, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const TArray<FUniqueOfferId>& InOfferIds, const FOnQueryOnlineStoreOffersComplete& InDelegate);

	//~ Begin FOnlineAsyncTaskConcurrencyLive Interface
	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Marketplace::CatalogItemDetails^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Marketplace::CatalogItemDetails^>^>& CompletedTask) override;
	//~ End FOnlineAsyncTaskConcurrencyLive Interface

	//~ Begin FOnlineAsyncTaskConcurrencyLive Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryOffers");}
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

protected:
	TArray<FUniqueOfferId> OfferIds;
	TArray<FOnlineStoreOfferRef> OffersData;
	FOnlineError ErrorResponse;
	FOnQueryOnlineStoreOffersComplete Delegate;
	int32 OfferQueriedIndex;
};
