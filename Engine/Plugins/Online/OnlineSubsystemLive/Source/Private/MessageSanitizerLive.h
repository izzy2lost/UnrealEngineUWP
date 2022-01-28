// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IMessageSanitizerInterface.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineUserInterfaceLive.h"
#include "OnlineSubsystemLivePackage.h"

class FOnlineSubsystemLive;

/**
 * Implements the Live specific interface chat message sanitization
 */
class FMessageSanitizerLive :
	public IMessageSanitizer
{

public:

	// IMessageSanitizer
	virtual void SanitizeDisplayName(const FString& DisplayName, const FOnMessageProcessed& CompletionDelegate) override;
	virtual void SanitizeDisplayNames(const TArray<FString>& DisplayNames, const FOnMessageArrayProcessed& CompletionDelegate) override;
	virtual void QueryBlockedUser(int32 LocalUserNum, const FString& FromUserIdStr, const FOnQueryUserBlockedResponse& InCompletionDelegate) override;
	virtual void ResetBlockedUserCache() override;
	// FMessageSanitizerLive

	explicit FMessageSanitizerLive(FOnlineSubsystemLive* InLiveSubsystem);
	virtual ~FMessageSanitizerLive();

private:

	void HandleAppResume();
	void HandleAppReactivated();
	void OnQueryBlockedUserComplete(const FOnlineError& RequestStatus, const TSharedRef<const FUniqueNetId>& RequestingUser, const FCommunicationPermissionResultsMap& Results, FOnQueryUserBlockedResponse CompletionDelegate);

	FDelegateHandle AppResumeDelegateHandle;
	FDelegateHandle AppReactivatedDelegateHandle;

	/** Reference to the main Live subsystem */
	FOnlineSubsystemLive* LiveSubsystem;

	struct FRemoteUserBlockInfo
	{
		FRemoteUserBlockInfo()
			: State(EOnlineAsyncTaskState::NotStarted)
			, bIsBlocked(false)
		{ }

		/** Remote user id */
		FString RemoteUserId;

		/** State of the query */
		EOnlineAsyncTaskState::Type State;

		/** Is this user blocked, valid only if bIsComplete is true */
		bool bIsBlocked;

		/** Delegates to fire when the query is complete, should only be filled while bIsComplete is false */
		TArray<FOnQueryUserBlockedResponse> Delegates;
	};

	struct FBlockedUserData
	{
		/** Local user id */
		FString LocalUserId;
		/** Mapping from remote users to their cached info */
		TMap<FString, FRemoteUserBlockInfo> RemoteUserData;
	};

	/** Mapping of all local users to the block data for remote users */
	TMap<FString, FBlockedUserData> UserBlockData;
};

typedef TSharedPtr<FMessageSanitizerLive, ESPMode::ThreadSafe> FMessageSanitizerLivePtr;
