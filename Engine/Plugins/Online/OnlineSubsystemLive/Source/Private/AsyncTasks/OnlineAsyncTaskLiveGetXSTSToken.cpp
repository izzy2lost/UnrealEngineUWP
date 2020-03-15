// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveGetXSTSToken.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

FOnlineAsyncTaskLiveGetXSTSToken::FOnlineAsyncTaskLiveGetXSTSToken(
	FOnlineSubsystemLive* const InLiveSubsystem,
	Windows::Xbox::System::User^ InLocalUser,
	int32 InLocalUserNum,
	const FString& InEndPointURL,
	const FOnXSTSTokenCompleteDelegate& CompletionDelegate
)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLocalUser)
	, LocalUserNum(InLocalUserNum)
	, RequestEndPointURL(InEndPointURL)
	, TaskCompletionDelegate(CompletionDelegate)
	, LiveUserId(InLocalUser->XboxUserId)
{
}

Windows::Foundation::IAsyncOperation<GetTokenAndSignatureResult^>^ FOnlineAsyncTaskLiveGetXSTSToken::CreateOperation()
{
	try
	{
		auto GetTokenOperation = LocalUser->GetTokenAndSignatureAsync(
			ref new Platform::String(TEXT("GET")),			// HTTP method for the token
			ref new Platform::String(*RequestEndPointURL),	// URL for the token
			ref new Platform::String()						// Headers
		);

		return GetTokenOperation;
	}
	catch (Platform::COMException^ Ex)
	{
		UE_LOG(LogOnline, Warning, TEXT("FOnlineAsyncTaskLiveGetXSTSToken::GetTokenAndSignatureAsync: Failed to get token. HResult = 0x%0.8X"), Ex->HResult);
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveGetXSTSToken::ProcessResult(const Concurrency::task<GetTokenAndSignatureResult^>& CompletedTask)
{
	try
	{
		GetTokenAndSignatureResult^ Result = CompletedTask.get();

		Platform::String^ TokenRaw = Result->Token;
		ResultToken = TokenRaw->Data();

		Platform::String^ SignatureRaw = Result->Signature;
		ResultSignature = SignatureRaw->Data();
	}
	catch (Platform::InvalidArgumentException^ Ex)
	{
		UE_LOG(LogOnline, Error, TEXT("GetTokenAndSignatureAsync failed due to invalid argument with 0x%0.8X. Was the Login Endpoint setup correctly?"), Ex->HResult);
		return false;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG(LogOnline, Error, TEXT("GetTokenAndSignatureAsync failed with 0x%0.8X"), Ex->HResult);
		return false;
	}

	return true;
}

void FOnlineAsyncTaskLiveGetXSTSToken::Finalize()
{
	if (bWasSuccessful)
	{
		FOnlineIdentityLivePtr LiveIdentity = Subsystem->GetIdentityLive();
		// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
		LiveIdentity->SetUserXSTSToken(LocalUser, RequestEndPointURL, ResultToken);
		// @ATG_CHANGE : END
	}
}

void FOnlineAsyncTaskLiveGetXSTSToken::TriggerDelegates()
{
	TaskCompletionDelegate.ExecuteIfBound(FOnlineError(bWasSuccessful), LocalUserNum, LiveUserId, ResultSignature, ResultToken);
}
