// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlinePurchaseInterfaceLive.h"
#include "OnlineStoreInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveQueryReceipts.h"
#include "AsyncTasks/OnlineAsyncTaskLivePurchaseOffer.h"
#include "AsyncTasks/OnlineAsyncTaskLiveRedeemCode.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Xbox::ApplicationModel::Store::Product;
using Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventHandler;
using Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventArgs;

FOnlinePurchaseLive::FOnlinePurchaseLive(FOnlineSubsystemLive* InLiveSubsystem)
	: LiveSubsystem(InLiveSubsystem)
	, bIsCurrentyInCheckout(false)
	, bSoftFailReceiptFailures(false)
{
	check(LiveSubsystem);
	GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("ReceiptsXSTSEndpoint"), ReceiptsXSTSEndpoint, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemLive"), TEXT("bSoftFailReceiptFailures"), bSoftFailReceiptFailures, GEngineIni);
}

FOnlinePurchaseLive::~FOnlinePurchaseLive()
{
	UnregisterLivePurchaseHooks();
}

bool FOnlinePurchaseLive::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
	Windows::Xbox::System::User^ LiveUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(static_cast<const FUniqueNetIdLive&>(UserId));
	if (!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePurchaseLive::IsAllowedToPurchase failed with due to known user %s."), *UserId.ToDebugString());
		return false;
	}

	const uint32 PrivilegeToCheck = static_cast<uint32>(Windows::Xbox::ApplicationModel::Store::KnownPrivileges::XPRIVILEGE_PURCHASE_CONTENT);

	bool bHasPermission = false;

	auto CheckOp = Windows::Xbox::ApplicationModel::Store::Product::CheckPrivilegeAsync(LiveUser, PrivilegeToCheck, true, nullptr);
	try
	{
		Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult Result = Concurrency::create_task(CheckOp).get();
		if (Result == Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult::NoIssue)
		{
			bHasPermission = true;
		}
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePurchaseLive::IsAllowedToPurchase failed with code 0x%0.8X."), Ex->HResult);
	}

	return bHasPermission;
}

void FOnlinePurchaseLive::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	// Lambda to wrap calling our delegate with an error and logging the message
	auto CallDelegateError = [this, &Delegate](const FString& ErrorMessage)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, ErrorMessage]
		{
			UE_LOG_ONLINE(Error, TEXT("%s"), *ErrorMessage);

			const TSharedRef<FPurchaseReceipt> PurchaseReceipt = MakeShared<FPurchaseReceipt>();
			PurchaseReceipt->TransactionState = EPurchaseTransactionState::Failed;

			Delegate.ExecuteIfBound(FOnlineError(false), PurchaseReceipt);
		});
	};

	const FUniqueNetIdLive& NetIdLive = static_cast<const FUniqueNetIdLive&>(UserId);

	Windows::Xbox::System::User^ LiveUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(NetIdLive);
	if (!LiveUser)
	{
		CallDelegateError(FString::Printf(TEXT("FOnlinePurchaseLive::Checkout failed due to unknown user %s."), *UserId.ToDebugString()));
		return;
	}
	else if (CheckoutRequest.PurchaseOffers.Num() == 0)
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, there were no entries passed to purchase"));
		return;
	}
	else if (CheckoutRequest.PurchaseOffers.Num() != 1)
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, there were more than one entry passed to purchase. Live currently only supports one."));
		return;
	}

	XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(LiveUser);
	if (!UserLiveContext)
	{
		CallDelegateError(FString::Printf(TEXT("FOnlinePurchaseLive::Checkout failed due to missing context for user %s."), *UserId.ToDebugString()));
		return;
	}

	check(CheckoutRequest.PurchaseOffers.IsValidIndex(0));
	const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Entry = CheckoutRequest.PurchaseOffers[0];

	if (Entry.Quantity != 1)
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, purchase quantity not set to one. Live currently only supports one."));
		return;
	}

	if (Entry.OfferId.IsEmpty())
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, OfferId is blank."));
		return;
	}

	TSharedPtr<FOnlineStoreOfferLive> LiveOffer = StaticCastSharedPtr<FOnlineStoreOfferLive>(LiveSubsystem->GetStoreLive()->GetOffer(Entry.OfferId));
	if (!LiveOffer.IsValid())
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, Could not find corresponding offer."));
		return;
	}

	if (bIsCurrentyInCheckout)
	{
		CallDelegateError(TEXT("FOnlinePurchaseLive::Checkout failed, another purchase already in progress."));
		return;
	}

	bIsCurrentyInCheckout = true;
	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLivePurchaseOffer>(LiveSubsystem, LiveOffer->SignedOffer, LiveUser, UserLiveContext, NetIdLive, Delegate);
}

void FOnlinePurchaseLive::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
	// No-Op
	UNREFERENCED_PARAMETER(UserId);
	UNREFERENCED_PARAMETER(ReceiptId);
}

