// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Private filters not exposed to modules that depend on ContentBrowser

#include "IContentBrowserSingleton.h"
#include "Filters/GenericFilter.h"
#include "FrontendFilterBase.h"
#include "Templates/SharedPointer.h"

/** 
 *  A custom filter that appears in the filter bar but actually controls a content browser setting controlling 
 *  visibility of redirectors for use in backend filtering.
 */
class FFilter_ShowRedirectors : public FFrontendFilter
{
public:
	// TODO: Consider not giving this the entire viewmodel to sync with filter state
	// could make viewmode own/create this, or send a smaller interface ptr into the constructor?
	FFilter_ShowRedirectors (TSharedPtr<FFrontendFilterCategory> InCategory);

	/** Returns the system name for this filter */
	virtual FString GetName() const override { return TEXT("ShowRedirectorsBackend"); } 

	/** Returns the human readable name for this filter */
	virtual FText GetDisplayName() const override;

	/** Returns the tooltip for this filter, shown in the filters menu */
	virtual FText GetToolTipText() const override;

	/** Returns the name of the icon to use in menu entries */
	virtual FName GetIconName() const override;

	/** If true, the filter will be active in the FilterBar when it is inactive in the UI (i.e the filter pill is grayed out)
	 * @See: FFrontendFilter_ShowOtherDevelopers in Content Browser
	 */
	virtual bool IsInverseFilter() const 
	{
		// This has to be an inverse filter to prevent the asset view from recursively displaying all assets 
		return true; 
	}

	/** Notification that the filter became active or inactive */
	virtual void ActiveStateChanged(bool bActive) override;

	/** Can be overriden for custom FilterBar subclasses to save settings, currently not implemented in any gneeric Filter Bar */
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;

	/** Can be overriden for custom FilterBar subclasses to load settings, currently not implemented in any gneeric Filter Bar */
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;

	virtual bool PassesFilter(FAssetFilterType InItem) const override;
};