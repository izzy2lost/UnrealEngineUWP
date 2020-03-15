// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlineAsyncTaskLiveQueryOffers.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineStoreInterfaceLive.h"

#include <collection.h>

using Microsoft::Xbox::Services::XboxLiveContext;
using Microsoft::Xbox::Services::Marketplace::CatalogItemAvailability;
using Microsoft::Xbox::Services::Marketplace::CatalogItemDetails;
using Platform::Collections::Vector;
using Windows::Foundation::Collections::IVector;
using Windows::Foundation::Collections::IVectorView;

// Maximum of item-details we may request at once; this would ideally be exposed by their API but isn't?
const int32 MaxDetailItemRequestSize = 10;

#define DEBUG_LOG_LIVE_STORE_ITEMS 0

FOnlineAsyncTaskLiveQueryOffers::FOnlineAsyncTaskLiveQueryOffers(FOnlineSubsystemLive* InLiveSubsystem, XboxLiveContext^ InLiveContext, const TArray<FUniqueOfferId>& InOfferIds, const FOnQueryOnlineStoreOffersComplete& InDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, OfferIds(InOfferIds)
	, Delegate(InDelegate)
	, OfferQueriedIndex(0)
{
	check(Subsystem);
	OffersData.Empty(OfferIds.Num());
}

Windows::Foundation::IAsyncOperation<IVectorView<CatalogItemDetails^>^>^ FOnlineAsyncTaskLiveQueryOffers::CreateOperation()
{
	IVector<Platform::String^>^ OfferIdVector = ref new Vector<Platform::String^>();
	for (int32 i = 0;
		i < OfferIds.Num() && OfferIdVector->Size < MaxDetailItemRequestSize;
		++i, ++OfferQueriedIndex)
	{
		OfferIdVector->Append(ref new Platform::String(*(OfferIds[i])));
	}

	try
	{
		// @ATG_CHANGE : BEGIN - UWP Store support
		auto AsyncOp = GetCatalogService(LiveContext)->GetCatalogItemDetailsAsync(OfferIdVector->GetView());
		// @ATG_CHANGE : END
		return AsyncOp;
	}
	catch (Platform::Exception^ Ex)
	{
		ErrorResponse.bSucceeded = false;
		ErrorResponse.SetFromErrorMessage(FText::FromString(FString::Printf(TEXT("Error querying store offers, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data())));
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryOffers::ProcessResult(const Concurrency::task<IVectorView<CatalogItemDetails^>^>& CompletedTask)
{
	IVectorView<CatalogItemDetails^>^ ResultsVector = nullptr;

	try
	{
		ResultsVector = CompletedTask.get();
		
		ErrorResponse.bSucceeded = true;
	}
	catch (Platform::Exception^ Ex)
	{
		FString ErrorMessage = FString::Printf(TEXT("FOnlineStoreLive::QueryOffersById failed with code 0x%0.8X."), Ex->HResult);
		ErrorResponse.SetFromErrorMessage(FText::FromString(ErrorMessage));
		ErrorResponse.bSucceeded = false;
	}

	while (ResultsVector != nullptr)
	{
		for (CatalogItemDetails^ XboxCatalogItem : ResultsVector)
		{
#if DEBUG_LOG_LIVE_STORE_ITEMS
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("Id"),					XboxCatalogItem->Id->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("Name"),					XboxCatalogItem->Name->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("ReducedName"),			XboxCatalogItem->ReducedName->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("Description"),			XboxCatalogItem->Description->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %d"), TEXT("TitleId"),				XboxCatalogItem->TitleId);
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %lld"), TEXT("ReleaseDate"),			XboxCatalogItem->ReleaseDate.UniversalTime);
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("ProductId"),				XboxCatalogItem->ProductId->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %ls"), TEXT("SandboxId"),				XboxCatalogItem->SandboxId->Data());
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %s"), TEXT("IsBundle"),				XboxCatalogItem->IsBundle ? TEXT("True") : TEXT("False"));
			UE_LOG_ONLINE(Log, TEXT("CatalogItemDetails: %s = %s"), TEXT("IsPartOfAnyBundle"),		XboxCatalogItem->IsPartOfAnyBundle ? TEXT("True") : TEXT("False"));
#endif

			if (XboxCatalogItem->Availabilities->Size > 0)
			{
				CatalogItemAvailability^ XboxCatalogItemPriceInfo = XboxCatalogItem->Availabilities->GetAt(0);

#if DEBUG_LOG_LIVE_STORE_ITEMS
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("ContentId"),					XboxCatalogItemPriceInfo->ContentId->Data());
				for (Platform::String^ AcceptPaymentType : XboxCatalogItemPriceInfo->AcceptablePaymentInstrumentTypes)
				{
					UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("AcceptablePaymentType[]"),	AcceptPaymentType->Data());
				}
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("AvailabilityTitle"),			XboxCatalogItemPriceInfo->AvailabilityTitle->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("AvailabilityDescription"),		XboxCatalogItemPriceInfo->AvailabilityDescription->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("CurrencyCode"),					XboxCatalogItemPriceInfo->CurrencyCode->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("DisplayPrice"),					XboxCatalogItemPriceInfo->DisplayPrice->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("DisplayListPrice"),				XboxCatalogItemPriceInfo->DisplayListPrice->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("DistributionType"),				XboxCatalogItemPriceInfo->DistributionType->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %s"), TEXT("IsPurchasable"),					XboxCatalogItemPriceInfo->IsPurchasable ? TEXT("True") : TEXT("False"));
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %f"), TEXT("Price"),							XboxCatalogItemPriceInfo->Price);
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %f"), TEXT("ListPrice"),						XboxCatalogItemPriceInfo->ListPrice);
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %d"), TEXT("ConsumableQuantity"),			XboxCatalogItemPriceInfo->ConsumableQuantity);
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("PromotionalText"),				XboxCatalogItemPriceInfo->PromotionalText->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("SignedOffer"),					XboxCatalogItemPriceInfo->SignedOffer->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("OfferId"),						XboxCatalogItemPriceInfo->OfferId->Data());
				UE_LOG_ONLINE(Log, TEXT("CatalogItemAvailability: %s = %ls"), TEXT("OfferDisplayDataJson"),			XboxCatalogItemPriceInfo->OfferDisplayDataJson->Data());
#endif

				FOnlineStoreOfferLiveRef NewOffer = MakeShared<FOnlineStoreOfferLive>();

				// Set Offer Id
				NewOffer->OfferId = FString(XboxCatalogItem->ProductId->Data());

				// Set display text fields
				NewOffer->Title = FText::FromString(FString(XboxCatalogItem->Name->Data()));
				NewOffer->Description = FText::FromString(FString(XboxCatalogItem->Description->Data()));
				NewOffer->LongDescription = NewOffer->Description;

				// Save the SignedOffer, we need to pass this to the purchase call as the "id"
				NewOffer->SignedOffer = FString(XboxCatalogItemPriceInfo->SignedOffer->Data());

				// Non-discount prices
				NewOffer->RegularPriceText = FText::FromString(FString(XboxCatalogItemPriceInfo->DisplayListPrice->Data()));
				NewOffer->RegularPrice = XboxCatalogItemPriceInfo->ListPrice;

				// Current purchase price
				NewOffer->PriceText = FText::FromString(FString(XboxCatalogItemPriceInfo->DisplayPrice->Data()));
				NewOffer->NumericPrice = XboxCatalogItemPriceInfo->Price;

				// Price currency code
				NewOffer->CurrencyCode = FString(XboxCatalogItemPriceInfo->CurrencyCode->Data());

				// Save out our new price
				OffersData.Emplace(MoveTemp(NewOffer));
			}
		}

		ResultsVector = nullptr;

		const bool bHasMoreToQuery = OfferQueriedIndex < OfferIds.Num();
		if (bHasMoreToQuery)
		{
			try
			{
				IVector<Platform::String^>^ OfferIdVector = ref new Vector<Platform::String^>();
				for (int32 i = OfferQueriedIndex;
					i < OfferIds.Num() && OfferIdVector->Size < MaxDetailItemRequestSize;
					++i, ++OfferQueriedIndex)
				{
					OfferIdVector->Append(ref new Platform::String(*(OfferIds[i])));
				}

				// Query our next page
				// @ATG_CHANGE : BEGIN - UWP Store support
				ResultsVector = Concurrency::create_task(GetCatalogService(LiveContext)->GetCatalogItemDetailsAsync(OfferIdVector->GetView())).get();
				// @ATG_CHANGE : END
			}
			catch (Platform::Exception^ Ex)
			{
				FString ErrorMessage = FString::Printf(TEXT("FOnlineStoreLive::QueryOffersById failed with code 0x%0.8X."), Ex->HResult);
				ErrorResponse.SetFromErrorMessage(FText::FromString(ErrorMessage));
				ErrorResponse.bSucceeded = false;
			}
		}
	}

	return ErrorResponse.bSucceeded;
}

void FOnlineAsyncTaskLiveQueryOffers::Finalize()
{
	if (ErrorResponse.bSucceeded)
	{
		FOnlineStoreLivePtr StoreInt = Subsystem->GetStoreLive();
		if (StoreInt.IsValid())
		{
			StoreInt->CachedOffers.Reserve(StoreInt->CachedOffers.Num() + OffersData.Num());
			for (FOnlineStoreOfferRef& StoreOffer : OffersData)
			{
				StoreInt->CachedOffers.Emplace(StoreOffer->OfferId, StoreOffer);
			}
		}
	}
}

void FOnlineAsyncTaskLiveQueryOffers::TriggerDelegates()
{
	TArray<FUniqueOfferId> NewOfferIds;
	NewOfferIds.Reserve(OffersData.Num());
	for (const FOnlineStoreOfferRef& Offer : OffersData)
	{
		NewOfferIds.AddUnique(Offer->OfferId);
	}

	Delegate.ExecuteIfBound(ErrorResponse.bSucceeded, NewOfferIds, ErrorResponse.ErrorMessage.ToString());
}

#undef DEBUG_LOG_LIVE_STORE_ITEMS

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END