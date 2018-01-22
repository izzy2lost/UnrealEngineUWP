// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlineAsyncTaskLivePurchaseOffer.h"
#include "OnlineAsyncTaskLiveQueryReceipts.h"
#include "OnlineSubsystemLive.h"

FOnlineAsyncTaskLivePurchaseOffer::FOnlineAsyncTaskLivePurchaseOffer(FOnlineSubsystemLive* const InLiveInterface, const FString& InSignedOffer, Windows::Xbox::System::User^ InLiveUser, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InNetIdLive, const FOnPurchaseCheckoutComplete& InDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveUser, InLiveContext)
	, SignedOffer(InSignedOffer)
	, NetIdLive(InNetIdLive)
	, Delegate(InDelegate)
{
	check(!SignedOffer.IsEmpty());
}

Windows::Foundation::IAsyncAction^ FOnlineAsyncTaskLivePurchaseOffer::CreateOperation()
{
	Windows::Foundation::IAsyncAction^ AsyncOperation;
	try
	{
		AsyncOperation = Windows::Xbox::ApplicationModel::Store::Product::ShowPurchaseAsync(LocalUser, ref new Platform::String(*SignedOffer));
	}
	catch (Platform::COMException^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Error starting Live purchase, SignedOffer: %s error: (0x%0.8X) %ls."), *SignedOffer, Ex->HResult, Ex->ToString()->Data());
	}

	return AsyncOperation;
}

bool FOnlineAsyncTaskLivePurchaseOffer::ProcessResult(const Concurrency::task<void>& CompletedTask)
{
	try
	{
		CompletedTask.get();

		// If CompletedTask didn't throw, we were successful
		ErrorResult.bSucceeded = true;
	}
	catch (const Concurrency::task_canceled& Ex)
	{
		ErrorResult.SetFromErrorMessage(NSLOCTEXT("OnlineSubsystemLive", "PurchaseCancelled", "Your purchase was cancelled."));
		UE_LOG_ONLINE(Log, TEXT("FOnlinePurchaseLive::Checkout was cancelled with reason %s."), ANSI_TO_TCHAR(Ex.what()));
	}
	catch (Platform::Exception^ Ex)
	{
		ErrorResult.SetFromErrorMessage(NSLOCTEXT("OnlineSubsystemLive", "PurchaseFailed", "We were unable to complete your purchase at this time."));
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePurchaseLive::Checkout failed with code 0x%0.8X and message %ls."), Ex->HResult, Ex->Message->Data());
	}

	return ErrorResult.bSucceeded;
}

void FOnlineAsyncTaskLivePurchaseOffer::Finalize()
{
	FOnlinePurchaseLivePtr PurchaseInt = Subsystem->GetPurchaseLive();
	if (PurchaseInt.IsValid())
	{
		PurchaseInt->bIsCurrentyInCheckout = false;
	}

	// Query our new entitlements if we succeeded
	if (ErrorResult.bSucceeded)
	{
		constexpr bool bReceiptsExpected = true;
		Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryReceipts>(Subsystem, NetIdLive, LiveContext, bReceiptsExpected, Delegate);
	}
}

void FOnlineAsyncTaskLivePurchaseOffer::TriggerDelegates()
{
	// Only trigger delegates now if we failed, otherwise our QueryReceipts call will do it for us
	if (!ErrorResult.bSucceeded)
	{
		Delegate.ExecuteIfBound(ErrorResult, MakeShared<FPurchaseReceipt>());
	}
}

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END