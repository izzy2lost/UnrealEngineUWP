// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"

class FOnlineAsyncTaskLiveSessionChanged : public FOnlineAsyncTaskLive
{
public:
	// This constructor should only be called on the game thread.
	FOnlineAsyncTaskLiveSessionChanged(
		class FOnlineSubsystemLive* InLiveSubsystem,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionName,
		FString InChangeBranch,
		uint64 InChangeNumber);

	virtual ~FOnlineAsyncTaskLiveSessionChanged() = default;

	void			OnFailed();
	virtual void	Initialize() override;

	virtual FString ToString() const override { return TEXT("SessionChanged");}
	virtual void	Finalize() override;
	virtual void	TriggerDelegates() override;

private:
	FString ChangeBranch;
	uint64 ChangeNumber;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionReference;
	
	FName SessionName;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ CachedLiveSession;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ UpdatedLiveSession;

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff;
	bool bShouldTriggerDelegates;
		
	FName GetSessionNameForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef);
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ GetLiveSessionRefForSessionName(const FName& SessionName);
};
