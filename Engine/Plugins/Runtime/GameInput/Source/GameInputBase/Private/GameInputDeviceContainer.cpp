// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputDeviceContainer.h"
#include "Framework/Application/SlateApplication.h"	// for GetPlatformCursor
#include "GenericPlatform/ICursor.h"
#include "GameInputUtils.h"
#include "GameInputLogging.h"
#include "GameInputDeveloperSettings.h"
#include "HAL/IConsoleManager.h"

#if GAME_INPUT_SUPPORT

namespace UE::GameInput
{
	static TAutoConsoleVariable<bool> CVarGameInputForceDeviceProcesorsOncePerFrame
	(
		TEXT("gameinput.bForceDeviceProcesorsOncePerFrame"),
		false,
		TEXT("If true, then FGameInputDeviceContainer::ProcessInput will ensure that device processors are only called once per ProcessInput call (i.e. once per frame)"),
		ECVF_Default
	);
};

FGameInputDeviceContainer::FGameInputDeviceContainer(
	const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler,
	IGameInputDevice* InDevice,
	GameInputKind InAllowedGameInputKinds,
	FPlatformUserId InUserId, 
	FInputDeviceId InDeviceId)
	: MessageHandler(InMessageHandler)
	, Device(InDevice)
	, AllowedGameInputKinds(InAllowedGameInputKinds)
	, UserId(InUserId)
	, AssignedDeviceId(InDeviceId)
	, IgnoreReadingTimestamp(0)
{
	SetGameInputDevice(InDevice);
}

void FGameInputDeviceContainer::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

void FGameInputDeviceContainer::InitalizeDeviceProcessors()
{
	if (!Device)
	{
		return;
	}

	// Based on the GameInputKind of this device, create any processors that are supported
	const GameInputDeviceInfo* Info = Device->GetDeviceInfo();

	const GameInputKind DeviceKind = Info->supportedInput;

	UE_LOG(LogGameInput, Log, TEXT("InitalizeDeviceProcessors for device kind %s with an allowed kind of %s"), *UE::GameInput::LexToString(DeviceKind), *UE::GameInput::LexToString(AllowedGameInputKinds));

	InitalizeDeviceProcessors_Impl();
}

void FGameInputDeviceContainer::InitalizeDeviceProcessors_Impl()
{
	if (!Device)
	{
		return;
	}

	// Based on the GameInputKind of this device, create any processors that are supported
	const GameInputDeviceInfo* Info = Device->GetDeviceInfo();

	const GameInputKind DeviceKind = Info->supportedInput;

	if (DeviceKind & AllowedGameInputKinds & GameInputKindGamepad)
	{
		Processors.Emplace(MakeShared<FGameInputGamepadDeviceProcessor>());
	}
	
	if (DeviceKind & AllowedGameInputKinds & GameInputKindKeyboard)
	{
		Processors.Emplace(MakeShared<FGameInputKeyboardDeviceProcessor>());
	}
	
	// TODO: This was throwing a linker error on some platforms, investigate.
	//if (DeviceKind & AllowedGameInputKinds & GameInputKindMouse)
	//{
	//	// TODO: Make high precision mouse a config setting
	//	Processors.Emplace(MakeShared<FGameInputMouseDeviceProcessor>(FSlateApplication::Get().GetPlatformCursor()));
	//}

	if (DeviceKind & AllowedGameInputKinds & GameInputKindTouch)
	{
		Processors.Emplace(MakeShared<FGameInputTouchDeviceProcessor>());
	}

	// Only allow for external controller devices have have specified configs if that is enabled
	const UGameInputDeveloperSettings* Settings = GetDefault<UGameInputDeveloperSettings>();
	const UGameInputPlatformSettings* PlatformSettings = UGameInputPlatformSettings::Get();
	
	// "Controller" types
	if (DeviceKind & AllowedGameInputKinds & GameInputKindController)
	{
		const bool bIsDeviceAllowed =
			PlatformSettings->bSpecialDevicesRequireExplicitDeviceConfiguration ?
			(Settings->FindDeviceConfiguration(Info) != nullptr) :
			true;

		if (bIsDeviceAllowed)
		{
			Processors.Emplace(MakeShared<FGameInputControllerDeviceProcessor>());
		}
		else
		{
			UE_LOG(
				LogGameInput,
				Warning,
				TEXT("A game input controller device (%s) was connected but will not be processed because it does not have an explict device configuration"),
				*UE::GameInput::LexToString(Device));
		}
	}

	// Raw Input
	if (DeviceKind & AllowedGameInputKinds & GameInputKindRawDeviceReport)
	{
		const bool bIsDeviceAllowed =
			PlatformSettings->bSpecialDevicesRequireExplicitDeviceConfiguration ?
			(Settings->FindDeviceConfiguration(Info) != nullptr) :
			true;

		if (bIsDeviceAllowed)
		{
			Processors.Emplace(MakeShared<FGameInputRawDeviceProcessor>());
		}
		else
		{
			UE_LOG(
				LogGameInput,
				Warning,
				TEXT("A raw game input device (%s) was connected but will not be processed because it does not have an explict device configuration"),
				*UE::GameInput::LexToString(Device));
		}
	}

	// Racing wheels
	if (DeviceKind & AllowedGameInputKinds & GameInputKindRacingWheel)
	{
		Processors.Emplace(MakeShared<FGameInputRacingWheelProcessor>());
	}

	// TODO: Implement these kinds of input! 

	//if (DeviceKind & AllowedGameInputKinds & GameInputKindFlightStick)
	//{

	//}

	//if (DeviceKind & AllowedGameInputKinds & GameInputKindArcadeStick)
	//{

	//}
}

