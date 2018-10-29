// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSafeWriteSession.h"
#include "Interfaces/OnlineSessionInterface.h"

class FOnlineSubsystemLive;

/** 
 * Async task used to have every local player Leave() the Live session.
 */
class FOnlineAsyncTaskLiveDestroySessionBase : public FOnlineAsyncTaskLiveSafeWriteSession
{
public:
	FOnlineAsyncTaskLiveDestroySessionBase(
			FName InSessionName,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem);
	
	// FOnlineAsyncItem
	virtual FString ToString() const override {
		const bool WasSuccessful = bWasSuccessful;
		return FString::Printf(TEXT("FOnlineAsyncTaskLiveDestroySessionBase SessionName: %s bWasSuccessful: %d"), *GetSessionName().ToString(), WasSuccessful); }

	/** Triggers the appropriate delegate based on DelegateType */
	static void RemoveAndCleanupSession(
		FOnlineSubsystemLive* Subsystem,
		FName SessionName);

private:

	// FOnlineAsyncTaskLiveSafeWriteSession
	virtual bool UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

};

class FOnlineAsyncTaskLiveDestroyMatchmakingSession : public FOnlineAsyncTaskLiveDestroySessionBase
{
public:
	FOnlineAsyncTaskLiveDestroyMatchmakingSession(
		FName InSessionName,
		Microsoft::Xbox::Services::XboxLiveContext^ InContext,
		FOnlineSubsystemLive* InSubsystem) :
		FOnlineAsyncTaskLiveDestroySessionBase(InSessionName, InContext, InSubsystem)
	{}

	// FOnlineAsyncItem
	virtual FString ToString() const override {
		const bool WasSuccessful = bWasSuccessful;
		return FString::Printf(TEXT("FOnlineAsyncTaskLiveDestroyMatchmakingSession SessionName: %s bWasSuccessful: %d"), *GetSessionName().ToString(), WasSuccessful); }
	virtual void Finalize() override;

};

class FOnlineAsyncTaskLiveDestroySession : public FOnlineAsyncTaskLiveDestroySessionBase
{
public:
	FOnlineAsyncTaskLiveDestroySession(
		FName InSessionName,
		Microsoft::Xbox::Services::XboxLiveContext^ InContext,
		FOnlineSubsystemLive* InSubsystem,
		const FOnDestroySessionCompleteDelegate& InCompletionDelegate) :
		FOnlineAsyncTaskLiveDestroySessionBase(InSessionName, InContext, InSubsystem),
		CompletionDelegate(InCompletionDelegate)
	{}

	// FOnlineAsyncItem
	virtual FString ToString() const override {
		const bool WasSuccessful = bWasSuccessful;
		return FString::Printf(TEXT("FOnlineAsyncTaskLiveDestroySession SessionName: %s bWasSuccessful: %d"), *GetSessionName().ToString(), WasSuccessful); }
	virtual void Finalize() override;

private:
	FOnDestroySessionCompleteDelegate CompletionDelegate;
};

void CreateDestroyMatchmakingCompleteTask(FName SessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session,
	FOnlineSubsystemLive* Subsystem,
	bool bWasSuccessful);

void CreateDestroyTask(FName SessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session,
	FOnlineSubsystemLive* Subsystem,
	bool bWasSuccessful,
	const FOnDestroySessionCompleteDelegate& CompletionDelegate);
