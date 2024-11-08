#include "JoyShockInterface.h"
#include "JoyShockLibrary.h"

// Based from XInputInterface.cpp

static int32 ForceJoyshockControllerStateUpdate = 0;
FAutoConsoleVariableRef CVarForceJoyShockControllerStateUpdate(
	TEXT("JoyShockLib.ForceControllerStateUpdate"),
	ForceJoyshockControllerStateUpdate,
	TEXT("Force JoyShockLib refresh of controller state on each frame.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

TSharedRef<JoyShockInterface> JoyShockInterface::Create(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, bool bShouldBePrimaryDevice)
{
	return MakeShareable(new JoyShockInterface(InMessageHandler, bShouldBePrimaryDevice));
}

JoyShockInterface::JoyShockInterface(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, bool bShouldBePrimaryDevice)
	: bIsPrimaryDevice(bShouldBePrimaryDevice), MessageHandler(InMessageHandler)
{
	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_JOYSHOCK_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		FMemory::Memzero( &ControllerState, sizeof(FControllerState) );

		ControllerState.ControllerId = 200/*Id range of 200-215*/ + ControllerIndex;
	}

	JslConnectDevices();

	bIsGamepadAttached = false;
	bNeedsControllerStateUpdate = true;
	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("InitialButtonRepeatDelay"), InitialButtonRepeatDelay, GInputIni);
	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("ButtonRepeatDelay"), ButtonRepeatDelay, GInputIni);

	// In the engine, all controllers map to xbox controllers for consistency 
	JoyShockToXboxControllerMapping[0] = 0;		// A
	JoyShockToXboxControllerMapping[1] = 1;		// B
	JoyShockToXboxControllerMapping[2] = 2;		// X
	JoyShockToXboxControllerMapping[3] = 3;		// Y
	JoyShockToXboxControllerMapping[4] = 4;		// L1
	JoyShockToXboxControllerMapping[5] = 5;		// R1
	JoyShockToXboxControllerMapping[6] = 6;		// Back 
	JoyShockToXboxControllerMapping[7] = 7;		// Start
	JoyShockToXboxControllerMapping[8] = 8;		// Left thumbstick
	JoyShockToXboxControllerMapping[9] = 9;		// Right thumbstick
	JoyShockToXboxControllerMapping[10] = 10;	// L2
	JoyShockToXboxControllerMapping[11] = 11;	// R2
	JoyShockToXboxControllerMapping[12] = 12;	// Dpad up
	JoyShockToXboxControllerMapping[13] = 13;	// Dpad down
	JoyShockToXboxControllerMapping[14] = 14;	// Dpad left
	JoyShockToXboxControllerMapping[15] = 15;	// Dpad right
	JoyShockToXboxControllerMapping[16] = 16;	// Left stick up
	JoyShockToXboxControllerMapping[17] = 17;	// Left stick down
	JoyShockToXboxControllerMapping[18] = 18;	// Left stick left
	JoyShockToXboxControllerMapping[19] = 19;	// Left stick right
	JoyShockToXboxControllerMapping[20] = 20;	// Right stick up
	JoyShockToXboxControllerMapping[21] = 21;	// Right stick down
	JoyShockToXboxControllerMapping[22] = 22;	// Right stick left
	JoyShockToXboxControllerMapping[23] = 23;	// Right stick right

	Buttons[0] = FGamepadKeyNames::FaceButtonBottom;
	Buttons[1] = FGamepadKeyNames::FaceButtonRight;
	Buttons[2] = FGamepadKeyNames::FaceButtonLeft;
	Buttons[3] = FGamepadKeyNames::FaceButtonTop;
	Buttons[4] = FGamepadKeyNames::LeftShoulder;
	Buttons[5] = FGamepadKeyNames::RightShoulder;
	Buttons[6] = FGamepadKeyNames::SpecialRight;
	Buttons[7] = FGamepadKeyNames::SpecialLeft;
	Buttons[8] = FGamepadKeyNames::LeftThumb;
	Buttons[9] = FGamepadKeyNames::RightThumb;
	Buttons[10] = FGamepadKeyNames::LeftTriggerThreshold;
	Buttons[11] = FGamepadKeyNames::RightTriggerThreshold;
	Buttons[12] = FGamepadKeyNames::DPadUp;
	Buttons[13] = FGamepadKeyNames::DPadDown;
	Buttons[14] = FGamepadKeyNames::DPadLeft;
	Buttons[15] = FGamepadKeyNames::DPadRight;
	Buttons[16] = FGamepadKeyNames::LeftStickUp;
	Buttons[17] = FGamepadKeyNames::LeftStickDown;
	Buttons[18] = FGamepadKeyNames::LeftStickLeft;
	Buttons[19] = FGamepadKeyNames::LeftStickRight;
	Buttons[20] = FGamepadKeyNames::RightStickUp;
	Buttons[21] = FGamepadKeyNames::RightStickDown;
	Buttons[22] = FGamepadKeyNames::RightStickLeft;
	Buttons[23] = FGamepadKeyNames::RightStickRight;
}

