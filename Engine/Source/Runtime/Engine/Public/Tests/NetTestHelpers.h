// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkObjectList.h"

class APlayerController;
class IConsoleVariable;
class UGameInstance;
class UWorld;

struct FGameInstancePIEParameters;

#if WITH_EDITOR

namespace UE::Net
{

/**
 * Properly scoped/RAII wrapper around a GameInstance/WorldContext/World that makes it easier to write tests
 * involving full UWorld functionality within the scope of one function.
 */
struct FTestWorldInstance
{
	ENGINE_API static FTestWorldInstance CreateServer(const TCHAR* InURL);
	ENGINE_API static FTestWorldInstance CreateClient(int32 ServerPort);

	ENGINE_API ~FTestWorldInstance();

	FTestWorldInstance(const FTestWorldInstance&) = delete;
	FTestWorldInstance& operator=(const FTestWorldInstance&) = delete;

	explicit FTestWorldInstance(bool bDelayedInit) : GameInstance(nullptr) {}
	
	ENGINE_API FTestWorldInstance(FTestWorldInstance&& Other);
	ENGINE_API FTestWorldInstance& operator=(FTestWorldInstance&& Other);
	
	ENGINE_API UWorld* GetWorld() const;
	ENGINE_API FWorldContext* GetWorldContext() const;
	ENGINE_API UNetDriver* GetNetDriver() const;

	ENGINE_API int32 GetPort();

	ENGINE_API void Tick(float DeltaSeconds = 0.0166f);

	ENGINE_API void LoadStreamingLevel(FName LevelName);
	ENGINE_API void UnloadStreamingLevel(FName LevelName);

public:

	UGameInstance* GameInstance = nullptr;

private:
	explicit FTestWorldInstance(const FGameInstancePIEParameters& InstanceParams);

	void Shutdown();

	static int32 FindUnusedPIEInstance();

	int32 LevelStreamRequestUUID = 0;
};

/**
 * Stores FTestWorldInstances for a server and clients and allows synchronously ticking them.
 * Can be used within a single function to make automated tests that use the whole world & net driver flow.
 */
struct FTestWorlds
{
	/** Creates a server world using the given URL. */
	ENGINE_API explicit FTestWorlds(const TCHAR* ServerURL, float DeltaSeconds = 0.0166f);
	ENGINE_API ~FTestWorlds();

	ENGINE_API bool CreateAndConnectClient();

	/** Ticks all server & client worlds NumTick times synchronously. */
	ENGINE_API void TickAll(int32 NumTicks=1);
	ENGINE_API void TickServer();
	ENGINE_API void TickClients();

	/** Tick the world and drop all outgoing packets */
	ENGINE_API void TickServerAndDrop();
	ENGINE_API void TickClientsAndDrop();

	/** Tick the world but delay the packets that would be sent */
	ENGINE_API void TickServerAndDelay(uint32 NumFramesToDelay = 1);
	ENGINE_API void TickClientsAndDelay(uint32 NumFramesToDelay = 1);

	/**
	 * Ticks all server & client worlds until Predicate returns true, or MaxTicks is reached.
	 * Returns true if Predicate did, false if it didn't within MaxTicks.
	 */
	template<class PredicateT>
	bool TickAllUntil(const PredicateT& Predicate, float DeltaSeconds = 0.0166f, int32 MaxTicks = 60);

	/** Ticks all server & client worlds until the passed in client world has a valid client PlayerController. */
	ENGINE_API bool WaitForClientConnect(FTestWorldInstance& Client);

	/** Return the Server's player state corresponding to a specific client */
	ENGINE_API APlayerController* GetServerPlayerControllerOfClient(uint32 ClientIndex);

public:

	/** Server and Client Worlds */
	FTestWorldInstance Server;
	TArray<FTestWorldInstance> Clients;

private:

	void OnNetDriverCreated(UWorld* InWorld, UNetDriver* InNetDriver);
	FDelegateHandle NetDriverCreatedHandle;

	float TickDeltaSeconds = 0.0166f;
};

//------------------------------------------------------------------------
// Inline functions
//------------------------------------------------------------------------

template<class PredicateT>
inline bool FTestWorlds::TickAllUntil(const PredicateT& Predicate, float DeltaSeconds, int32 MaxTicks)
{
	int32 TickCount = 0;
	bool bPredicateResult = Predicate();
		
	while (!bPredicateResult && TickCount < MaxTicks)
	{
		Server.Tick(DeltaSeconds);
		for (FTestWorldInstance& Client : Clients)
		{
			Client.Tick(DeltaSeconds);
		}
		TickCount++;
		GFrameCounter++;
		bPredicateResult = Predicate();
	}

	return bPredicateResult;
}

//------------------------------------------------------------------------
// FScopedCVarOverrideInt
//------------------------------------------------------------------------
class FScopedCVarOverrideInt
{
public:
	FScopedCVarOverrideInt(const TCHAR* VariableName, int32 Value);
	~FScopedCVarOverrideInt();

	FScopedCVarOverrideInt(FScopedCVarOverrideInt&&) = delete;
	FScopedCVarOverrideInt(const FScopedCVarOverrideInt&) = delete;
	FScopedCVarOverrideInt& operator=(FScopedCVarOverrideInt&&) = delete;
	FScopedCVarOverrideInt& operator=(const FScopedCVarOverrideInt&) = delete;

private:
	IConsoleVariable* Variable = nullptr;
	int32 SavedValue = 0;
};

/**
 * Sets and restores globals and cvars needed to use FNetTestWorldInstances within a scope.
 * Meant to be used within a single function.
 */
class FScopedTestSettings
{
public:
	FScopedTestSettings();
	~FScopedTestSettings();

	FScopedTestSettings(FScopedTestSettings&&) = delete;
	FScopedTestSettings(const FScopedTestSettings&) = delete;
	FScopedTestSettings& operator=(FScopedTestSettings&&) = delete;
	FScopedTestSettings& operator=(const FScopedTestSettings&) = delete;

private:
	FScopedCVarOverrideInt AddressResolutionDisabled;
	FScopedCVarOverrideInt BandwidthThrottlingDisabled;
	FScopedCVarOverrideInt RepGraphBandwidthThrottlingDisabled;
	FScopedCVarOverrideInt RandomNetUpdateDelayDisabled;
	FScopedCVarOverrideInt GameplayDebuggerDisabled;

	UWorld* OldGWorld;
	int32 OldPIEID;
	bool OldGIsPlayInEditorWorld;
};

} // end namespace UE::Net

#endif //WITH_EDITOR