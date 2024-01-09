// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "HAL/Thread.h"
#include "UbaHordeAgent.h"

class FEvent;
namespace uba { class NetworkServer; }

class FUbaHordeAgentManager
{
public:
	FUbaHordeAgentManager(uba::NetworkServer* InServer);
	~FUbaHordeAgentManager();

	void SetTargetCoreCount(uint32 Count);

private:
	void RequestAgent();
	void ParseConfig();

	struct FHordeAgentWrapper
	{
		FThread Thread;
		FEvent* ShouldExit;
	};

	uba::NetworkServer* UbaServer;

	FString Url;
	FString Pool;
	FString Oidc;
	uint32 MaxCores = 0;

	TUniquePtr<FUbaHordeMetaClient> HordeMetaClient;

	FCriticalSection UbaAgentBundleFilePathLock;
	FString UbaAgentBundleFilePath;

	FCriticalSection AgentsLock;
	TArray<TUniquePtr<FHordeAgentWrapper>> Agents;

	TAtomic<uint64> LastRequestFailTime;
	TAtomic<uint32> TargetCoreCount;
	TAtomic<uint32> EstimatedCoreCount;
	TAtomic<bool> AskForAgents;
};
