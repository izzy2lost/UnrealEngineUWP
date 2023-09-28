// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerVLogSettings.h"
#include "Misc/CoreDelegates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewindDebuggerVLogSettings)

URewindDebuggerVLogSettings::URewindDebuggerVLogSettings()
{
	FCoreDelegates::OnPreExit.AddLambda([]()
	{
		Get().SaveConfig();
	});
}

#if WITH_EDITOR

FText URewindDebuggerVLogSettings::GetSectionText() const
{
	return NSLOCTEXT("RewindDebugger", "RewindDebuggerVLogSettingsName", "Rewind Debugger");
}

FText URewindDebuggerVLogSettings::GetSectionDescription() const
{
	return NSLOCTEXT("RewindDebugger", "RewindDebuggerVLogSettingsDescription", "Configure options for the Rewind Debugger.");
}

#endif

FName URewindDebuggerVLogSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

URewindDebuggerVLogSettings& URewindDebuggerVLogSettings::Get()
{
	URewindDebuggerVLogSettings* MutableCDO = GetMutableDefault<URewindDebuggerVLogSettings>();
	check(MutableCDO != nullptr)
	
	return *MutableCDO;
}

