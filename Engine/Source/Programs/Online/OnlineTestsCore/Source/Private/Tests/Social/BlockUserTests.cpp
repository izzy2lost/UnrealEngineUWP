// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Social/BlockUserHelper.h"
#include "Helpers/Auth/AuthLogout.h"

#include "OnlineCatchHelper.h"

#define SOCIAL_TAG "[suite_social]"
#define EG_SOCIAL_BLOCKUSER_TAG SOCIAL_TAG "[blockuser]"
#define EG_SOCIAL_BLOCKUSEREOS_TAG SOCIAL_TAG "[blockuser][.EOS]"
#define SOCIAL_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, SOCIAL_TAG __VA_ARGS__)

SOCIAL_TEST_CASE("Verify that BlockUser returns a error if call with an invalid local user account id", EG_SOCIAL_BLOCKUSER_TAG)
{
	const int32 NumUsersToLogin = 0;

	FBlockUser::Params OpBlockUserParams;
	FBlockUserHelper::FHelperParams BlockUserHelperHelperParams;
	BlockUserHelperHelperParams.OpParams = &OpBlockUserParams;
	BlockUserHelperHelperParams.OpParams->LocalAccountId = FAccountId();

	SubsystemType OnlineSubsystem = GetSubsystem();
	EOnlineServices ServicesProvider = OnlineSubsystem->GetServicesProvider();
	
	/*if (ServicesProvider == EOnlineServices::Epic)
	{
		BlockUserHelperHelperParams.ExpectedError = TOnlineResult<FBlockUser>(Errors::InvalidParams());

	}
	else if (ServicesProvider == EOnlineServices::Xbox)
	{
		BlockUserHelperHelperParams.ExpectedError = TOnlineResult<FBlockUser>(Errors::InvalidUser());
	}*/

	BlockUserHelperHelperParams.ExpectedError = TOnlineResult<FBlockUser>(Errors::InvalidParams());


	GetLoginPipeline(NumUsersToLogin)
		.EmplaceStep<FBlockUserHelper>(MoveTemp(BlockUserHelperHelperParams));

	RunToCompletion();
}

SOCIAL_TEST_CASE("Verify that BlockUser returns a error if call with an target user account id", EG_SOCIAL_BLOCKUSER_TAG)
{
	FAccountId AccountId;

	FBlockUser::Params OpBlockUserParams;
	FBlockUserHelper::FHelperParams BlockUserHelperHelperParams;
	BlockUserHelperHelperParams.OpParams = &OpBlockUserParams;
	BlockUserHelperHelperParams.OpParams->TargetAccountId = FAccountId();
	BlockUserHelperHelperParams.ExpectedError = TOnlineResult<FBlockUser>(Errors::InvalidParams());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	BlockUserHelperHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FBlockUserHelper>(MoveTemp(BlockUserHelperHelperParams));

	RunToCompletion();
}

SOCIAL_TEST_CASE("Verify that BlockUser returns a fail message if the local user is not logged in", EG_SOCIAL_BLOCKUSEREOS_TAG)
{
	FAccountId FirstAccountId, SecondAccountId;
	int32 UserNumToLogout = 1;
	bool bLogout = true;

	FBlockUser::Params OpBlockUserParams;
	FBlockUserHelper::FHelperParams BlockUserHelperHelperParams;
	BlockUserHelperHelperParams.OpParams = &OpBlockUserParams;
	BlockUserHelperHelperParams.ExpectedError = TOnlineResult<FBlockUser>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	BlockUserHelperHelperParams.OpParams->LocalAccountId = FirstAccountId;
	BlockUserHelperHelperParams.OpParams->TargetAccountId = SecondAccountId;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FBlockUserHelper>(MoveTemp(BlockUserHelperHelperParams));

	RunToCompletion(bLogout, UserNumToLogout);
}

SOCIAL_TEST_CASE("Verify that BlockUser completes successfully if both users are logged in", EG_SOCIAL_BLOCKUSEREOS_TAG)
{
	FAccountId FirstAccountId, SecondAccountId;

	FBlockUser::Params OpBlockUserParams;
	FBlockUserHelper::FHelperParams BlockUserHelperHelperParams;
	BlockUserHelperHelperParams.OpParams = &OpBlockUserParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	BlockUserHelperHelperParams.OpParams->LocalAccountId = FirstAccountId;
	BlockUserHelperHelperParams.OpParams->TargetAccountId = SecondAccountId;

	LoginPipeline
		.EmplaceStep<FBlockUserHelper>(MoveTemp(BlockUserHelperHelperParams));

	RunToCompletion();
}
