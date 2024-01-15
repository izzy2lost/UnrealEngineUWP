// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include "CoreFwd.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"
#include "Trace/StoreClient.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FMenuBuilder;

class TRACEINSIGHTS_API STraceServerControl
{
public:
	STraceServerControl(const TCHAR* Host, uint32 Port = 0, FName StyleSet = NAME_None);
	~STraceServerControl() = default;

	void MakeMenu(FMenuBuilder& Builder);

private:
	enum class EState : uint8
	{
		NotConnected,
		Connecting,
		Connected,
		CheckStatus,
		Command
	};

	bool ChangeState(EState Expected, EState ChangeTo, uint32 Attempts = 1);

	void TriggerStatusUpdate();
	void UpdateStatus();
	void ResetStatus();

	bool CanServerBeStarted() const { return bIsLocalHost && State.load(std::memory_order_relaxed) == EState::NotConnected; }
	bool CanServerBeStopped() const { return bIsLocalHost && State.load(std::memory_order_relaxed) == EState::Connected; }
	bool AreControlsEnabled() const { return bIsLocalHost && State.load(std::memory_order_relaxed) == EState::Connected; }
	bool IsSponsored() const { return bSponsored.load(std::memory_order_relaxed); }

	void OnStart_Clicked();
	void OnStop_Clicked();
	void OnSponsored_Changed();

	std::atomic<EState> State = EState::NotConnected;

	std::atomic<bool> bCanServerBeStarted = false;
	std::atomic<bool> bCanServerBeStopped = false;
	std::atomic<bool> bSponsored = false;
	FCriticalSection StringsLock;
	FString StatusString;

	FString Host;
	uint32 Port;
	FName StyleSet;
	bool bIsLocalHost;
	TUniquePtr<UE::Trace::FStoreClient> Client;
	
	friend const TCHAR* LexState(EState);
};
