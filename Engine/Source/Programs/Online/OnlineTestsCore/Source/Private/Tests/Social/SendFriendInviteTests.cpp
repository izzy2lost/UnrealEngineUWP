// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Social/GetFriendsHelper.h"	
#include "Helpers/Social/QueryFriendsHelper.h"
#include "Helpers/Social/SendFriendInviteHelper.h"
#include "Helpers/Identity/IdentityGetLoginByUserId.h"
#include "Helpers/Auth/AuthLogout.h"
#include "Helpers/Auth/AuthLogin.h"

#include "OnlineCatchHelper.h"

#define SOCIAL_TAG "[suite_social]"
#define EG_SOCIAL_SENDFRIENDINVITE_TAG SOCIAL_TAG "[sendfriendinvite]"
#define EG_SOCIAL_SENDFRIENDINVITEEOS_TAG SOCIAL_TAG "[sendfriendinvite][.EOS]"
#define EG_SOCIAL_DISABLED_TAG SOCIAL_TAG "[socialdisabled]"
#define SOCIAL_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, SOCIAL_TAG __VA_ARGS__)

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns a fail message if use invalid local user account id", EG_SOCIAL_SENDFRIENDINVITE_TAG)
{
	const int32 NumUsersToLogin = 0;

	FSendFriendInvite::Params OpSendFriendInviteParams;
	FSendFriendInviteHelper::FHelperParams SendFriendInviteHelperParams;
	SendFriendInviteHelperParams.OpParams = &OpSendFriendInviteParams;
	SendFriendInviteHelperParams.OpParams->LocalAccountId = FAccountId();
	SendFriendInviteHelperParams.ExpectedError = TOnlineResult<FSendFriendInvite>(Errors::InvalidParams());

	GetLoginPipeline(NumUsersToLogin)
		.EmplaceStep<FSendFriendInviteHelper>(MoveTemp(SendFriendInviteHelperParams));

	RunToCompletion();
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns a fail message if use invalid target user account id", EG_SOCIAL_SENDFRIENDINVITE_TAG)
{
	FAccountId AccountId;

	FSendFriendInvite::Params OpSendFriendInviteParams;
	FSendFriendInviteHelper::FHelperParams SendFriendInviteHelperParams;
	SendFriendInviteHelperParams.OpParams = &OpSendFriendInviteParams;
	SendFriendInviteHelperParams.OpParams->TargetAccountId = FAccountId();
	SendFriendInviteHelperParams.ExpectedError = TOnlineResult<FSendFriendInvite>(Errors::InvalidParams());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	SendFriendInviteHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FSendFriendInviteHelper>(MoveTemp(SendFriendInviteHelperParams));

	RunToCompletion();
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns a fail message if the local user is not logged in", EG_SOCIAL_SENDFRIENDINVITEEOS_TAG)
{
	FAccountId FirstAccountId, SecondAccountId;

	FSendFriendInvite::Params OpSendFriendInviteParams;
	FSendFriendInviteHelper::FHelperParams SendFriendInviteHelperParams;
	SendFriendInviteHelperParams.OpParams = &OpSendFriendInviteParams;
	SendFriendInviteHelperParams.ExpectedError = TOnlineResult<FSendFriendInvite>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);

	SendFriendInviteHelperParams.OpParams->LocalAccountId = FirstAccountId;
	SendFriendInviteHelperParams.OpParams->TargetAccountId = SecondAccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FSendFriendInviteHelper>(MoveTemp(SendFriendInviteHelperParams));

	RunToCompletion(bLogout);
}

//SOCIAL_TEST_CASE("Verify that SendFriendInvite returns fail message if ERelationship with target user is Friend")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is NotFriend, ERelationship becomes InviteSent")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is InviteSent, ERelationship remains InviteSent")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is InviteReceived, ERelationship becomes InviteSent")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that SendFriendInvite returns fail message if ERelationship with target user is Blocked")
//{
//	// TODO
//}
