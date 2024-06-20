// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Commerce/GetEntitlementsHelper.h"
#include "Helpers/Commerce/QueryEntitlementsHelper.h"
#include "Helpers/Auth/AuthLogout.h"

#define COMMERCE_TAG "[suite_commerce]"

#define COMMERCE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, COMMERCE_TAG __VA_ARGS__)

COMMERCE_TEST_CASE("Verify that QueryEntitlements returns a fail message if the local user is not logged in")
{
	FAccountId AccountId;
	TSharedPtr<FPlatformUserId> AccountPlatformUserId = MakeShared<FPlatformUserId>();

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceQueryEntitlements>(Errors::NotLoggedIn());
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline({ AccountId });
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

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
		.EmplaceStep<FQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams));

			RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryEntitlements returns a fail message of the given local user ID does not match the actual local user ID")
{
	FAccountId AccountId;

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;
	QueryEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceQueryEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline({ AccountId });
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = FAccountId();

	LoginPipeline
		.EmplaceStep<FQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams));

	RunToCompletion();
}

COMMERCE_TEST_CASE("Verify that QueryEntitlements caches all entitlements")
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedEntitlementsNum = 0;

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline({ AccountId });
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;
	QueryEntitlementsHelperParams.OpParams->bIncludeRedeemed = true;
	GetEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams))
		.EmplaceStep<FGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams), ExpectedEntitlementsNum);

	RunToCompletion();
}