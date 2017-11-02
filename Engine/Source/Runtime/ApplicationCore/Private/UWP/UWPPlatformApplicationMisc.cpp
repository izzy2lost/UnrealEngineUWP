#include "UWPPlatformApplicationMisc.h"
#include "UWPApplication.h"
#include "Misc/App.h"

GenericApplication* FUWPPlatformApplicationMisc::CreateApplication()
{
	return FUWPApplication::CreateUWPApplication();
}

// Defined in UWPLaunch.cpp
extern void appWinPumpMessages();

void FUWPPlatformApplicationMisc::PumpMessages(bool bFromMainLoop)
{
	if (!bFromMainLoop)
	{
		return;
	}

	GPumpingMessagesOutsideOfMainLoop = false;

	appWinPumpMessages();

	bool HasFocus = true;
	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier(HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier());
}