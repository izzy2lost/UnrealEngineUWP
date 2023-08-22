// Copyright Epic Games, Inc. All Rights Reserved.

#include "XInputDeviceModule.h"
#include "XInputInterface.h"
#include "Modules/ModuleManager.h"	// For IMPLEMENT_MODULE

TSharedPtr<IInputDevice> FXInputDeviceModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	return XInputInterface::Create(InMessageHandler);
}

IMPLEMENT_MODULE(FXInputDeviceModule, XInputDevice)