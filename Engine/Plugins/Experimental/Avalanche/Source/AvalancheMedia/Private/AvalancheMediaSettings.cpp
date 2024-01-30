// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheMediaSettings.h"

UAvalancheMediaSettings::UAvalancheMediaSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName  = TEXT("Playback & Broadcast");
}

UAvalancheMediaSettings* UAvalancheMediaSettings::GetSingletonInstance()
{
	UAvalancheMediaSettings* DefaultSettings = GetMutableDefault<UAvalancheMediaSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}

ELogVerbosity::Type UAvalancheMediaSettings::ToLogVerbosity(EAvaMediaLogVerbosity InAvaMediaLogVerbosity)
{
	switch (InAvaMediaLogVerbosity)
	{
	case EAvaMediaLogVerbosity::NoLogging:
		return ELogVerbosity::NoLogging;
	case EAvaMediaLogVerbosity::Fatal:
		return ELogVerbosity::Fatal;
	case EAvaMediaLogVerbosity::Error:
		return ELogVerbosity::Error;
	case EAvaMediaLogVerbosity::Warning:
		return ELogVerbosity::Warning;
	case EAvaMediaLogVerbosity::Display:
		return ELogVerbosity::Display;
	case EAvaMediaLogVerbosity::Log:
		return ELogVerbosity::Log;
	case EAvaMediaLogVerbosity::Verbose:
		return ELogVerbosity::Verbose;
	case EAvaMediaLogVerbosity::VeryVerbose:
		return ELogVerbosity::VeryVerbose;
	default:
		return ELogVerbosity::NoLogging;
	}
}



