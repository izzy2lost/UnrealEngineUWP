// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "HAL/Thread.h"
#include "UbaBase.h"

class FEvent;
class FUbaHordeMetaClient;
namespace uba { class NetworkServer; }

class FUbaHordeAgentManager
{
public:
	UBACOORDINATORHORDE_API FUbaHordeAgentManager(const FString& InWorkingDir, const FString& BinariesPath);
	UBACOORDINATORHORDE_API ~FUbaHordeAgentManager();

	inline void SetUrl(const FString& InUrl) { Url = InUrl; }
	inline void SetPool(const FString& InPool) { Pool = InPool; }
	inline void SetOidc(const FString& InOidc) { Oidc = InOidc; }
	inline void SetMaxCoreCount(uint32 Count) { MaxCores = Count; }

	UBACOORDINATORHORDE_API void SetTargetCoreCount(uint32 Count);

	using AddClientCallback = bool(void* userData, const uba::tchar* ip, uint16 port);
	UBACOORDINATORHORDE_API void SetAddClientCallback(AddClientCallback* callback, void* userData);

	// Returns the number of agents currently handled by this agent manager.
	UBACOORDINATORHORDE_API int32 GetAgentCount() const;

	// Returns the active number of cores allocated across all agents.
	UBACOORDINATORHORDE_API uint32 GetActiveCoreCount() const;

private:
	struct FHordeAgentWrapper
	{
		FThread Thread;
		FEvent* ShouldExit;
	};

	void RequestAgent();
	void ThreadAgent(FHordeAgentWrapper& Wrapper);

	FString WorkingDir;
	FString BinariesPath;

	FString Url;
	FString Pool;
	FString Oidc;
	uint32 MaxCores = 0;

	TUniquePtr<FUbaHordeMetaClient> HordeMetaClient;

	FCriticalSection BundleRefPathsLock;
	TArray<FString> BundleRefPaths;

	mutable FCriticalSection AgentsLock;
	TArray<TUniquePtr<FHordeAgentWrapper>> Agents;

	TAtomic<uint64> LastRequestFailTime;
	TAtomic<uint32> TargetCoreCount;
	TAtomic<uint32> EstimatedCoreCount;
	TAtomic<uint32> ActiveCoreCount;
	TAtomic<bool> AskForAgents;

	AddClientCallback* m_callback = nullptr;
	void* m_userData;

	FUbaHordeAgentManager(const FUbaHordeAgentManager&) = delete;
	void operator=(const FUbaHordeAgentManager&) = delete;
};
