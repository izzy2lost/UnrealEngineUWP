// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemLivePrivatePCH.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"

class FOnlineSubsystemLive;

class FOnlineStoreOfferLive
	: public FOnlineStoreOffer
{
public:
	/** Signed value to pass to XBox when purchasing this offer */
	FString SignedOffer;
};

using FOnlineStoreOfferLiveRef = TSharedRef<FOnlineStoreOfferLive>;

class FOnlineStoreLive
	: public IOnlineStoreV2
	, public TSharedFromThis<FOnlineStoreLive, ESPMode::ThreadSafe>
{
public:
	FOnlineStoreLive(FOnlineSubsystemLive* InLiveSubsystem);
	virtual ~FOnlineStoreLive();

public:
	//~ Begin IOnlineStoreV2 Interface
	virtual void QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate = FOnQueryOnlineStoreCategoriesComplete()) override;
	virtual void GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const override;
	virtual void QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate = FOnQueryOnlineStoreOffersComplete()) override;
	virtual void QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate = FOnQueryOnlineStoreOffersComplete()) override;
	virtual void GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const override;
	virtual TSharedPtr<FOnlineStoreOffer> GetOffer(const FUniqueOfferId& OfferId) const override;
	//~ End IOnlineStoreV2 Interface

PACKAGE_SCOPE:
	FOnlineSubsystemLive* LiveSubsystem;

	TMap<FUniqueOfferId, FOnlineStoreOfferRef> CachedOffers;
};

typedef TSharedPtr<FOnlineStoreLive, ESPMode::ThreadSafe> FOnlineStoreLivePtr;