const GameInputKind FGameInputDeviceContainer::ProcessInput(IGameInput* GameInput, const GameInputKind CurrentSupportedKind, const GameInputKind ProcessedKindsForPlatformUserThisFrame)
{
	GameInputKind OutProcessedInputKinds = GameInputKindUnknown;

	const bool bDoNotProcessDuplicateCapabilitiesForSingleUser = GetDefault<UGameInputDeveloperSettings>()->bDoNotProcessDuplicateCapabilitiesForSingleUser;

	// keep reading the input snapshots for this device
	if (!LastReading.IsValid())
	{				
		GameInput->GetCurrentReading(CurrentSupportedKind, Device, &LastReading);
	}

	// Keep track of the number of input device processors we have used this frame.
	// We only want to process each one once...
	static TSet<TSharedPtr<IGameInputDeviceProcessor>> UsedProcessorsThisFrame;
	UsedProcessorsThisFrame.Reset();

	// Set of processors that we should skip because the platform user has already processed one
	// of their type this frame already.
	static TSet<TSharedPtr<IGameInputDeviceProcessor>> ProcessorsToSkipThisFrame;
	ProcessorsToSkipThisFrame.Reset();

	int32 NumReadingsProcessed = 0;

	while (LastReading.IsValid())
	{
		// The current game input reading. This can be null if there is nothing else in the input stack
		TComPtr<IGameInputReading> Reading;
		HRESULT hr = GameInput->GetNextReading(LastReading.Get(), CurrentSupportedKind, Device, &Reading);
		
		// On the last reading of the frame, there will not be a "next" reading.
		const bool bIsLastReadingOfFrame = (hr == GAMEINPUT_E_READING_NOT_FOUND);

		if (FAILED(hr))
		{
			if (hr != GAMEINPUT_E_READING_NOT_FOUND)
			{
				// unexpected error - start from scratch next frame
				LastReading.Reset();
				break;
			}
		}

		// ignore this input if we're suppressing input for a while
		if (Reading && Reading->GetTimestamp() < IgnoreReadingTimestamp)
		{
			LastReading = Reading;
			return OutProcessedInputKinds;
		}

		const GameInputKind CurrentReadingKind = Reading ? Reading->GetInputKind() : LastReading->GetInputKind();

		// Pass along this reading to our Input Processors, who will actually do the work of sending messages
		// and events to the message handler
		IGameInputDeviceProcessor::FGameInputEventParams Params = {};
		Params.Reading = Reading;
		Params.PreviousReading = LastReading;
		Params.Device = Device;
		Params.MessageHandler = MessageHandler;
		Params.PlatformUserId = UserId;
		Params.InputDeviceId = AssignedDeviceId;

		const bool bEnforceOncePerFrameProcessors = UE::GameInput::CVarGameInputForceDeviceProcesorsOncePerFrame.GetValueOnAnyThread();

		for (TSharedPtr<IGameInputDeviceProcessor>& Processor : Processors)
		{
			// Skip any processors that have already been used this frame
			if (bEnforceOncePerFrameProcessors && UsedProcessorsThisFrame.Contains(Processor))
			{
				continue;
			}

			const GameInputKind ProcessorKind = Processor->GetSupportedReadingKind();

			// The first time we process this, check for if the user has already handled a processor of this kind during this frame.
			// If they have, then we should skip processing for the rest of this frame.
			if (NumReadingsProcessed == 0 && bDoNotProcessDuplicateCapabilitiesForSingleUser && (ProcessedKindsForPlatformUserThisFrame & ProcessorKind) != 0)
			{
				ProcessorsToSkipThisFrame.Add(Processor);
			}

			if (ProcessorsToSkipThisFrame.Contains(Processor))
			{
				continue;
			}

			// Only try and process this processor if the GameInput reading has readings for it
			if ((ProcessorKind & CurrentReadingKind) != 0)
			{
				bool bHasInputThisFrame = false;
				if (bIsLastReadingOfFrame)
				{
					// On the last reading input for the frame, the current reading pointer should be null
					ensure(!Params.Reading && Params.PreviousReading);
					bHasInputThisFrame |= Processor->PostProcessInput(Params);
				}
				else
				{
					// When processing normal input we should always have a current
					// and previous reading
					ensure(Params.Reading && Params.PreviousReading);
					bHasInputThisFrame |= Processor->ProcessInput(Params);
					++NumReadingsProcessed;
				}

				UsedProcessorsThisFrame.Add(Processor);

				// Keep track of what input processors have sent events
				if (bHasInputThisFrame)
				{
					OutProcessedInputKinds |= ProcessorKind;
				}				
			}
		}

		// if this was the last reading of the frame, then Reading is going to be null
		// we dont need to track anything here because it happened on the previous iteration
		if (bIsLastReadingOfFrame)
		{
			break;
		}

		// Remember this reading so that we can diff against it next frame if we need to
		LastReading = Reading;

		// Keep track of the timestamp of this reading so that we can later determine the most recently used device
		LastReadingTimestamp = LastReading->GetTimestamp();
	}

	UE_LOG(LogGameInput, VeryVerbose, TEXT("Processed '%d' GameInput readings off the input stack for device: %s"), NumReadingsProcessed, *UE::GameInput::LexToString(Device));

	return OutProcessedInputKinds;
}

