// Copyright (C) Microsoft. All rights reserved.


#include "UWP/UWPWindow.h"
#include "UWP/UWPApplication.h"
#include "CoreTypes.h"

FUWPWindow::FUWPWindow() :
	WindowMode(EWindowMode::Windowed)
{
}

FUWPWindow::~FUWPWindow()
{
}

TSharedRef< FUWPWindow > FUWPWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually contain a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FUWPWindow() );
}

void FUWPWindow::Initialize(FUWPApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition)
{
	// Only 2 options in UWP: windowed and windowed fullscreen.
	auto View = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
	WindowMode = View->IsFullScreenMode ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
}

static const float DipsPerInch = 96.0f;

int32 FUWPWindow::ConvertDipsToPixels(int32 Dips, float Dpi)
{
	return FMath::FloorToInt(static_cast<float>(Dips) * Dpi / DipsPerInch + 0.5f);
}

int32 FUWPWindow::ConvertPixelsToDips(int32 Pixels, float Dpi)
{
	return FMath::FloorToInt(static_cast<float>(Pixels) * DipsPerInch / Dpi + 0.5f);
}

void FUWPWindow::ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height)
{
	auto View = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	Windows::Foundation::Size RequestedSize;
	RequestedSize.Width = ConvertPixelsToDips(Width, Dpi);
	RequestedSize.Height = ConvertPixelsToDips(Height, Dpi);

	// Not a big problem if this fails - will be handled by AdjustCachedSize
	if (View->TryResizeView(RequestedSize))
	{
		// Remember for next time - when we launch windowed we try to match the window size to the
		// render target size, which will always come through this call.
		View->PreferredLaunchViewSize = RequestedSize;
	}
}

FPlatformRect FUWPWindow::GetOSWindowBounds()
{
	auto LowLevelWindow = Windows::ApplicationModel::Core::CoreApplication::GetCurrentView()->CoreWindow;
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	FPlatformRect Bounds;
	Bounds.Top = ConvertDipsToPixels(LowLevelWindow->Bounds.Top, Dpi);
	Bounds.Left = ConvertDipsToPixels(LowLevelWindow->Bounds.Left, Dpi);
	Bounds.Bottom = ConvertDipsToPixels(LowLevelWindow->Bounds.Bottom, Dpi);
	Bounds.Right = ConvertDipsToPixels(LowLevelWindow->Bounds.Right, Dpi);
	return Bounds;
}

void FUWPWindow::AdjustCachedSize(FVector2D& Size) const
{
	// Force SWindow size to match the bounds of the OS window, which we are not entirely in control of.
	FPlatformRect OSBounds = GetOSWindowBounds();
	Size.X = OSBounds.Right - OSBounds.Left;
	Size.Y = OSBounds.Bottom - OSBounds.Top;
}

void FUWPWindow::SetWindowMode(EWindowMode::Type InNewWindowMode)
{
	auto View = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
	if (InNewWindowMode == EWindowMode::Windowed && View->IsFullScreenMode)
	{
		View->ExitFullScreenMode();

		// This will persist the user preference for Windows, and will attempt to match the size to the
		// underlying render resolution.  Note it will ignore direct resizing of the window by the user
		// (not typical UWP behavior, but probably desirable for UE games).
		View->PreferredLaunchWindowingMode = Windows::UI::ViewManagement::ApplicationViewWindowingMode::PreferredLaunchViewSize;
	}
	else if (InNewWindowMode != EWindowMode::Windowed && !View->IsFullScreenMode)
	{
		if (View->TryEnterFullScreenMode())
		{
			// Persist the preference for full-screen.
			View->PreferredLaunchWindowingMode = Windows::UI::ViewManagement::ApplicationViewWindowingMode::FullScreen;
		}

		// Note that the attempt to go full-screen might fail!
		// @todo: who's responsible for checking this and handling it?
	}

	WindowMode = View->IsFullScreenMode ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
}

EWindowMode::Type FUWPWindow::GetWindowMode() const
{
	return WindowMode;
}
