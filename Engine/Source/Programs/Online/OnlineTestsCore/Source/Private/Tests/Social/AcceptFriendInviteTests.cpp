// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/Social/AcceptFriendInviteHelper.h"
#include "Helpers/Auth/AuthLogout.h"

#include "OnlineCatchHelper.h"

#define SOCIAL_TAG "[suite_social]"
#define EG_SOCIAL_ACCEPTFRIENDINVITE_TAG SOCIAL_TAG "[acceptfriendinvite]"
#define EG_SOCIAL_ACCEPTFRIENDINVITEEOS_TAG SOCIAL_TAG "[acceptfriendinvite][.EOS]"
#define EG_SOCIAL_DISABLED_TAG SOCIAL_TAG "[socialdisabled]"
#define SOCIAL_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, SOCIAL_TAG __VA_ARGS__)

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns an error if call with an invalid local user account id", EG_SOCIAL_ACCEPTFRIENDINVITE_TAG)
//{
//	const int32 NumUsersToLogin = 0;
//
//	FAcceptFriendInvite::Params OpAcceptFriendInviteParams;
//	FAcceptFriendInviteHelper::FHelperParams AcceptInviteHelperParams;
//	AcceptInviteHelperParams.OpParams = &OpAcceptFriendInviteParams;
//	AcceptInviteHelperParams.OpParams->LocalAccountId = FAccountId();
//	AcceptInviteHelperParams.ExpectedError = TOnlineResult<FAcceptFriendInvite>(Errors::InvalidParams());
//
//	GetLoginPipeline(NumUsersToLogin)
//		.EmplaceStep<FAcceptFriendInviteHelper>(MoveTemp(AcceptInviteHelperParams));
//
//	RunToCompletion();
//}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns an error if call with an target user account id", EG_SOCIAL_ACCEPTFRIENDINVITE_TAG)
{
	FAccountId AccountId;

	FAcceptFriendInvite::Params OpAcceptFriendInviteParams;
	FAcceptFriendInviteHelper::FHelperParams AcceptInviteHelperParams;
	AcceptInviteHelperParams.OpParams = &OpAcceptFriendInviteParams;
	AcceptInviteHelperParams.OpParams->TargetAccountId = FAccountId();
	AcceptInviteHelperParams.ExpectedError = TOnlineResult<FAcceptFriendInvite>(Errors::InvalidParams());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	AcceptInviteHelperParams.OpParams->LocalAccountId = AccountId;

	LoginPipeline
		.EmplaceStep<FAcceptFriendInviteHelper>(MoveTemp(AcceptInviteHelperParams));

	RunToCompletion();
}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if the local user is not logged in", EG_SOCIAL_ACCEPTFRIENDINVITEEOS_TAG)
//{
//	FAccountId FirstAccountId, SecondAccountId;
//	int32 UserNumToLogout = 1;
//	bool bLogout = true;
//
//	FAcceptFriendInvite::Params OpAcceptFriendInviteParams;
//	FAcceptFriendInviteHelper::FHelperParams AcceptInviteHelperParams;
//	AcceptInviteHelperParams.OpParams = &OpAcceptFriendInviteParams;
//	AcceptInviteHelperParams.ExpectedError = TOnlineResult<FAcceptFriendInvite>(Errors::NotLoggedIn());
//
//	FTestPipeline& LoginPipeline = GetLoginPipeline(FirstAccountId, SecondAccountId);
//
//	AcceptInviteHelperParams.OpParams->LocalAccountId = FirstAccountId;
//	AcceptInviteHelperParams.OpParams->TargetAccountId = SecondAccountId;
//
//	LoginPipeline
//		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
//		.EmplaceStep<FAcceptFriendInviteHelper>(MoveTemp(AcceptInviteHelperParams));
//
//	RunToCompletion(bLogout, UserNumToLogout);
//}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is Friend")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is NotFriend")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is InviteSent")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite successfully completes if ERelationship with target user is InviteReceived, ERelationship becomes Friend")
//{
//	// TODO
//}

//SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is Blocked", EG_SOCIAL_DISABLED_TAG)
//{
//	// TODO
//}
