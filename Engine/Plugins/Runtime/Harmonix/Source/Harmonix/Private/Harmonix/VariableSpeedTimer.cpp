// Copyright Epic Games, Inc. All Rights Reserved.

#include "Harmonix/VariableSpeedTimer.h"
#include "HAL/PlatformTime.h"

FVariableSpeedTimer::FVariableSpeedTimer()
	: AccumulatedSeconds(0)
	, Speed(1.0)
{}

void FVariableSpeedTimer::Start()
{
	if (!Running)
	{
		CurrentStartTime = FPlatformTime::Seconds();
		Running = true;
	}
}

void FVariableSpeedTimer::Stop()
{
	double CurrentSeconds = FPlatformTime::Seconds();

	if (!Running)
	{
		return;
	}

	AccumulatedSeconds += (CurrentSeconds - CurrentStartTime) * Speed;

	Running = false;
}

void FVariableSpeedTimer::Reset(double InitialMs)
{
	// NOTE: Internally we track time in seconds because that is what the 
	// Unreal "platform time" interface works in. BUT, this means that we 
	// need to convert seconds <-> milliseconds for the user of this class.

	CurrentStartTime = FPlatformTime::Seconds();
	AccumulatedSeconds = InitialMs / 1000.0;
}

void FVariableSpeedTimer::SetSpeed(double NewSpeed)
{
	double CurrentSeconds = FPlatformTime::Seconds();

	if (Running)
	{
		AccumulatedSeconds += (CurrentSeconds - CurrentStartTime) * Speed;
		CurrentStartTime = CurrentSeconds;
	}

	Speed = NewSpeed;
}

double FVariableSpeedTimer::Ms()
{
	// NOTE: Internally we track time in seconds because that is what the 
	// Unreal "platform time" interface works in. BUT, this means that we 
	// need to convert seconds <-> milliseconds for the user of this class.

	if (!Running)
	{
		return AccumulatedSeconds * 1000.0;
	}

	double CurrentSeconds = FPlatformTime::Seconds();

	return (AccumulatedSeconds + (CurrentSeconds - CurrentStartTime) * Speed) * 1000.0f;
}
