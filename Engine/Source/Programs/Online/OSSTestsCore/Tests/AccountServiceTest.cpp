// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestDriver.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"

#include "OnlineSubsystemCatchHelper.h"

#define ACCOUNTSERVICE_TAG "[AccountService]"

#define ACCOUNTSERVICE_TEST_CASE(x, ...) ONLINESUBSYSTEM_TEST_CASE(x, ACCOUNTSERVICE_TAG __VA_ARGS__)

ACCOUNTSERVICE_TEST_CASE("Verify if we can properly create a OnlineAccountCredentials object")
{
	// FOnlineAccountCredentials(const FString& InType, const FString& InId, const FString& InToken) :
	FString LocalType = "test_account";
	FString LocalId = "12345";
	FString LocalToken = "fake_token";
	FOnlineAccountCredentials LocalAccount = FOnlineAccountCredentials(LocalType, LocalId, LocalToken);
	CHECK(&LocalAccount != nullptr);
}

ACCOUNTSERVICE_TEST_CASE("Verify if we can properly instantiate the OSS")
{
	GetPipeline()
		.EmplaceLambda([](IOnlineSubsystem* Services)
		{
			int32 LocalUserNum = 0;
			FString LocalType = "test_account";
			FString LocalId = "12345";
			FString LocalToken = "fake_token";
			FOnlineAccountCredentials LocalAccount = FOnlineAccountCredentials(LocalType, LocalId, LocalToken);

			IOnlineIdentityPtr IdentityInterface = Services->GetIdentityInterface();		
			bool LoggedIn = IdentityInterface.Get()->Login(LocalUserNum, LocalAccount);
			REQUIRE(LoggedIn);
		});

	RunToCompletion();
}