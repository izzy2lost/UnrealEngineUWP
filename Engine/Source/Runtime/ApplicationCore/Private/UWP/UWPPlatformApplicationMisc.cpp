#include "UWP/UWPPlatformApplicationMisc.h"
#include "UWP/UWPApplication.h"
#include "UWP/UWPErrorOutputDevice.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

FOutputDeviceError* FUWPPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FUWPErrorOutputDevice Singleton;
	return &Singleton;
}

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

float FUWPPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	return Dpi / 96.0f;
}

void FUWPPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	using namespace Windows::ApplicationModel::DataTransfer;

	DataPackage ^Package = ref new DataPackage();
	Package->RequestedOperation = DataPackageOperation::Copy;
	Package->SetText(ref new Platform::String(Str));
	Clipboard::SetContent(Package);
}

void FUWPPlatformApplicationMisc::ClipboardPaste(class FString& Dest)
{
	using namespace Windows::Foundation;
	using namespace Windows::ApplicationModel::DataTransfer;

	DataPackageView^ ClipboardContent = Clipboard::GetContent();
	if (ClipboardContent != nullptr && ClipboardContent->Contains(StandardDataFormats::Text))
	{
		IAsyncOperation<Platform::String^>^ AsyncOp = ClipboardContent->GetTextAsync();
		while (AsyncOp->Status == AsyncStatus::Started)
		{
			PumpMessages(false);

			// Yield
			FPlatformProcess::Sleep(0.f);
		}

		if (AsyncOp->Status == AsyncStatus::Completed)
		{
			Dest = AsyncOp->GetResults()->Data();
		}
	}
}
