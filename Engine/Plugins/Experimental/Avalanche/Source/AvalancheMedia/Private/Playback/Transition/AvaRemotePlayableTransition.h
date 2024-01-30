// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playback/Transition/AvalanchePlayableTransition.h"

#include "AvaRemotePlayableTransition.generated.h"

class IAvaMediaPlaybackClient;
namespace UE::AvaMediaPlaybackClient::Delegates
{
	struct FPlaybackTransitionEventArgs;
}

UCLASS()
class UAvaRemotePlayableTransition : public UAvalanchePlayableTransition
{
	GENERATED_BODY()
	
public:
	virtual ~UAvaRemotePlayableTransition() override;

	void SetChannelName(const FName& InChannelName) { ChannelName = InChannelName; }
	
	//~ Begin UAvalanchePlayableTransition
	virtual bool Start() override;
	virtual void Stop() override;
	virtual bool IsRunning() const override;
	//~ End UAvalanchePlayableTransition

protected:
	void HandlePlaybackTransitionEvent(IAvaMediaPlaybackClient& InPlaybackClient,
		const UE::AvaMediaPlaybackClient::Delegates::FPlaybackTransitionEventArgs& InArgs);

	void RegisterToPlaybackClientDelegates();
	void UnregisterFromPlaybackClientDelegates() const;

protected:
	FGuid TransitionId;
	FName ChannelName;
};