void FOnlinePurchaseLive::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	UNREFERENCED_PARAMETER(RedeemCodeRequest);

	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	Windows::Xbox::System::User^ LiveUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(LiveUserId);
	if (!LiveUser)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, LiveUserId]
		{
			UE_LOG_ONLINE(Warning, TEXT("No LocalUser found for user %s"), *LiveUserId.ToDebugString());
			Delegate.ExecuteIfBound(FOnlineError(TEXT("NoLocalUserFound")), MakeShared<FPurchaseReceipt>());
		});
		return;
	}

	Microsoft::Xbox::Services::XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(LiveUser);
	if (!UserLiveContext)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, LiveUserId]
		{
			UE_LOG_ONLINE(Warning, TEXT("No XboxLiveContext found for user %s"), *LiveUserId.ToDebugString());
			Delegate.ExecuteIfBound(FOnlineError(TEXT("NoLiveContextFound")), MakeShared<FPurchaseReceipt>());
		});
		return;
	}

	bIsCurrentyInCheckout = true;
	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveRedeemCode>(LiveSubsystem, LiveUser, UserLiveContext, LiveUserId, Delegate);
}

void FOnlinePurchaseLive::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	UNREFERENCED_PARAMETER(bRestoreReceipts);

	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	Windows::Xbox::System::User^ LiveUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(LiveUserId);
	if (!LiveUser)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, LiveUserId]
		{
			UE_LOG_ONLINE(Warning, TEXT("No LocalUser found for user %s"), *LiveUserId.ToDebugString());
			Delegate.ExecuteIfBound(FOnlineError(TEXT("NoLocalUserFound")));
		});
		return;
	}

	Microsoft::Xbox::Services::XboxLiveContext^ UserLiveContext = LiveSubsystem->GetLiveContext(LiveUser);
	if (!UserLiveContext)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, LiveUserId]
		{
			UE_LOG_ONLINE(Warning, TEXT("No XboxLiveContext found for user %s"), *LiveUserId.ToDebugString());
			Delegate.ExecuteIfBound(FOnlineError(TEXT("NoLiveContextFound")));
		});
		return;
	}

	constexpr bool bReceiptsExpected = false;
	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryReceipts>(LiveSubsystem, LiveUserId, UserLiveContext, bReceiptsExpected, Delegate);
}

void FOnlinePurchaseLive::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	const TArray<FPurchaseReceipt>* FoundReceipts = UserCachedReceipts.Find(LiveUserId);
	if (FoundReceipts == nullptr)
	{
		// The user is getting receipts when we haven't queried for this user (or the query hasn't finished yet)
		UE_LOG_ONLINE(Warning, TEXT("No cached receipts found for player %s"), *LiveUserId.ToDebugString());
		OutReceipts.Empty();
	}
	else
	{
		OutReceipts = *FoundReceipts;
	}
}

void FOnlinePurchaseLive::RegisterLivePurchaseHooks()
{
	TWeakPtr<FOnlinePurchaseLive, ESPMode::ThreadSafe> WeakThis(AsShared());
	LiveEventProductPurchaseHandle = Product::ProductPurchased += ref new ProductPurchasedEventHandler([WeakThis](ProductPurchasedEventArgs^ Args)
	{
		UE_LOG_ONLINE(Log, TEXT("A product has been purchased!  Receipt Id: %ls"), Args->Receipt->Data());

		// We are on the async thread, only use this StrongPtr to queue a lambda to be called on the game-thread
		FOnlinePurchaseLivePtr StrongThis = WeakThis.Pin();
		if (!StrongThis.IsValid())
		{
			return;
		}

		// Try to read the JSON string from live about this purchase
		TSharedRef<TJsonReader<> > JsonReader(TJsonReaderFactory<>::Create(FString(Args->Receipt->Data())));
		TSharedPtr<FJsonObject> JsonObject;
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Unable to read live purchase event json string: %ls"), Args->Receipt->Data());
			return;
		}

		// Try to pull the player-who-received-the-purchase's XUID
		FString UserXuidString;
		if (!JsonObject->TryGetStringField(TEXT("beneficiaryXuid"), UserXuidString) || UserXuidString.IsEmpty())
		{
			UE_LOG_ONLINE(Warning, TEXT("Missing/Empty field 'beneficiaryXuid' in live purchase event json string: %ls"), Args->Receipt->Data());
			return;
		}

		// Json XUIDs are stored as doubles, and so we need to trim a trailing decimal point if there is one present.
		// It seems like a bad idea to store large numbers as numbers in JSON, so maybe they'll switch to strings
		// at some point in the future.  If so, this code will still just work and do nothing.
		static const FString TrailingDecimal(TEXT(".0"));
		UserXuidString.RemoveFromEnd(TrailingDecimal, ESearchCase::CaseSensitive);

		// Queue this user to be updated about this next-tick on the game-thread, if they're not mid-purchase
		StrongThis->LiveSubsystem->ExecuteNextTick([WeakThis, UserXuidString]()
		{
			FOnlinePurchaseLivePtr StrongThis = WeakThis.Pin();
			if (StrongThis.IsValid())
			{
				if (!StrongThis->bIsCurrentyInCheckout)
				{
					const TSharedRef<const FUniqueNetId> UserId(MakeShared<const FUniqueNetIdLive>(UserXuidString));
					StrongThis->TriggerOnUnexpectedPurchaseReceiptDelegates(*UserId);
				}
			}
		});
	});
}

void FOnlinePurchaseLive::UnregisterLivePurchaseHooks()
{
	Product::ProductPurchased -= LiveEventProductPurchaseHandle;
}

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END