// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#if GAME_INPUT_SUPPORT
struct IGameInput;
#endif

class GAMEINPUTBASE_API FGameInputBaseModule : public IModuleInterface
{
public:

	static FGameInputBaseModule& Get();

	/** Returns true if this module is loaded (aka available) by the FModuleManager */
	static bool IsAvailable();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

#if GAME_INPUT_SUPPORT
	/** 
	* Pointer to the static IGameInput that is created upon module startup.
	*/
	static IGameInput* GetGameInput();
#endif

protected:

	void InitializeGameInputKeys();
};