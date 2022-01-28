// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UWP/UWPApplication.h"
#include "UWP/UWPWindow.h"
#include "UWP/UWPCursor.h"
#include "UWP/UWPInputInterface.h"
#include "UWP/UWPPlatformMisc.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogUWP);

// Flip this on to make license checks operate against the retail Windows Store environment.
// We default to simulator which is only permitted when the OS is in developer mode.  Titles
// will need to manually flip this switch to use retail before ship.  Note that retail license
// checks will not function until the title has been ingested into the retail store catalog.
//#define USING_RETAIL_WINDOWS_STORE 1
#ifndef USING_RETAIL_WINDOWS_STORE
#define USING_RETAIL_WINDOWS_STORE 0
#endif
#if USING_RETAIL_WINDOWS_STORE
using CurrentStoreApp = Windows::ApplicationModel::Store::CurrentApp;
#else
using CurrentStoreApp = Windows::ApplicationModel::Store::CurrentAppSimulator;
#endif


static FUWPApplication* UWPApplication = NULL;
FVector2D FUWPApplication::DesktopSize;

FUWPApplication* FUWPApplication::CreateUWPApplication()
{
	check( UWPApplication == NULL );
	UWPApplication = new FUWPApplication();

	UWPApplication->InitLicensing();

	return UWPApplication;
}

FUWPApplication* FUWPApplication::GetUWPApplication()
{
	return UWPApplication;
}

FUWPApplication::FUWPApplication()
	: GenericApplication( MakeShareable( new FUWPCursor() ) )
	, InputInterface( FUWPInputInterface::Create( MessageHandler ) )
	, ApplicationWindow( FUWPWindow::Make() )
{
}

TSharedRef< FGenericWindow > FUWPApplication::MakeWindow()
{
	return ApplicationWindow;
}

void FUWPApplication::InitializeWindow(const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately)
{
	const TSharedRef< FUWPWindow > Window = StaticCastSharedRef< FUWPWindow >(InWindow);

	Window->Initialize(this, InDefinition);
}

IInputInterface* FUWPApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

void FUWPApplication::PollGameDeviceState( const float TimeDelta )
{
	// Poll game device state and send new events
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
}

FVector2D FUWPApplication::GetDesktopSize()
{
	return DesktopSize;
}

bool FUWPApplication::IsMouseAttached() const
{
	return (ref new Windows::Devices::Input::MouseCapabilities())->MousePresent > 0;
}

bool FUWPApplication::IsGamepadAttached() const
{
	return Windows::Gaming::Input::Gamepad::Gamepads->Size > 0;
}

FPlatformRect FUWPApplication::GetWorkArea(const FPlatformRect& CurrentWindow) const
{
	// No good way of accounting for desktop task bar here (if present) since
	// it's not a Universal Windows construct.  Best we can do is return the
	// full desktop size.

	FPlatformRect WorkArea;
	WorkArea.Left = 0;
	WorkArea.Top = 0;
	WorkArea.Right = DesktopSize.X;
	WorkArea.Bottom = DesktopSize.Y;

	return WorkArea;
}

void FUWPApplication::GetInitialDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const
{
	FDisplayMetrics::RebuildDisplayMetrics(OutDisplayMetrics);

	// The initial virtual display rect is used to constrain maximum window size, and
	// so should report the full desktop resolution.  Later calls will report window
	// size in order to apply a tightly fitting hit test structure.
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
}

