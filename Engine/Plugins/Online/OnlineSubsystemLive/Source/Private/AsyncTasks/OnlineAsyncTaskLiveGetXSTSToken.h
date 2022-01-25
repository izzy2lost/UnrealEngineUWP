// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "OnlineError.h"

using Windows::Xbox::System::GetTokenAndSignatureResult;
using Windows::Xbox::System::User;

/**
* Delegate for after receiving an XSTS auth token.
*/
DECLARE_DELEGATE_FiveParams(FOnXSTSTokenCompleteDelegate, const FOnlineError& /*Result*/, int32 /*LocalUserNum*/, const FUniqueNetId& /*UserId*/, const FString& /*ResultSignature*/, const FString& /*ResultToken*/);

class FOnlineAsyncTaskLiveGetXSTSToken
	: public FOnlineAsyncTaskConcurrencyLive<GetTokenAndSignatureResult^>
{
public:
	FOnlineAsyncTaskLiveGetXSTSToken(
		class FOnlineSubsystemLive* const InLiveSubsystem,
		User^ InLocalUser,
		int32 InLocalUserNum,
		const FString& InEndPointURL,
		const FOnXSTSTokenCompleteDelegate& CompletionDelegate);

	virtual ~FOnlineAsyncTaskLiveGetXSTSToken() = default;

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveGetXSTSToken"); }

	virtual Windows::Foundation::IAsyncOperation<GetTokenAndSignatureResult^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<GetTokenAndSignatureResult^>& CompletedTask) override;

	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	int32 LocalUserNum;
	FString RequestEndPointURL;
	FOnXSTSTokenCompleteDelegate TaskCompletionDelegate;

	FUniqueNetIdLive LiveUserId;
	FString ResultToken;
	FString ResultSignature;
};