// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "OnlineError.h"

class FOnlineSubsystemLive;

/** Map of User's Permission's Allowed status */
using FPrivacyPermissionsResultsMap = TMap<TSharedRef<const FUniqueNetId>, TMap<FString, bool> >;

/**
 * Delegate used when the user's privacy permissions request has completed
 *
 * @param RequestStatus If this request was successful or not
 * @param RequestingUser The user who generated this request
 * @param Results A Map of Users to permission results
 */
DECLARE_DELEGATE_ThreeParams(FOnLivePrivacyPermissionsQueryComplete, const FOnlineError& /*RequestStatus*/, const TSharedRef<const FUniqueNetId>& /*RequestingUser*/, const FPrivacyPermissionsResultsMap& /*Results*/);

/**
 * Async Task to query the account details of a user
 */
class FOnlineAsyncTaskLiveQueryPrivacyPermissions
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult^>^>
{
public:
	FOnlineAsyncTaskLiveQueryPrivacyPermissions(FOnlineSubsystemLive* InLiveSubsystem,
								   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
								   const TArray<TSharedRef<const FUniqueNetId>>& InUsersToQuery,
								   Windows::Foundation::Collections::IVectorView<Platform::String^>^ InPermissionsToQuery,
								   const FOnLivePrivacyPermissionsQueryComplete& InDelegate);
	virtual ~FOnlineAsyncTaskLiveQueryPrivacyPermissions() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryPrivacyPermissions"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult^>^>& CompletedTask) override;

	virtual void Finalize();
	virtual void TriggerDelegates();

protected:
	TArray<TSharedRef<const FUniqueNetId>> UsersToQuery;
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ PermissionsToQueryVector;
	FOnLivePrivacyPermissionsQueryComplete Delegate;
	FOnlineError OnlineError;
	FPrivacyPermissionsResultsMap UserPermissionsMap;
};