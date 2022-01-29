#include "UWP/UWPErrorOutputDevice.h"

#include "UWP/UWPApplication.h"
#include "UWP/UWPPlatformApplicationMisc.h"
#include "HAL/PlatformMisc.h"
#include "UWP/WindowsHWrapper.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"


FUWPErrorOutputDevice::FUWPErrorOutputDevice()
{}

void FUWPErrorOutputDevice::Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FPlatformMisc::DebugBreak();

	if (!GIsCriticalError)
	{
		int32 LastError = ::GetLastError();

		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;

		if (LastError == 0)
		{
			UE_LOG(LogUWP, Log, TEXT("UWP GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
		else
		{
			UE_LOG(LogUWP, Error, TEXT("UWP GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
	}
	else
	{
		UE_LOG(LogUWP, Error, TEXT("Error reentered: %s"), Msg);
	}

	if (GIsGuarded)
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException(1);
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FUWPErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogUWP, Error, TEXT("HandleError re-entered."));
		return;
	}

	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist) - 1] = 0;

	// Trigger the OnSystemFailure hook if it exists
	// make sure it happens after GIsGuarded is set to 0 in case this hook crashes
	FCoreDelegates::OnHandleSystemError.Broadcast();

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::LogFormattedMessageWithCallstack(LogUWP.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
	GLog->PanicFlushThreadedLogs();

	// Copy to clipboard in non-cooked editor builds.
	FPlatformApplicationMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport(GErrorHist, EErrorReportMode::Interactive);

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}