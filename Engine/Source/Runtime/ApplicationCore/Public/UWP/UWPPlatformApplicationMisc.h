#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FUWPPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static class GenericApplication* CreateApplication();
	static void PumpMessages(bool bFromMainLoop);
};

typedef FUWPPlatformApplicationMisc FPlatformApplicationMisc;