static FName JoyShockInterfaceName = FName("JoyShockInterface");

static FString JoyShockDS4ControllerIdentifier = TEXT("JoyShock-DS4");
static FString JoyShockDSControllerIdentifier = TEXT("JoyShock-DS");
static FString JoyShockSwitchLeftControllerIdentifier = TEXT("JoyShock-Switch-Left");
static FString JoyShockSwitchRightControllerIdentifier = TEXT("JoyShock-Switch-Right");
static FString JoyShockSwitchProControllerIdentifier = TEXT("JoyShock-Switch-Pro");

void JoyShockInterface::GetPlatformUserAndDevice(int32 InControllerId, EInputDeviceConnectionState InDeviceState, FPlatformUserId& OutPlatformUserId, FInputDeviceId& OutDeviceId) const
{
	if (bIsPrimaryDevice)
	{
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		DeviceMapper.RemapControllerIdToPlatformUserAndDevice(InControllerId, OUT OutPlatformUserId, OUT OutDeviceId);

		// If the controller is connected now but was not before, refresh the information
		if (InDeviceState == EInputDeviceConnectionState::Connected || InDeviceState == EInputDeviceConnectionState::Disconnected)
		{
			DeviceMapper.Internal_MapInputDeviceToUser(OutDeviceId, OutPlatformUserId, InDeviceState);
		}
	}
	else
	{
		// Use the controller id as the device id for secondary input devices not connected to the input system.
		OutDeviceId = FInputDeviceId::CreateFromInternalId(InControllerId);
	}
}

