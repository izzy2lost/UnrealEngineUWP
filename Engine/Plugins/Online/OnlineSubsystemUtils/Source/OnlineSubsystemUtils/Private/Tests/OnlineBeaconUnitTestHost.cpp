// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineBeaconUnitTestHost.h"

#include "Online/CoreOnline.h"
#include "Tests/OnlineBeaconUnitTestUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineBeaconUnitTestHost)

AOnlineBeaconUnitTestHost::AOnlineBeaconUnitTestHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

bool AOnlineBeaconUnitTestHost::StartVerifyAuthentication(const FUniqueNetId& PlayerId, const FString& AuthenticationToken)
{
	const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
	if (TestConfig == nullptr)
	{
		return false;
	}

	if (!TestConfig->Auth.Method1.bEnabled)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Super::StartVerifyAuthentication(PlayerId, AuthenticationToken);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	if (TestConfig->Auth.Method1.bDelayDelegate)
	{
		return BeaconUnitTest::SetTimerForNextFrame(GetWorld(), GFrameCounter, [this, PlayerIdRef = PlayerId.AsShared()]()
		{
			const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			OnAuthenticationVerificationComplete(*PlayerIdRef, TestConfig ? TestConfig->Auth.Method1.Result : FOnlineError());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		});
	}
	else
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		OnAuthenticationVerificationComplete(PlayerId, TestConfig->Auth.Method1.Result);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return true;
	}
}

bool AOnlineBeaconUnitTestHost::StartVerifyAuthentication(const FUniqueNetId& PlayerId, const FString& AuthenticationToken, const FOnAuthenticationVerificationCompleteDelegate& OnComplete)
{
	const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
	if (TestConfig == nullptr)
	{
		return false;
	}

	if (!TestConfig->Auth.Method2.bEnabled)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Super::StartVerifyAuthentication(PlayerId, AuthenticationToken, OnComplete);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	if (TestConfig->Auth.Method2.bDelayDelegate)
	{
		return BeaconUnitTest::SetTimerForNextFrame(GetWorld(), GFrameCounter, [this, OnComplete]()
		{
			const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
			OnComplete.ExecuteIfBound(TestConfig ? TestConfig->Auth.Method2.Result : FOnlineError());
		});
	}
	else
	{
		OnComplete.ExecuteIfBound(TestConfig->Auth.Method2.Result);
		return true;
	}
}

bool AOnlineBeaconUnitTestHost::StartVerifyAuthentication(const FUniqueNetId& PlayerId, const FString& LoginOptions, const FString& AuthenticationToken, const FOnAuthenticationVerificationCompleteDelegate& OnComplete)
{
	const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
	if (TestConfig == nullptr)
	{
		return false;
	}

	if (!TestConfig->Auth.Method3.bEnabled)
	{
		return Super::StartVerifyAuthentication(PlayerId, LoginOptions, AuthenticationToken, OnComplete);
	}

	if (TestConfig->Auth.Method3.bDelayDelegate)
	{
		return BeaconUnitTest::SetTimerForNextFrame(GetWorld(), GFrameCounter, [this, OnComplete]()
		{
			const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
			OnComplete.ExecuteIfBound(TestConfig ? TestConfig->Auth.Method3.Result : FOnlineError());
		});
	}
	else
	{
		OnComplete.ExecuteIfBound(TestConfig->Auth.Method3.Result);
		return true;
	}
}

bool AOnlineBeaconUnitTestHost::VerifyJoinForBeaconType(const FUniqueNetId& PlayerId, const FString& BeaconType)
{
	const BeaconUnitTest::FTestConfig* TestConfig = BeaconUnitTest::FTestPrerequisites::GetActiveTestConfig();
	if (TestConfig == nullptr)
	{
		return false;
	}

	if (!TestConfig->Auth.Verify.bEnabled)
	{
		return Super::VerifyJoinForBeaconType(PlayerId, BeaconType);
	}

	return TestConfig->Auth.Verify.bResult;
}

void AOnlineBeaconUnitTestHost::OnFailure()
{
	if (BeaconUnitTest::FTestStats* TestStats = BeaconUnitTest::FTestPrerequisites::GetActiveTestStats())
	{
		++TestStats->Host.OnFailure.InvokeCount;
	}

	Super::OnFailure();
}

#endif /* WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR */
