// Copyright Microsoft Inc. All Rights Reserved.
#pragma once

#include <functional>
#include "SaveGameSystem.h"

// Async save/load support not enabled in this branch
#define HAS_ASYNC_SAVE_LOAD_SUPPORT 0

DECLARE_LOG_CATEGORY_EXTERN(LogUWPSaveGame, Log, All);

class FUWPSaveGameSystem : public ISaveGameSystem
{
public:
	FUWPSaveGameSystem() {}
	virtual ~FUWPSaveGameSystem() {}


	/** Returns true if the platform has a native UI (like many consoles) */
	virtual bool PlatformHasNativeUI() override
	{
		return false; // UWP does not have native UI that lets a user choose a save game to load or a location to save to
	}

	/** Return true if the named savegame exists */
	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) override
	{
		return ESaveExistsResult::OK == DoesSaveGameExistWithResult(Name, UserIndex);
	}

	/** Return ESaveExistsResult::OK if the named savegame exists, an error code otherwise. */
	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex) override;

	/** Saves the game, blocking until complete. Platform may use FGameDelegates to get more information from the game */
	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) override;

	/** Loads the game, blocking until complete */
	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) override;

	/** Delete an existing save game, blocking until complete */
	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) override;
};
