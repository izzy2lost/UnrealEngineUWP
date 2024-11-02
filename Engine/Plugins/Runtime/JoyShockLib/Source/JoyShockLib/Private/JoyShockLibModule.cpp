#include "JoyShockLibModule.h"
#include "JoyShockInterface.h"

TSharedPtr<IInputDevice> FJoyShockLibModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	return JoyShockInterface::Create(InMessageHandler, true);
}

TSharedPtr<IInputDevice> FJoyShockLibModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, FInputDeviceCreationParameters InParameters)
{
	return JoyShockInterface::Create(InMessageHandler, InParameters.bInitAsPrimaryDevice);
}

IMPLEMENT_MODULE(FJoyShockLibModule, JoyShockLib)