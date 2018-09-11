// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

// @ATG_CHANGE : BEGIN - allow more platforms to use AES-GCM
#if PLATFORM_XBOXONE
#include "XboxOneAllowPlatformTypes.h"
#else
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <bcrypt.h>

#if PLATFORM_XBOXONE
#include "XboxOneHidePlatformTypes.h"
#else
#include "Windows/HideWindowsPlatformTypes.h"
#endif
// @ATG_CHANGE : END

DECLARE_LOG_CATEGORY_EXTERN(LogPlatformCryptoBCrypt, Warning, All);

/**
 * Module interface for BCrypt/CNG cryptographic functionality. Users should go through the FEncryptionContext API.
 */
class FPlatformCryptoBCryptModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FPlatformCryptoBCryptModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FPlatformCryptoBCryptModule >( "PlatformCryptoBCrypt" );
	}

	/** Returns the cached provider of the given name, or attempts to open one and cache it if it's not cached already. */
	BCRYPT_ALG_HANDLE GetOrOpenProvider(const FString& ProviderName);

private:
	// Cache providers since opening them can be expensive
	TMap<FString, BCRYPT_ALG_HANDLE> ProviderCache;
};
