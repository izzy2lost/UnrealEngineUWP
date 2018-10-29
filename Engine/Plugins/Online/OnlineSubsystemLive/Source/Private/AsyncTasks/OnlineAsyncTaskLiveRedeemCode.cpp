// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"

// @ATG_CHANGE : BEGIN - UWP Store support
#if WITH_MARKETPLACE
// @ATG_CHANGE : END

#include "OnlineAsyncTaskLiveRedeemCode.h"
#include "OnlineAsyncTaskLiveQueryReceipts.h"
#include "OnlineSubsystemLive.h"
#include "Misc/CoreDelegates.h"

FOnlineAsyncTaskLiveRedeemCode::FOnlineAsyncTaskLiveRedeemCode(FOnlineSubsystemLive* const InLiveInterface, Windows::Xbox::System::User^ InLiveUser, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InNetIdLive, const FOnPurchaseRedeemCodeComplete& InDelegate)
	: FOnlineAsyncTaskLive(InLiveInterface, 0)
	, LocalUser(InLiveUser)
	, LiveContext(InLiveContext)
	, NetIdLive(InNetIdLive)
	, Delegate(InDelegate)
{
}

FOnlineAsyncTaskLiveRedeemCode::~FOnlineAsyncTaskLiveRedeemCode()
{
	// Ensure we're no-longer listening for reactivation
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
}

void FOnlineAsyncTaskLiveRedeemCode::Initialize()
{
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FOnlineAsyncTaskLiveRedeemCode::OnApplicationReactivated);
	try
	{
		Windows::Foundation::IAsyncAction^ ShowCodeTask = Windows::Xbox::ApplicationModel::Store::Product::ShowRedeemCodeAsync(LocalUser, nullptr);
		concurrency::create_task(ShowCodeTask).then([this](concurrency::task<void> task)
		{
			try
			{
				task.get();
				UE_LOG_ONLINE(Log, TEXT("Redeem Code: Code Redemption UI now displaying."));
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Error, TEXT("Error showing Code Redemption UI, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
			}
		});
	}
	catch (Platform::Exception^ Ex)
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		ErrorResult.SetFromErrorMessage(NSLOCTEXT("OnlineSubsystemLive", "RedeemCodeFailed", "Unable to redeem code."));
		ErrorResult.bSucceeded = false;
		bIsComplete = true;
		UE_LOG_ONLINE(Warning, TEXT("Error starting Code Redemption: %s error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
	}
}

void FOnlineAsyncTaskLiveRedeemCode::Finalize()
{
	ErrorResult.bSucceeded = bWasSuccessful;
	if (bWasSuccessful)
	{
		constexpr bool bReceiptsExpected = true;
		Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveQueryReceipts>(Subsystem, NetIdLive, LiveContext, bReceiptsExpected, Delegate);
	}
}

void FOnlineAsyncTaskLiveRedeemCode::TriggerDelegates()
{
	FOnlinePurchaseLivePtr PurchaseInt = Subsystem->GetPurchaseLive();
	if (PurchaseInt.IsValid())
	{
		PurchaseInt->bIsCurrentyInCheckout = false;
	}

	// Only trigger delegates now if we failed, otherwise our QueryReceipts call will do it for us
	if (!bWasSuccessful)
	{
		Delegate.ExecuteIfBound(ErrorResult, MakeShared<FPurchaseReceipt>());
	}
}

void FOnlineAsyncTaskLiveRedeemCode::OnApplicationReactivated()
{
	// Assume successful and let QueryReceipts in Finalize call our delegate if new stuff was found
	bWasSuccessful = true;
	bIsComplete = true;
}

// @ATG_CHANGE : BEGIN - UWP Store support
#endif // WITH_MARKETPLACE
// @ATG_CHANGE : END