// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UWP/UWPCursor.h"
#include "UWP/UWPApplication.h"
#include "UWP/UWPWindow.h"

using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

FUWPCursorMouseEventObj::FUWPCursorMouseEventObj()
{
}

Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>^ FUWPCursorMouseEventObj::GetMouseMovedHandler()
{
    return ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>(this, &FUWPCursorMouseEventObj::OnMouseMoved);
}

void FUWPCursorMouseEventObj::OnMouseMoved(Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args)
{
    FUWPApplication* Application = FUWPApplication::GetUWPApplication();
    if (Application != NULL)
    {
		Application->GetCursor()->OnRawMouseMove(FIntVector(args->MouseDelta.X, args->MouseDelta.Y, 0));
    }
}

FUWPCursor::FUWPCursor()
{
    Cursors = ref new Platform::Array<CoreCursor^>((int)EMouseCursor::TotalCursorCount);
    MouseEventObj = ref new FUWPCursorMouseEventObj();
    bUsingRawMouseNoCursor = false;
    bDeferredCursorTypeChange = false;

	// Load up cursors that we'll be using
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		Cursors[ CurCursorIndex ] = GetDefaultCursorForType(static_cast<EMouseCursor::Type>(CurCursorIndex));
	}
}

FUWPCursor::~FUWPCursor()
{
	// Release cursors
	// NOTE: Shared cursors will automatically be destroyed when the application is destroyed.
	//       For dynamically created cursors, use DestroyCursor
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		switch( CurCursorIndex )
		{
		case EMouseCursor::None:
		case EMouseCursor::Default:
		case EMouseCursor::TextEditBeam:
		case EMouseCursor::ResizeLeftRight:
		case EMouseCursor::ResizeUpDown:
		case EMouseCursor::ResizeSouthEast:
		case EMouseCursor::ResizeSouthWest:
		case EMouseCursor::CardinalCross:
		case EMouseCursor::Crosshairs:
		case EMouseCursor::Hand:
		case EMouseCursor::GrabHand:
		case EMouseCursor::GrabHandClosed:
		case EMouseCursor::SlashedCircle:
		case EMouseCursor::EyeDropper:
		case EMouseCursor::Custom:
			// Standard shared cursors don't need to be destroyed
			break;

		default:
			// Unrecognized cursor type!
			check( 0 );
			break;
		}
	}
}


FVector2D FUWPCursor::GetPosition() const
{
	return CursorPosition;
}

void FUWPCursor::SetPosition( const int32 X, const int32 Y )
{
    CursorPosition.X = X;
    CursorPosition.Y = Y;
}

void FUWPCursor::ProcessDeferredActions()
{
    if (bDeferredCursorTypeChange)
    {
		SetType(CurrentCursor);
    }

	if (DeferredMoveEvents.Num() > 0)
	{
		FUWPApplication* Application = FUWPApplication::GetUWPApplication();
		if (Application != nullptr)
		{
			float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
			for (const FIntVector& MouseDelta : DeferredMoveEvents)
			{
				Application->GetMessageHandler()->OnRawMouseMove(FUWPWindow::ConvertDipsToPixels(MouseDelta.X, Dpi), FUWPWindow::ConvertDipsToPixels(MouseDelta.Y, Dpi));
			}
			Application->GetMessageHandler()->OnCursorSet();
		}
		DeferredMoveEvents.Empty();
	}
}

void FUWPCursor::OnRawMouseMove(const FIntVector& MouseDelta)
{
	DeferredMoveEvents.Add(MouseDelta);
}

void FUWPCursor::SetType( const EMouseCursor::Type InNewCursor )
{
    checkf(InNewCursor < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), InNewCursor);

	if (CurrentCursor != InNewCursor || bDeferredCursorTypeChange)
	{
		CurrentCursor = InNewCursor;
		// if we're on the UI thread, change the cursor, otherwise queue a deferred change
        CoreWindow^ window = CoreWindow::GetForCurrentThread();
		if (nullptr == window)
		{
            bDeferredCursorTypeChange = true;
		}
		else
		{
			bDeferredCursorTypeChange = false;
			SetUseRawMouse(CurrentCursor == EMouseCursor::None);

			try
			{
				window->PointerCursor = Cursors[CurrentCursor];
			}
			catch (Platform::Exception^ Ex)
			{
				if (CurrentCursor == EMouseCursor::Custom && Cursors[CurrentCursor] != nullptr)
				{
					UE_LOG(LogCore, Error, TEXT("Failed to set custom cursor with resource id %u.  Either id was invalid or cursor was not compiled in.  OS error message : %s"), Cursors[CurrentCursor]->Id, Ex->Message->Data());
				}
				else
				{
					UE_LOG(LogCore, Error, TEXT("Failed to set cursor to type %d : %s"), static_cast<int32>(CurrentCursor), Ex->Message->Data());
				}
			}
		}
    }
}

