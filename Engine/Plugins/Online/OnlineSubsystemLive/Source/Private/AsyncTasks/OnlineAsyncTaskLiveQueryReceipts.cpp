// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlineAsyncTaskLiveQueryReceipts.h"
#include "OnlineSubsystemLive.h"
#include "../OnlinePurchaseInterfaceLive.h"
#include "Misc/Guid.h"

// @ATG_CHANGE : UWP LIVE support - moved platform specific includes pch

#include <initializer_list>

const constexpr int32 INVENTORY_PAGE_SIZE = 32;

using Microsoft::Xbox::Services::Marketplace::InventoryItem;
using Microsoft::Xbox::Services::Marketplace::InventoryItemsResult;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::IAsyncOperation;
using Windows::Xbox::System::GetTokenAndSignatureResult;
using Windows::Xbox::System::User;

FOnlineAsyncTaskLiveQueryReceipts::FOnlineAsyncTaskLiveQueryReceipts(FOnlineSubsystemLive* const InLiveInterface, const FUniqueNetIdLive& InUserId, XboxLiveContext^ InLiveContext, const bool bInReceiptsExpected, const FOnQueryReceiptsComplete& InQueryReceiptsDelegate)
	: FOnlineAsyncTaskLive(InLiveInterface)
	, LiveContext(InLiveContext)
	, UserId(InUserId)
	, bReceiptsExpected(bInReceiptsExpected)
	, bSoftFailReceiptFailures(false)
	, ErrorResponse(true)
	, QueryReceiptsDelegate(InQueryReceiptsDelegate)
{
	check(LiveContext);
}

FOnlineAsyncTaskLiveQueryReceipts::FOnlineAsyncTaskLiveQueryReceipts(FOnlineSubsystemLive* const InLiveInterface, const FUniqueNetIdLive& InUserId, XboxLiveContext^ InLiveContext, const bool bInReceiptsExpected, const FOnPurchaseCheckoutComplete& InPurchaseCheckoutDelegate)
	: FOnlineAsyncTaskLive(InLiveInterface)
	, LiveContext(InLiveContext)
	, UserId(InUserId)
	, bReceiptsExpected(bInReceiptsExpected)
	, bSoftFailReceiptFailures(false)
	, ErrorResponse(true)
	, PurchaseCheckoutDelegate(InPurchaseCheckoutDelegate)
{
	check(LiveContext);
}

FOnlineAsyncTaskLiveQueryReceipts::~FOnlineAsyncTaskLiveQueryReceipts()
{
	if (!bWasSuccessful)
	{
		// Need to finish retrieving all our tasks, or they'll be called in the destructor and (potentially) crash the app
		try
		{
			XSTSTokenTask.get();
		}
		catch (const std::exception&)
		{
		}
		catch (Platform::Exception^)
		{
		}

		for (const Concurrency::task<InventoryItemsResult^>& InventoryTask : InventoryTaskList)
		{
			try
			{
				InventoryTask.get();
			}
			catch (const std::exception&)
			{
			}
			catch (Platform::Exception^)
			{
			}
		}
	}
}

void FOnlineAsyncTaskLiveQueryReceipts::Initialize()
{
	FOnlinePurchaseLivePtr PurchaseInt = Subsystem->GetPurchaseLive();
	if (!PurchaseInt.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts unable to get purchase interface."));
		FailTask(false /* Hard-fail on missing PurchaseInt (not possible?) */);
		return;
	}

	try
	{
		XSTSTokenTask = concurrency::create_task(LiveContext->User->GetTokenAndSignatureAsync(
			ref new Platform::String(L"GET"),
			ref new Platform::String(*PurchaseInt->ReceiptsXSTSEndpoint),
			ref new Platform::String()));
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetTokenAndSignatureAsync operation failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
		FailTask(false /* Hard-fail on task starts*/);
		return;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetTokenAndSignatureAsync operation failed with code 0x%0.8X."), Ex->HResult);
		FailTaskCode(Ex->HResult, false /* Hard-fail on task starts*/);
		return;
	}

	try
	{
		// Get all durable entitlements
		// @ATG_CHANGE : BEGIN UWP Store support
		InventoryTaskList.Add(concurrency::create_task(GetInventoryService(LiveContext)->GetInventoryItemsForAllUsersAsync(
			Microsoft::Xbox::Services::Marketplace::MediaItemType::GameContent,
			/*bExpandSatisfyingEntitlements*/ true,
			/*bIncludeAllItemStatesAndAvailabilities*/ true)));

		// Get all bundles/season-pass entitlements
		InventoryTaskList.Add(concurrency::create_task(GetInventoryService(LiveContext)->GetInventoryItemsForAllUsersAsync(
			Microsoft::Xbox::Services::Marketplace::MediaItemType::GameV2,
			/*bExpandSatisfyingEntitlements*/ true,
			/*bIncludeAllItemStatesAndAvailabilities*/ true)));

		// Get all consumable entitlements
		InventoryTaskList.Add(concurrency::create_task(GetInventoryService(LiveContext)->GetInventoryItemsAsync(
			Microsoft::Xbox::Services::Marketplace::MediaItemType::GameConsumable,
			/*bExpandSatisfyingEntitlements*/ false,
			/*bIncludeAllItemStatesAndAvailabilities*/ true)));
		// @ATG_CHANGE : END
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
		FailTask(false /* Hard-fail on task starts*/);
		return;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with code 0x%0.8X."), Ex->HResult);
		FailTaskCode(Ex->HResult, false /* Hard-fail on task starts*/);
		return;
	}

	bSoftFailReceiptFailures = PurchaseInt->bSoftFailReceiptFailures;
}

