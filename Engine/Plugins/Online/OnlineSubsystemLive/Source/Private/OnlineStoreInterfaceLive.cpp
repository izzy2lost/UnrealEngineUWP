// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlineStoreInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineError.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveQueryOffers.h"

// @ATG_CHANGE : UWP Live Support - Xbox headers to pch

FOnlineStoreLive::FOnlineStoreLive(FOnlineSubsystemLive* InLiveSubsystem)
	: LiveSubsystem(InLiveSubsystem)
{
	check(LiveSubsystem);
}

FOnlineStoreLive::~FOnlineStoreLive()
{
}

void FOnlineStoreLive::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate /*= FOnQueryOnlineStoreCategoriesComplete()*/)
{
	LiveSubsystem->ExecuteNextTick([Delegate]()
	{
		Delegate.ExecuteIfBound(false, TEXT("FOnlineStoreLive::QueryCategories Not Implemented"));
	});
}

void FOnlineStoreLive::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories.Empty();
}

void FOnlineStoreLive::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate /*= FOnQueryOnlineStoreOffersComplete()*/)
{
	LiveSubsystem->ExecuteNextTick([Delegate]()
	{
		constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(bWasSuccessful, TArray<FUniqueOfferId>(), FString(TEXT("QueryOffersByFilter not implemented")));
	});
}

void FOnlineStoreLive::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate /*= FOnQueryOnlineStoreOffersComplete()*/)
{
	if (OfferIds.Num() < 1)
	{
		constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(bWasSuccessful, TArray<FUniqueOfferId>(), TEXT("No OfferIds requested"));
		return;
	}

	Microsoft::Xbox::Services::XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(UserId);
	if (UserLiveContext == nullptr)
	{
		constexpr bool bWasSuccessful = false;
		Delegate.ExecuteIfBound(bWasSuccessful, TArray<FUniqueOfferId>(), TEXT("Could not map requested user to a LiveContext"));
		return;
	}

	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryOffers>(LiveSubsystem, UserLiveContext, OfferIds, Delegate);
}

void FOnlineStoreLive::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	CachedOffers.GenerateValueArray(OutOffers);
}

TSharedPtr<FOnlineStoreOffer> FOnlineStoreLive::GetOffer(const FUniqueOfferId& OfferId) const
{
	const FOnlineStoreOfferRef* FoundOfferPtr = CachedOffers.Find(OfferId);
	if (FoundOfferPtr == nullptr)
	{
		return nullptr;
	}

	return *FoundOfferPtr;
}

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END