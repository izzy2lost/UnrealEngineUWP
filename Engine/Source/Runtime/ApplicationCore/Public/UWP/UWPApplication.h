// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUWP, Log, All);

class FUWPApplication : public GenericApplication
{
public:

	static FUWPApplication* CreateUWPApplication();

	static FUWPApplication* GetUWPApplication();

	static void CacheDesktopSize();

	static FVector2D GetDesktopSize();

public:	

	virtual ~FUWPApplication() {}

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual bool IsMouseAttached() const override;

	virtual bool IsGamepadAttached() const override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;
	virtual TSharedRef< FGenericWindow > MakeWindow();
	virtual void InitializeWindow(const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately);

	virtual IInputInterface* GetInputInterface() override;

	TSharedRef< class FGenericApplicationMessageHandler > GetMessageHandler() const;

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;
	virtual void PumpMessages(const float TimeDelta) override;

	virtual void GetInitialDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const override;

	TSharedRef< class FUWPCursor > GetCursor() const;
	TSharedPtr< class FUWPInputInterface > GetUWPInputInterface() const { return InputInterface; }

	TSharedPtr< class FUWPWindow > GetUWPWindow() const { return ApplicationWindow; }

	virtual bool ApplicationLicenseValid(FPlatformUserId PlatformUser = PLATFORMUSERID_NONE);

private:

	FUWPApplication();

	static void InitLicensing();
	static void LicenseChangedHandler();
	static void BroadcastUELicenseChangeEvent();
private:

	TSharedPtr< class FUWPInputInterface > InputInterface;

	TSharedRef< class FUWPWindow > ApplicationWindow;

	static FVector2D DesktopSize;
};