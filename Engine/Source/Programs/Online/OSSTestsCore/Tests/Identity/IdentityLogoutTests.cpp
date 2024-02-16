// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestDriver.h"

#include "OnlineSubsystemCatchHelper.h"
#include "Helpers/Identity/IdentityLoginHelper.h"
#include "Helpers/Identity/IdentityLogoutHelper.h"

#define IDENTITY_TAG "[suite_identity]"
#define EG_IDENTITY_LOGOUT_TAG IDENTITY_TAG "[logout]"

#define IDENTITY_TEST_CASE(x, ...) ONLINESUBSYSTEM_TEST_CASE(x, IDENTITY_TAG __VA_ARGS__)

IDENTITY_TEST_CASE("Verify calling Identity Logout with valid inputs returns the expected result(Success Case)", EG_IDENTITY_LOGOUT_TAG)
{
	int32 LocalUserNum = 0;
	int32 TargetUserNum = 1;
	int32 NumUsersToImplicitLogin = 1;

	GetLoginPipeline(NumUsersToImplicitLogin)
		.EmplaceStep<FIdentityLoginStep>(TargetUserNum, GetCredentials(TargetUserNum))
		.EmplaceStep<FIdentityLogoutStep>(TargetUserNum);

	RunToCompletion();
}