void FUWPApplication::CacheDesktopSize()
{
#if WIN10_SDK_VERSION >= 14393
	if (Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent("Windows.Graphics.Display.DisplayInformation", "ScreenHeightInRawPixels"))
	{
		Windows::Graphics::Display::DisplayInformation^ DisplayInfo = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
		DesktopSize.X = DisplayInfo->ScreenWidthInRawPixels;
		DesktopSize.Y = DisplayInfo->ScreenHeightInRawPixels;
	}
	else
#endif
	{
		// Note this only works *before* the CoreWindow has been activated and received its first resize event.
		Windows::UI::ViewManagement::ApplicationView^ ViewManagementView = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
		float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
		FVector2D Size;
		Size.X = FUWPWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Width, Dpi);
		Size.Y = FUWPWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Height, Dpi);
		DesktopSize = Size;
	}
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	FVector2D DesktopSize = FUWPApplication::GetDesktopSize();

	OutDisplayMetrics.PrimaryDisplayWidth = DesktopSize.X;
	OutDisplayMetrics.PrimaryDisplayHeight = DesktopSize.Y;

	// As with FUWPApplication::GetWorkArea we can't really measure the difference between 
	// desktop size and work area on UWP.
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = DesktopSize.X;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = DesktopSize.Y;

	// We can't get a proper VirtualDisplayRect that's the equivalent of Windows Desktop.
	// We can, however, take advantage of the fact that one of the primary purposes of this
	// property is to define the coordinate space for mouse events.  Reporting the window
	// size aligns this with the coordinate space naturally used for UWP pointer events,
	// and allows us to correctly handle cases where a windowed app spans multiple screens.
	OutDisplayMetrics.VirtualDisplayRect = FUWPWindow::GetOSWindowBounds();

	// Translate so that the top left of the Window is the origin to exactly match
	// pointer event coordinate space.
	OutDisplayMetrics.VirtualDisplayRect.Bottom -= OutDisplayMetrics.VirtualDisplayRect.Top;
	OutDisplayMetrics.VirtualDisplayRect.Right -= OutDisplayMetrics.VirtualDisplayRect.Left;
	OutDisplayMetrics.VirtualDisplayRect.Left = 0;
	OutDisplayMetrics.VirtualDisplayRect.Top = 0;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< class FGenericApplicationMessageHandler > FUWPApplication::GetMessageHandler() const
{
	return MessageHandler;
}

void FUWPApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FUWPApplication::PumpMessages(const float TimeDelta)
{
	FUWPMisc::PumpMessages(true);
}

TSharedRef< class FUWPCursor > FUWPApplication::GetCursor() const
{
	return StaticCastSharedPtr<FUWPCursor>( Cursor ).ToSharedRef();
}

bool FUWPApplication::ApplicationLicenseValid(FPlatformUserId PlatformUser)
{
	try
	{
		return CurrentStoreApp::LicenseInformation->IsActive;
	}
	catch (...)
	{
		UE_LOG(LogCore, Warning, TEXT("Error retrieving license information.  This typically indicates that the incorrect version of CurrentStoreApp is being used.  The currently selected version is Windows::ApplicationModel::Store::%s.  Check USING_RETAIL_WINDOWS_STORE."), USING_RETAIL_WINDOWS_STORE != 0 ? TEXT("CurrentApp") : TEXT("CurrentAppSimulator"));
		return false;
	}
}

void FUWPApplication::InitLicensing()
{
#if !USING_RETAIL_WINDOWS_STORE
	FORCE_WACK_FAILURE(LogCore, "This build uses the Store Simulator for license checks.  It will run only in Developer Mode and is not appropriate for sideloading or Store submission.");
#endif
	try
	{
		CurrentStoreApp::LicenseInformation->LicenseChanged += ref new Windows::ApplicationModel::Store::LicenseChangedEventHandler(LicenseChangedHandler);
	}
	catch (...)
	{
		UE_LOG(LogCore, Warning, TEXT("Error registering for LicenseChanged event.  This typically indicates that the incorrect version of CurrentStoreApp is being used.  The currently selected version is Windows::ApplicationModel::Store::%s.  Check USING_RETAIL_WINDOWS_STORE."), USING_RETAIL_WINDOWS_STORE != 0 ? TEXT("CurrentApp") : TEXT("CurrentAppSimulator"));
#if UE_BUILD_SHIPPING
		// In shipping builds we treat this as fatal to reduce the chance of a build with misconfigured licensing being accidentally released.
		throw;
#endif
	}
}

void FUWPApplication::LicenseChangedHandler()
{
	Windows::ApplicationModel::Core::CoreApplication::MainView->Dispatcher->RunAsync(
		Windows::UI::Core::CoreDispatcherPriority::Normal,
		ref new Windows::UI::Core::DispatchedHandler(BroadcastUELicenseChangeEvent));
}

void FUWPApplication::BroadcastUELicenseChangeEvent()
{
	FCoreDelegates::ApplicationLicenseChange.Broadcast();
}


