// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "UWPApplication.h"
#include "UWPWindow.h"
#include "UWPCursor.h"
#include "UWPInputInterface.h"
#include "UWPMisc.h"
#include "GenericApplication.h"


static FUWPApplication* UWPApplication = NULL;

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

FPlatformRect FUWPApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	//@todo UWP: Use the actual device settings here.
	FPlatformRect WorkArea;
	WorkArea.Left = 0;
	WorkArea.Top = 0;
	WorkArea.Right = 1920;
	WorkArea.Bottom = 1080;

	return WorkArea;
}


void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	//@todo UWP: Use the actual device settings here.
	OutDisplayMetrics.PrimaryDisplayWidth = 1920;
	OutDisplayMetrics.PrimaryDisplayHeight = 1080;

	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = 1920;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = 1080;

	OutDisplayMetrics.VirtualDisplayRect.Left = 0;
	OutDisplayMetrics.VirtualDisplayRect.Top = 0;
	OutDisplayMetrics.VirtualDisplayRect.Right = 1920;
	OutDisplayMetrics.VirtualDisplayRect.Bottom = 1080;
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
