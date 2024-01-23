// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineBeaconUnitTestClient.h"
#include "OnlineBeaconUnitTestUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineBeaconUnitTestClient)

AOnlineBeaconUnitTestClient::AOnlineBeaconUnitTestClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

void AOnlineBeaconUnitTestClient::OnConnected()
{
	if (BeaconUnitTest::FTestStats* TestStats = BeaconUnitTest::FTestPrerequisites::GetActiveTestStats())
	{
		++TestStats->Client.OnConnected.InvokeCount;
	}

	Super::OnConnected();


	if (const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig())
	{
		if (TestConfig->Client.OnConnected.Callback)
		{
			TestConfig->Client.OnConnected.Callback();
		}
	}
}

void AOnlineBeaconUnitTestClient::OnFailure()
{
	if (BeaconUnitTest::FTestStats* TestStats = BeaconUnitTest::FTestPrerequisites::GetActiveTestStats())
	{
		++TestStats->Client.OnFailure.InvokeCount;
	}

	Super::OnFailure();
}
