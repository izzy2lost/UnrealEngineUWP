// Copyright Microsoft Inc. All Rights Reserved.

#include "PlatformFeatures.h"
#include "UWPSaveGameSystem.h"
#include "OnlineSubsystem.h"

class FUWPPlatformFeatures : public IPlatformFeaturesModule
{
public:
	virtual class ISaveGameSystem* GetSaveGameSystem() override
	{
		// Only use UWP's connected storage save system if we're also using Xbox Live.
		if (IOnlineSubsystem::DoesInstanceExist(LIVE_SUBSYSTEM))
		{
			static FUWPSaveGameSystem UWPSaveGameSystem;
			return &UWPSaveGameSystem;
		}

		UE_LOG(LogUWPSaveGame, Error, TEXT("Xbox Live support is not enabled.  UWP ConnectedStorage save system requires Xbox Live."));
		return IPlatformFeaturesModule::GetSaveGameSystem();
	}
};

IMPLEMENT_MODULE(FUWPPlatformFeatures, UWPPlatformFeatures);
