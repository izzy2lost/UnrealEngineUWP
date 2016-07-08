// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "UWPCursor.h"
#include "UWPApplication.h"

using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

PACK_WINRT()

FUWPCursorMouseEventObj::FUWPCursorMouseEventObj()
{
}

Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>^ FUWPCursorMouseEventObj::GetMouseMovedHandler()
{
    return ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>(this, &FUWPCursorMouseEventObj::OnMouseMoved);
}

void FUWPCursorMouseEventObj::OnMouseMoved(Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args)
{
    FUWPApplication* const Application = FUWPApplication::GetUWPApplication();
    if (Application != NULL)
    {
        Application->GetMessageHandler()->OnRawMouseMove(args->MouseDelta.X, args->MouseDelta.Y);
        Application->GetMessageHandler()->OnCursorSet();
    }
}


FUWPCursor::FUWPCursor()
{
    Cursors = ref new Platform::Array<CoreCursor^>((int)EMouseCursor::TotalCursorCount);
    MouseEventObj = ref new FUWPCursorMouseEventObj();
    bUsingRawMouseNoCursor = false;
    bDeferredCursorTypeChange = false;
    MouseDevice = Windows::Devices::Input::MouseDevice::GetForCurrentView();
    MouseEventRegistrationToken = MouseDevice->MouseMoved += MouseEventObj->GetMouseMovedHandler();

	// Load up cursors that we'll be using
	for( int32 CurCursorIndex = 0; CurCursorIndex < EMouseCursor::TotalCursorCount; ++CurCursorIndex )
	{
		CoreCursor^ Cursor = nullptr;
		switch( CurCursorIndex )
		{
		case EMouseCursor::None:
			// The mouse cursor will not be visible when None is used
			break;

		case EMouseCursor::Default:
            Cursor = ref new CoreCursor(CoreCursorType::Arrow, 0); 
			break;

		case EMouseCursor::TextEditBeam:
            Cursor = ref new CoreCursor(CoreCursorType::IBeam, 0); 
			break;

		case EMouseCursor::ResizeLeftRight:
            Cursor = ref new CoreCursor(CoreCursorType::SizeWestEast, 0); 
			break;

		case EMouseCursor::ResizeUpDown:
            Cursor = ref new CoreCursor(CoreCursorType::SizeNorthSouth, 0); 
			break;

		case EMouseCursor::ResizeSouthEast:
            Cursor = ref new CoreCursor(CoreCursorType::SizeNorthwestSoutheast, 0); 
			break;

		case EMouseCursor::ResizeSouthWest:
            Cursor = ref new CoreCursor(CoreCursorType::SizeNortheastSouthwest, 0); 
			break;

		case EMouseCursor::CardinalCross:
            Cursor = ref new CoreCursor(CoreCursorType::SizeAll, 0); 
			break;

		case EMouseCursor::Crosshairs:
            Cursor = ref new CoreCursor(CoreCursorType::Cross, 0); 
			break;

		case EMouseCursor::Hand:
            Cursor = ref new CoreCursor(CoreCursorType::Hand, 0); 
			break;

		case EMouseCursor::GrabHand:
            Cursor = ref new CoreCursor(CoreCursorType::Hand, 0); 
			break;

		case EMouseCursor::GrabHandClosed:
            Cursor = ref new CoreCursor(CoreCursorType::Hand, 0); 
			break;

		case EMouseCursor::SlashedCircle:
            Cursor = ref new CoreCursor(CoreCursorType::UniversalNo, 0); 
			break;

		case EMouseCursor::EyeDropper:
            Cursor = ref new CoreCursor(CoreCursorType::Arrow, 0);  
			break;

			// NOTE: For custom app cursors, use:
			//		Cursor = ref new CoreCursor(CoreCursorType::Custom, MY_RESOURCE_ID );

		default:
			// Unrecognized cursor type!
			check( 0 );
			break;
		}

		Cursors[ CurCursorIndex ] = Cursor;
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
        CoreWindow^ window = CoreWindow::GetForCurrentThread();
        if (nullptr != window)
        {
            window->PointerCursor = Cursors[CurrentCursor];
            if (CurrentCursor == EMouseCursor::None)
            {
                bUsingRawMouseNoCursor = true;
            }
            bDeferredCursorTypeChange = false;
        }
    }
}


void FUWPCursor::SetType( const EMouseCursor::Type InNewCursor )
{
    checkf(InNewCursor < EMouseCursor::TotalCursorCount, TEXT("Invalid cursor(%d) supplied"), InNewCursor);

	if (CurrentCursor != InNewCursor)
	{
        if (CurrentCursor == EMouseCursor::None)
        {
            bUsingRawMouseNoCursor = false;
        }

        // if we're on the UI thread, change the cursor, otherwise queue a deferred change
        CoreWindow^ window = CoreWindow::GetForCurrentThread();
		if (nullptr == window)
		{
            bDeferredCursorTypeChange = true;
		}
		else
		{
            window->PointerCursor = Cursors[InNewCursor];
            // if switching to view-look-capture mode...
            if (InNewCursor == EMouseCursor::None)
            {
                bUsingRawMouseNoCursor = true;
            }
		}
        CurrentCursor = InNewCursor;
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

PACK_WINRT_REVERT()