void FOnlineAsyncTaskLiveQueryReceipts::Tick()
{
	if (bIsComplete)
	{
		return;
	}

	GetTokenAndSignatureResult^ TokenResult = nullptr;
	try
	{
		TokenResult = XSTSTokenTask.get();
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
		FailTask(false /* We always want to hard fail here, as the XSTS is required */);
		return;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with code 0x%0.8X."), Ex->HResult);
		FailTaskCode(Ex->HResult, false /* We always want to hard fail here, as the XSTS is required */);
		return;
	}

	const FString XSTSToken(TokenResult->Token->Data());

	FPurchaseReceipt PurchaseReceipt;

	int32 TaskNumber = 0;
	while (InventoryTaskList.Num() > 0)
	{
		const bool bAllowShrinking = false;
		const Concurrency::task<InventoryItemsResult^> InventoryTask(InventoryTaskList.Pop(bAllowShrinking));
		TaskNumber++;

		InventoryItemsResult^ Results = nullptr;
		try
		{
			Results = InventoryTask.get();
		}
		catch (const std::exception& Ex)
		{
			UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
			FailTask(bSoftFailReceiptFailures);
			if (!bSoftFailReceiptFailures)
			{
				return;
			}
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts starting GetInventoryItems operation failed with code 0x%0.8X."), Ex->HResult);
			FailTaskCode(Ex->HResult, bSoftFailReceiptFailures);
			if (!bSoftFailReceiptFailures)
			{
				return;
			}
		}

		while (Results != nullptr)
		{
			const int32 FoundItems = Results->Items->Size;
			UE_LOG_ONLINE(Log, TEXT("[FOnlineAsyncTaskLiveQueryReceipts] Found %d Receipts during InventoryTask %d"), FoundItems, TaskNumber);

			for (int32 Index = 0; Index < FoundItems; ++Index)
			{
				InventoryItem^ PurchasedItem = Results->Items->GetAt(Index);
				check(PurchasedItem != nullptr);

				UE_LOG_ONLINE(Log, TEXT("Start ReceiptItemDetails for index %d"), Index);
				UE_LOG_ONLINE(Log, TEXT("ReceiptItemDetails: %s = %u"),		TEXT("ConsumableBalance"),		PurchasedItem->ConsumableBalance);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %ls"),	TEXT("ConsumableUrl"),			(PurchasedItem->ConsumableUrl != nullptr) ? PurchasedItem->ConsumableUrl->AbsoluteUri->Data() : L"N/A");
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %lld"),	TEXT("EndDate"),				PurchasedItem->EndDate.UniversalTime);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %d"),		TEXT("InventoryItemState"),		(int)PurchasedItem->InventoryItemState);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %s"),		TEXT("IsTrialEntitlement"),		PurchasedItem->IsTrialEntitlement ? TEXT("True") : TEXT("False"));
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %d"),		TEXT("MediaItemType"),			(int)PurchasedItem->MediaItemType);
				UE_LOG_ONLINE(Log, TEXT("ReceiptItemDetails: %s = %ls"),	TEXT("ProductId"),				PurchasedItem->ProductId->Data());
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %lld"),	TEXT("RightsObtainedDate"),		PurchasedItem->RightsObtainedDate.UniversalTime);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %lld"),	TEXT("StartDate"),				PurchasedItem->StartDate.UniversalTime);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %u"),		TEXT("TitleId"),				PurchasedItem->TitleId);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %lld"),	TEXT("TimeTrialRemaining"),		PurchasedItem->TrialTimeRemaining.Duration);
				UE_LOG_ONLINE(VeryVerbose, TEXT("ReceiptItemDetails: %s = %ls"),	TEXT("Url"),					PurchasedItem->Url->AbsoluteUri->Data());
				UE_LOG_ONLINE(Log, TEXT("End ReceiptItemDetails"));

				// How many times can we consume this item?
				const bool bIsConsumable = PurchasedItem->MediaItemType == Microsoft::Xbox::Services::Marketplace::MediaItemType::GameConsumable;
				const int32 ItemCount = bIsConsumable ? PurchasedItem->ConsumableBalance : 1;

				const FString ProductId(PurchasedItem->ProductId->Data());

				FPurchaseReceipt::FReceiptOfferEntry OfferEntry(FString(), ProductId, ItemCount);
				{
					// We generate them in index order, and later consume these receipts in the reverse order, so that index == quantity at the time
					for (int32 ConsumableIndex = 1; ConsumableIndex <= ItemCount; ++ConsumableIndex)
					{
						FPurchaseReceipt::FLineItemInfo LineItem;
						LineItem.ItemName = ProductId;
						LineItem.UniqueId = FString::Printf(TEXT("%s:%d:%s"), *ProductId, ConsumableIndex, *FGuid::NewGuid().ToString());
						LineItem.ValidationInfo = FString::Printf(TEXT("%s:%s"), *UserId.ToString(), *XSTSToken);
						OfferEntry.LineItems.Emplace(MoveTemp(LineItem));
					}
				}

				PurchaseReceipt.ReceiptOffers.Emplace(MoveTemp(OfferEntry));
			}

			// Bail out of loop if we're done
			if (!Results->HasNext)
			{
				break;
			}

			// Retrieve the next page of results
			try
			{
				Results = Concurrency::create_task(Results->GetNextAsync(INVENTORY_PAGE_SIZE)).get();
			}
			catch (const std::exception& Ex)
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts Retreiving next page of receipts failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
				FailTask(bSoftFailReceiptFailures);
				if (!bSoftFailReceiptFailures)
				{
					return;
				}
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveQueryReceipts Retreiving next page of receipts failed with code 0x%0.8X."), Ex->HResult);
				FailTaskCode(Ex->HResult, bSoftFailReceiptFailures);
				if (!bSoftFailReceiptFailures)
				{
					return;
				}
			}
		}
	}

	// Add our receipt into our array
	ReceiptData.Add(MoveTemp(PurchaseReceipt));

	// If we expected receipts, ensure we have at least 1
	if (bReceiptsExpected)
	{
		bool bHasReceipts = false;

		for (const FPurchaseReceipt& Receipt : ReceiptData)
		{
			if (Receipt.ReceiptOffers.Num() > 0)
			{
				bHasReceipts = true;
				break;
			}
		}

		if (!bHasReceipts)
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskLiveQueryReceipts expected to find receipts, but found none."));
			FailTask(NSLOCTEXT("OnlinePurchaseLive", "NoNewEntitlementsFound", "We expected at least one new entitlement, but found none."), false /* We always want to fail on this */);
			return;
		}
	}

	bWasSuccessful = ErrorResponse.bSucceeded;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveQueryReceipts::Finalize()
{
	if (ErrorResponse.bSucceeded)
	{
		FOnlinePurchaseLivePtr PurchaseInt = Subsystem->GetPurchaseLive();
		if (PurchaseInt.IsValid())
		{
			PurchaseInt->UserCachedReceipts.Emplace(MoveTemp(UserId), ReceiptData);
		}
	}
}

