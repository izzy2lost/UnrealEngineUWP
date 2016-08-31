// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "UWPApplication.h"
#include "UWPWindow.h"
#include "UWPCursor.h"
#include "UWPInputInterface.h"
#include "UWPMisc.h"
#include "GenericApplication.h"

PACK_WINRT()


static FUWPApplication* UWPApplication = NULL;
FVector2D FUWPApplication::DesktopSize;

FUWPApplication* FUWPApplication::CreateUWPApplication()
{
	check( UWPApplication == NULL );
	UWPApplication = new FUWPApplication();
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

void FUWPApplication::CacheDesktopSize()
{
	// Note this only works *before* the CoreWindow has been activated and received its first resize event.
	Windows::UI::ViewManagement::ApplicationView^ ViewManagementView = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	FVector2D Size;
	Size.X = FUWPWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Width, Dpi);
	Size.Y = FUWPWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Height, Dpi);
	DesktopSize = Size;
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
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

PACK_WINRT_REVERT()