void FUWPCursor::GetSize( int32& Width, int32& Height ) const
{
	Width = 16;
	Height = 16;
}

void FUWPCursor::UpdatePosition( const FVector2D& NewPosition )
{
	CursorPosition = NewPosition;
}

void FUWPCursor::Show( bool bShow )
{
}

void FUWPCursor::Lock( const RECT* const Bounds )
{
}

void FUWPCursor::SetUseRawMouse(bool bUse)
{
	if (bUsingRawMouseNoCursor != bUse)
	{
		bUsingRawMouseNoCursor = bUse;

		try
		{
			Windows::Devices::Input::MouseDevice^ LowLevelMouse = Windows::Devices::Input::MouseDevice::GetForCurrentView();
			if (bUsingRawMouseNoCursor)
			{
				check(MouseEventRegistrationToken.Value == 0);
				MouseEventRegistrationToken = (LowLevelMouse->MouseMoved += MouseEventObj->GetMouseMovedHandler());
			}
			else
			{
				check(MouseEventRegistrationToken.Value != 0);
				LowLevelMouse->MouseMoved -= MouseEventRegistrationToken;
				MouseEventRegistrationToken.Value = 0;
			}
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogCore, Warning, TEXT("Exception managing registration for low-level mouse events: %s"), Ex->Message->Data());
		}
	}
}

void FUWPCursor::SetTypeShape(EMouseCursor::Type InCursorType, void* CursorHandle)
{
	if (CursorHandle != nullptr)
	{
		// This will succeed even if CursorResourceId is invalid.  The point of failure if
		// someone supplied a bad value will be when we actually try to set the window cursor.
		Cursors[InCursorType] = ref new CoreCursor(CoreCursorType::Custom, static_cast<uint32>(reinterpret_cast<uint64>(CursorHandle)));
	}
	else
	{
		Cursors[InCursorType] = GetDefaultCursorForType(InCursorType);
	}

	if (CurrentCursor == InCursorType)
	{
		SetType(InCursorType);
	}
}

Windows::UI::Core::CoreCursor^ FUWPCursor::GetDefaultCursorForType(EMouseCursor::Type InCursorType)
{
	switch (InCursorType)
	{
	case EMouseCursor::None:
	case EMouseCursor::Custom:
		return nullptr;

	case EMouseCursor::Default:
		return ref new CoreCursor(CoreCursorType::Arrow, 0);

	case EMouseCursor::TextEditBeam:
		return ref new CoreCursor(CoreCursorType::IBeam, 0);

	case EMouseCursor::ResizeLeftRight:
		return ref new CoreCursor(CoreCursorType::SizeWestEast, 0);

	case EMouseCursor::ResizeUpDown:
		return ref new CoreCursor(CoreCursorType::SizeNorthSouth, 0);

	case EMouseCursor::ResizeSouthEast:
		return ref new CoreCursor(CoreCursorType::SizeNorthwestSoutheast, 0);

	case EMouseCursor::ResizeSouthWest:
		return ref new CoreCursor(CoreCursorType::SizeNortheastSouthwest, 0);

	case EMouseCursor::CardinalCross:
		return ref new CoreCursor(CoreCursorType::SizeAll, 0);

	case EMouseCursor::Crosshairs:
		return ref new CoreCursor(CoreCursorType::Cross, 0);

	case EMouseCursor::Hand:
		return ref new CoreCursor(CoreCursorType::Hand, 0);

	case EMouseCursor::GrabHand:
		return ref new CoreCursor(CoreCursorType::Hand, 0);

	case EMouseCursor::GrabHandClosed:
		return ref new CoreCursor(CoreCursorType::Hand, 0);

	case EMouseCursor::SlashedCircle:
		return ref new CoreCursor(CoreCursorType::UniversalNo, 0);

	case EMouseCursor::EyeDropper:
		return ref new CoreCursor(CoreCursorType::Arrow, 0);

	default:
		// Unrecognized cursor type!
		check(0);
		return nullptr;
	}

}

