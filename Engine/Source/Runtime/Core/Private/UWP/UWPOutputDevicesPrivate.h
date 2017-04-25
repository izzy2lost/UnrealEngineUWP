// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UWPOutputDevicesPrivate.h: UWP implementations of output devices
=============================================================================*/

#pragma once
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceConsole.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUWPOutputDevices, Log, All);

PACK_WINRT()

/**
* Output device that writes to UWP Event Log
*/
class FOutputDeviceEventLog :
	public FOutputDevice
{
	Windows::Foundation::Diagnostics::LoggingChannel^ EtwLogChannel;

public:
	/**
	* Constructor, initializing member variables
	*/
	FOutputDeviceEventLog()
	{
#if !UE_BUILD_SHIPPING
		// In normal startup flow InitializeSession is called later.  But it's useful to identify the provider using a GUID
		// that can be controlled by an external application, and session id is ideal for this purpose.  Luckily it doesn't
		// really hurt to call InitializeSession twice.
		FApp::InitializeSession();
#endif
		Platform::Guid PlatformGuid;
		if (FApp::IsStandalone())
		{
			static const Platform::Guid MicrosoftWindowsDiagnoticsLoggingChannelId(0x4bd2826e, 0x54a1, 0x4ba9, 0xbf, 0x63, 0x92, 0xb7, 0x3e, 0xa1, 0xac, 0x4a);
			PlatformGuid = MicrosoftWindowsDiagnoticsLoggingChannelId;
		}
		else
		{
			FGuid ProviderId = FApp::GetSessionId();

			// Memory layout of UE FGuid and Windows GUID is potentially different, so convert carefully to make sure that the
			// provider GUID gets the value that the external driver expects.
			PlatformGuid = Platform::Guid(ProviderId.A,
				ProviderId.B >> 16, ProviderId.B & 0xffff,
				ProviderId.C >> 24, (ProviderId.C >> 16) & 0xff, (ProviderId.C >> 8) & 0xff, ProviderId.C & 0xff,
				ProviderId.D >> 24, (ProviderId.D >> 16) & 0xff, (ProviderId.D >> 8) & 0xff, ProviderId.D & 0xff);
		}
		EtwLogChannel = ref new Windows::Foundation::Diagnostics::LoggingChannel(ref new Platform::String(FApp::GetGameName()), nullptr, PlatformGuid);
	}

	/** Destructor that cleans up any remaining resources */
	virtual ~FOutputDeviceEventLog()
	{
		TearDown();
	}

	virtual void Serialize(const TCHAR* Buffer, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (EtwLogChannel != nullptr)
		{
			Windows::Foundation::Diagnostics::LoggingLevel LogWithLevel = GetWindowsLoggingLevelFromUEVerbosity(Verbosity);
#if NO_LOGGING
			if (LogWithLevel >= Windows::Foundation::Diagnostics::LoggingLevel::Warning)
#endif
			{
				EtwLogChannel->LogMessage(ref new Platform::String(Buffer), LogWithLevel);
			}
		}
	}

	Windows::Foundation::Diagnostics::LoggingLevel GetWindowsLoggingLevelFromUEVerbosity(ELogVerbosity::Type Verbosity)
	{
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
			return Windows::Foundation::Diagnostics::LoggingLevel::Error;

		case ELogVerbosity::Warning:
			return Windows::Foundation::Diagnostics::LoggingLevel::Warning;

		case ELogVerbosity::Fatal:
			return Windows::Foundation::Diagnostics::LoggingLevel::Critical;

		case ELogVerbosity::Display:
		case ELogVerbosity::Log:
			return Windows::Foundation::Diagnostics::LoggingLevel::Information;

		case ELogVerbosity::Verbose:
		case ELogVerbosity::VeryVerbose:
		default:
			return Windows::Foundation::Diagnostics::LoggingLevel::Verbose;
		}
	}

	/** Does nothing */
	virtual void Flush(void)
	{
	}

	/**
	* Closes any event log handles that are open
	*/
	virtual void TearDown(void)
	{
	}
};

PACK_WINRT_REVERT()

class FOutputDeviceUWPError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceUWPError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError();

private:
	int32		ErrorPos;
};

/**
 * UWP implementation of console log window, utilizing the Win32 console API
 */
class CORE_API FOutputDeviceConsoleUWP : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	HANDLE ConsoleHandle;

	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleUWP();
	~FOutputDeviceConsoleUWP();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show(bool ShowWindow);

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category );
};