namespace UE::JoyShockInterface::Private
{
EInputDeviceConnectionState GetInputDeviceConnectionState(bool bWasConnected, bool bControllerStateIsConnected)
{
	if (!bWasConnected && bControllerStateIsConnected)
	{
		return EInputDeviceConnectionState::Connected;
	}
	else if (bWasConnected && !bControllerStateIsConnected)
	{
		return EInputDeviceConnectionState::Disconnected;
	}
	return EInputDeviceConnectionState::Unknown;
}

}
void JoyShockInterface::SendControllerEvents()
{
	if (JslConnectedDevicesChanged())
	{
		SetNeedsControllerStateUpdate();
	}
	
	bool bWereConnected[MAX_NUM_JOYSHOCK_CONTROLLERS];

	bIsGamepadAttached = false;
	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_JOYSHOCK_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];

		bWereConnected[ControllerIndex] = ControllerState.bIsConnected;

		if (ControllerState.bIsConnected || bNeedsControllerStateUpdate || ForceJoyshockControllerStateUpdate != 0)
		{
			ControllerState.bIsConnected = JslConnected(ControllerIndex);
			ControllerState.ControllerType = JslGetControllerType(ControllerIndex);

			if (ControllerState.bIsConnected)
			{
				bIsGamepadAttached = true;
			}
		}
	}
		
	for ( int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_JOYSHOCK_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];

		FString ControllerIdentifier;
		switch (ControllerState.ControllerType)
		{
		case JS_TYPE_DS4:
			ControllerIdentifier = JoyShockDS4ControllerIdentifier;
			break;
		case JS_TYPE_DS:
			ControllerIdentifier = JoyShockDSControllerIdentifier;
			break;
		case JS_TYPE_JOYCON_LEFT:
			ControllerIdentifier = JoyShockSwitchLeftControllerIdentifier;
			break;
		case JS_TYPE_JOYCON_RIGHT:
			ControllerIdentifier = JoyShockSwitchRightControllerIdentifier;
			break;
		case JS_TYPE_PRO_CONTROLLER:
			ControllerIdentifier = JoyShockSwitchProControllerIdentifier;
			break;
		}
		
		FInputDeviceScope InputScope(this, JoyShockInterfaceName, ControllerIndex, ControllerIdentifier);
		
		const bool bWasConnected = bWereConnected[ControllerIndex];

		// If the controller is connected send events or if the controller was connected send a final event with default states so that 
		// the game doesn't think that controller buttons are still held down
		if( ControllerState.bIsConnected || bWasConnected )
		{
			FPlatformUserId PlatformUser = PLATFORMUSERID_NONE;
			FInputDeviceId InputDevice = INPUTDEVICEID_NONE;
			EInputDeviceConnectionState State = UE::JoyShockInterface::Private::GetInputDeviceConnectionState(bWasConnected, ControllerState.bIsConnected);
			GetPlatformUserAndDevice(ControllerState.ControllerId, State, OUT PlatformUser, OUT InputDevice);

			bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};

			int32 Buttons = JslGetButtons(ControllerIndex);

			float TRIGGER_THRESHOLD = 0.1f;
			float THUMB_THRESHOLD = 0.1f;

			float LeftTrigger = JslGetLeftTrigger(ControllerIndex);
			float RightTrigger = JslGetRightTrigger(ControllerIndex);

			float LeftX = JslGetLeftX(ControllerIndex);
			float LeftY = JslGetLeftY(ControllerIndex);
			float RightX = JslGetRightX(ControllerIndex);
			float RightY = JslGetRightY(ControllerIndex);
		
			// Get the current state of all buttons
			CurrentStates[JoyShockToXboxControllerMapping[0]] = !!(Buttons & JSMASK_S);
			CurrentStates[JoyShockToXboxControllerMapping[1]] = !!(Buttons & JSMASK_E);
			CurrentStates[JoyShockToXboxControllerMapping[2]] = !!(Buttons & JSMASK_W);
			CurrentStates[JoyShockToXboxControllerMapping[3]] = !!(Buttons & JSMASK_N);
			CurrentStates[JoyShockToXboxControllerMapping[4]] = !!(Buttons & JSMASK_SL);
			CurrentStates[JoyShockToXboxControllerMapping[5]] = !!(Buttons & JSMASK_SR);
			CurrentStates[JoyShockToXboxControllerMapping[6]] = !!(Buttons & JSMASK_PLUS);
			CurrentStates[JoyShockToXboxControllerMapping[7]] = !!(Buttons & JSMASK_MINUS);
			CurrentStates[JoyShockToXboxControllerMapping[8]] = !!(Buttons & JSMASK_LCLICK);
			CurrentStates[JoyShockToXboxControllerMapping[9]] = !!(Buttons & JSMASK_RCLICK);
			CurrentStates[JoyShockToXboxControllerMapping[10]] = !!(LeftTrigger > TRIGGER_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[11]] = !!(RightTrigger > TRIGGER_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[12]] = !!(Buttons & JSMASK_UP);
			CurrentStates[JoyShockToXboxControllerMapping[13]] = !!(Buttons & JSMASK_DOWN);
			CurrentStates[JoyShockToXboxControllerMapping[14]] = !!(Buttons & JSMASK_LEFT);
			CurrentStates[JoyShockToXboxControllerMapping[15]] = !!(Buttons & JSMASK_RIGHT);
			CurrentStates[JoyShockToXboxControllerMapping[16]] = !!(LeftY > THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[17]] = !!(LeftY < -THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[18]] = !!(LeftX < -THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[19]] = !!(LeftX > THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[20]] = !!(RightY > THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[21]] = !!(RightY < -THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[22]] = !!(RightX < -THUMB_THRESHOLD);
			CurrentStates[JoyShockToXboxControllerMapping[23]] = !!(RightX > THUMB_THRESHOLD);

			// Send new analog data if it's different or outside the platform deadzone.
			auto OnControllerAnalog = [this, &PlatformUser, &InputDevice](const FName& GamePadKey, const auto NewAxisValue, const float NewAxisValueNormalized, auto& OldAxisValue, const auto DeadZone) {
				if (OldAxisValue != NewAxisValue || FMath::Abs((int32)NewAxisValue) > DeadZone)
				{
					MessageHandler->OnControllerAnalog(GamePadKey, PlatformUser, InputDevice, NewAxisValueNormalized);
				}
				OldAxisValue = NewAxisValue;
			};

			OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, LeftX, LeftX, ControllerState.LeftXAnalog, THUMB_THRESHOLD);
			OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, LeftY, LeftY, ControllerState.LeftYAnalog, THUMB_THRESHOLD);

			OnControllerAnalog(FGamepadKeyNames::RightAnalogX, RightX, RightX, ControllerState.RightXAnalog, THUMB_THRESHOLD);
			OnControllerAnalog(FGamepadKeyNames::RightAnalogY, RightY, RightY, ControllerState.RightYAnalog, THUMB_THRESHOLD);

			OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, LeftTrigger, LeftTrigger, ControllerState.LeftTriggerAnalog, TRIGGER_THRESHOLD);
			OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, RightTrigger, RightTrigger, ControllerState.RightTriggerAnalog, TRIGGER_THRESHOLD);

			const double CurrentTime = FPlatformTime::Seconds();

			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
			{
				if (CurrentStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
				{
					if( CurrentStates[ButtonIndex] )
					{
						MessageHandler->OnControllerButtonPressed( this->Buttons[ButtonIndex], PlatformUser, InputDevice, false );
					}
					else
					{
						MessageHandler->OnControllerButtonReleased( this->Buttons[ButtonIndex], PlatformUser, InputDevice, false );
					}

					if ( CurrentStates[ButtonIndex] != 0 )
					{
						// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
				}
				else if ( CurrentStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime )
				{
					MessageHandler->OnControllerButtonPressed( this->Buttons[ButtonIndex], PlatformUser, InputDevice, true );

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}

				// Update the state for next time
				ControllerState.ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
			}	

			// apply force feedback

			const float LargeValue = (ControllerState.ForceFeedback.LeftLarge > ControllerState.ForceFeedback.RightLarge ? ControllerState.ForceFeedback.LeftLarge : ControllerState.ForceFeedback.RightLarge);
			const float SmallValue = (ControllerState.ForceFeedback.LeftSmall > ControllerState.ForceFeedback.RightSmall ? ControllerState.ForceFeedback.LeftSmall : ControllerState.ForceFeedback.RightSmall);

			if (!FMath::IsNearlyEqual(LargeValue, ControllerState.LastLargeValue) || !FMath::IsNearlyEqual(SmallValue, ControllerState.LastSmallValue))
			{
				JslSetRumble(ControllerIndex,
					static_cast<int32>(LargeValue * 100.0f),
					static_cast<int32>(SmallValue * 100.0f));
 
				ControllerState.LastLargeValue = LargeValue;
				ControllerState.LastSmallValue = SmallValue;
			}
		}
	}

	bNeedsControllerStateUpdate = false;
}


void JoyShockInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void JoyShockInterface::SetDeviceProperty(int32 ControllerId, const FInputDeviceProperty* Property)
{
	// This should only get called from WindowsApplication.cpp when Windows detects a device change.
	static const FName UpdateRequestedName = TEXT("Request_Device_Update");
	if (Property && Property->Name == UpdateRequestedName)
	{
		JslConnectDevices();
		SetNeedsControllerStateUpdate();	
	}
}

bool JoyShockInterface::IsGamepadAttached() const
{
	return bIsGamepadAttached;
}

void JoyShockInterface::SetChannelValue( int32 ControllerId, const FForceFeedbackChannelType ChannelType, const float Value )
{
	if (ControllerId >= 0 && ControllerId < MAX_NUM_JOYSHOCK_CONTROLLERS)
	{
		FControllerState& ControllerState = ControllerStates[ ControllerId ];

		if( ControllerState.bIsConnected )
		{
			switch( ChannelType )
			{
				case FForceFeedbackChannelType::LEFT_LARGE:
					ControllerState.ForceFeedback.LeftLarge = Value;
					break;

				case FForceFeedbackChannelType::LEFT_SMALL:
					ControllerState.ForceFeedback.LeftSmall = Value;
					break;

				case FForceFeedbackChannelType::RIGHT_LARGE:
					ControllerState.ForceFeedback.RightLarge = Value;
					break;

				case FForceFeedbackChannelType::RIGHT_SMALL:
					ControllerState.ForceFeedback.RightSmall = Value;
					break;
			}
		}
	}
}

void JoyShockInterface::SetChannelValues( int32 ControllerId, const FForceFeedbackValues &Values )
{
	if (ControllerId >= 0 && ControllerId < MAX_NUM_JOYSHOCK_CONTROLLERS)
	{
		FControllerState& ControllerState = ControllerStates[ ControllerId ];

		if( ControllerState.bIsConnected )
		{
			ControllerState.ForceFeedback = Values;
		}
	}
}

void JoyShockInterface::SetLightColor(int32 ControllerId, FColor Color)
{
	JslSetLightColour(ControllerId, Color.R, Color.G, Color.B);
}

void JoyShockInterface::ResetLightColor(int32 ControllerId)
{
	JslSetLightColour(ControllerId, 0, 0, 0);
}