void FOnlineAsyncTaskLiveQueryReceipts::TriggerDelegates()
{
	// We support multiple delegate types for code-reuse reasons, so execute whichever are bound
	if (QueryReceiptsDelegate.IsBound())
	{
		QueryReceiptsDelegate.ExecuteIfBound(ErrorResponse);
	}

	if (PurchaseCheckoutDelegate.IsBound())
	{
		if (ReceiptData.IsValidIndex(0) && ErrorResponse.bSucceeded)
		{
			PurchaseCheckoutDelegate.ExecuteIfBound(ErrorResponse, MakeShared<FPurchaseReceipt>(ReceiptData[0]));
		}
		else
		{
			PurchaseCheckoutDelegate.ExecuteIfBound(ErrorResponse, MakeShared<FPurchaseReceipt>());
		}
	}
}

void FOnlineAsyncTaskLiveQueryReceipts::FailTask(const bool bSoftFail)
{
	// Call FText version with generic message
	FailTask(NSLOCTEXT("OnlineSubsystem", "FailedToRetrievePurchases", "An error has occurred while querying purchases; purchased content may be temporarily unavailable."), bSoftFail);
}

void FOnlineAsyncTaskLiveQueryReceipts::FailTask(const FText ErrorMessage, const bool bSoftFail)
{
	ErrorResponse.SetFromErrorMessage(ErrorMessage);
	ErrorResponse.bSucceeded = bSoftFail;

	bWasSuccessful = bSoftFail;
	if (!bSoftFail)
	{
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskLiveQueryReceipts::FailTaskCode(const int32 ErrorCode, const bool bSoftFail)
{
	FFormatNamedArguments FormatArguments;
	FormatArguments.Add(TEXT("ErrorCode"), FText::FromString(FString::Printf(TEXT("0x%0.8X"), ErrorCode)));
	const FText ErrorMessage(FText::Format(NSLOCTEXT("OnlineSubsystem", "FailedToRetrievePurchasesWithCode", "An error has occurred while querying your purchases; purchased content may be temporarily unavailable. Xbox Error Code: {ErrorCode}"), FormatArguments));

	FailTask(ErrorMessage, bSoftFail);
}

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END