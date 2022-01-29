// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformCrashContext.h"

#include "AllowWindowsPlatformTypes.h"
#include "DbgHelp.h"
#include "HideWindowsPlatformTypes.h"

#if WER_CUSTOM_REPORTS
struct CORE_API FUWPPlatformCrashContext : public FGenericCrashContext
{
	/** Platform specific constants. */
	enum EConstants
	{
		UE4_MINIDUMP_CRASHCONTEXT = LastReservedStream + 1,
	};

	virtual void AddPlatformSpecificProperties() override
	{
		AddCrashProperty( TEXT( "Platform.IsRunningUWP" ), 1 );
	}
};

typedef FUWPPlatformCrashContext FPlatformCrashContext;

#endif