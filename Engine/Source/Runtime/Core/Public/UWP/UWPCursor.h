// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"
#include "agile.h"

PACK_WINRT()
ref class FUWPCursorMouseEventObj sealed
{
public:
    FUWPCursorMouseEventObj();

    void OnMouseMoved(Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args);

    Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>^ GetMouseMovedHandler();
};
PACK_WINRT_REVERT()

class FUWPCursor : public ICursor
{
public:

    FUWPCursor();

	virtual ~FUWPCursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition( const int32 X, const int32 Y ) override;

	virtual void SetType( const EMouseCursor::Type InNewCursor ) override;

	virtual EMouseCursor::Type GetType() const override
	{
		return CurrentCursor;
	}

	virtual void GetSize( int32& Width, int32& Height ) const override;

	virtual void Show( bool bShow ) override;

	virtual void Lock( const RECT* const Bounds ) override;

    void UpdatePosition(const FVector2D& NewPosition);

    bool IsUsingRawMouseNoCursor() { return bUsingRawMouseNoCursor; }

    void ProcessDeferredActions();

private:

    EMouseCursor::Type                                CurrentCursor = (EMouseCursor::Type) - 1;
    FVector2D                                         CursorPosition;
    bool                                              bUsingRawMouseNoCursor;
    bool                                              bDeferredCursorTypeChange;
    
    /** Cursors */
    Platform::Array<Windows::UI::Core::CoreCursor^>^  Cursors;
    FUWPCursorMouseEventObj^                       MouseEventObj;
    Windows::Foundation::EventRegistrationToken       MouseEventRegistrationToken;
    Platform::Agile<Windows::Devices::Input::MouseDevice>             MouseDevice;
};