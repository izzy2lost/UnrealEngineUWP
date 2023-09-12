// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "ProfilingDebugging/MiscTrace.h"

/** Contains all settings for the Unreal Insights, accessible through the main manager. */
class FInsightsSessionBrowserSettings
{
public:
	FInsightsSessionBrowserSettings(bool bInIsDefault = false)
		: bIsEditing(false)
		, bIsDefault(bInIsDefault)
		, bAutoConnect(true)
	{
		if (!bIsDefault)
		{
			LoadFromConfig();
		}
	}

	~FInsightsSessionBrowserSettings()
	{
	}

	void LoadFromConfig()
	{
		if (!FConfigContext::ReadIntoGConfig().Load(TEXT("UnrealInsightsSessionBrowserSettings"), SettingsIni))
		{
			return;
		}

		GConfig->GetBool(TEXT("Insights.SessionBrowser"), TEXT("AutoConnect"), bAutoConnect, SettingsIni);
	}

	void SaveToConfig()
	{
		GConfig->SetBool(TEXT("Insights.SessionBrowser"), TEXT("AutoConnect"), bAutoConnect, SettingsIni);

		GConfig->Flush(false, SettingsIni);
	}

	void EnterEditMode()
	{
		bIsEditing = true;
	}

	void ExitEditMode()
	{
		bIsEditing = false;
	}

	const bool IsEditing() const
	{
		return bIsEditing;
	}

	const FInsightsSessionBrowserSettings& GetDefaults() const
	{
		return Defaults;
	}

	void ResetToDefaults()
	{
		bAutoConnect = Defaults.bAutoConnect;
	}

	#define SET_AND_SAVE(Option, Value) { if (Option != Value) { Option = Value; SaveToConfig(); } }

	bool IsAutoConnectEnabled() const { return bAutoConnect; }
	void SetAutoConnect(bool bOnOff) { bAutoConnect = bOnOff; }
	void SetAndSaveAutoConnect(bool bOnOff) { SET_AND_SAVE(bAutoConnect, bOnOff); }

	#undef SET_AND_SAVE

private:
	/** Contains default settings. */
	static FInsightsSessionBrowserSettings Defaults;

	/** Setting filename ini. */
	FString SettingsIni;

	/** Whether profiler settings is in edit mode. */
	bool bIsEditing;

	/** Whether this instance contains defaults. */
	bool bIsDefault;

	//////////////////////////////////////////////////
	// Actual settings.

	/** Whether Insights should signal to the Editor to auto connect and start tracing when Insights is running */
	bool bAutoConnect;
};