void FGameInputDeviceContainer::ClearInputState(IGameInput* GameInput)
{
	IGameInputDeviceProcessor::FGameInputEventParams Params = {};
	Params.Reading = nullptr;	// We have no reading when we clear input
	Params.PreviousReading = nullptr;
	Params.Device = Device;
	Params.MessageHandler = MessageHandler;
	Params.PlatformUserId = UserId;
	Params.InputDeviceId = AssignedDeviceId;

	for (TSharedPtr<IGameInputDeviceProcessor>& Processor : Processors)
	{
		Processor->ClearState(Params);
	}

	LastReading.Reset();
}

IGameInputDevice* FGameInputDeviceContainer::GetGameInputDevice() const
{
	return Device;
}

void FGameInputDeviceContainer::SetGameInputDevice(IGameInputDevice* InDevice)
{
	Device = InDevice;
	if (Device)
	{
		if (const GameInputDeviceInfo* Info = InDevice->GetDeviceInfo())
		{
			LocalDeviceId = Info->deviceId;
		}
		else
		{
			// We shouldn't be able to get here, but handle it just in case.
			ensure(false);
			LocalDeviceId = {};
		}
	}
	else
	{
		LocalDeviceId = {};
	}
}

APP_LOCAL_DEVICE_ID FGameInputDeviceContainer::GetGameInputDeviceId() const
{
	return LocalDeviceId;
}

void FGameInputDeviceContainer::SetPlatformUserId(const FPlatformUserId InUserId)
{
	UserId = InUserId;
}

FPlatformUserId FGameInputDeviceContainer::GetPlatformUserId() const
{
	return UserId;
}

void FGameInputDeviceContainer::SetInputDeviceId(const FInputDeviceId InDeviceId)
{
	AssignedDeviceId = InDeviceId;
}

FInputDeviceId FGameInputDeviceContainer::GetDeviceId() const
{
	return AssignedDeviceId;
}

#endif	// GAME_INPUT_SUPPORT
