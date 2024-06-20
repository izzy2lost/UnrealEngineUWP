// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Commerce/GetEntitlementsHelper.h"
#include "Helpers/Auth/AuthLogout.h"

#define COMMERCE_TAG "[suite_commerce]"
#define COMMERCE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, COMMERCE_TAG __VA_ARGS__)

COMMERCE_TEST_CASE("Verify that GetEntitlements returns a fail message if the local user is not logged in")
{
	FAccountId AccountId;
	TSharedPtr<FPlatformUserId> AccountPlatformUserId = MakeShared<FPlatformUserId>();

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;
	GetEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceGetEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline({ AccountId });
	GetEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceLambda([&AccountId, AccountPlatformUserId](SubsystemType OnlineSubsystem)
			{
				UE::Online::IAuthPtr OnlineAuthPtr = OnlineSubsystem->GetAuthInterface();
				REQUIRE(OnlineAuthPtr);
				UE::Online::TOnlineResult<UE::Online::FAuthGetLocalOnlineUserByOnlineAccountId> UserPlatfromUserIdResult = OnlineAuthPtr->GetLocalOnlineUserByOnlineAccountId({ AccountId });
				REQUIRE(UserPlatfromUserIdResult.IsOk());
				CHECK(UserPlatfromUserIdResult.TryGetOkValue() != nullptr);
				*AccountPlatformUserId = UserPlatfromUserIdResult.TryGetOkValue()->AccountInfo->PlatformUserId;
			})
		.EmplaceStep<FAuthLogoutStep>(MoveTemp(AccountPlatformUserId))
		.EmplaceStep<FGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetEntitlements returns a fail message of the given local user ID does not match the actual local user ID")
{
	FAccountId AccountId;

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;
	GetEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceGetEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline({ AccountId });
	GetEntitlementsHelperParams.OpParams->LocalAccountId = FAccountId();

	LoginPipeline
		.EmplaceStep<FGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams));

	RunToCompletion();
}

COMMERCE_TEST_CASE("Verify that GetEntitlements returns a list of all cached entitlements")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that GetEntitlements returns an empty list when no entitlements are cached")
{
	// TODO
}