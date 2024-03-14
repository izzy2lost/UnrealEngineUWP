// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters.h"

#include "FrontendFilterBase.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

FFilter_ShowRedirectors::FFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
{
}

/** Returns the human readable name for this filter */
FText FFilter_ShowRedirectors::GetDisplayName() const
{
	return LOCTEXT("FrontendFilter_ShowRedirectors", "Show Redirectors");
}

/** Returns the tooltip for this filter, shown in the filters menu */
FText FFilter_ShowRedirectors::GetToolTipText() const
{
	return LOCTEXT("FrontendFilter_ShowRedirectorsToolTip", "Allow display of Redirectors.");
}

/** Returns the name of the icon to use in menu entries */
FName FFilter_ShowRedirectors::GetIconName() const
{
	return NAME_None;
}

/** Notification that the filter became active or inactive */
void FFilter_ShowRedirectors::ActiveStateChanged(bool bActive)
{
	// Do nothing, filter state is queried externally e.g. by SContentBrowser
}

/** Can be overriden for custom FilterBar subclasses to save settings, currently not implemented in any gneeric Filter Bar */
void FFilter_ShowRedirectors::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const 
{

}

/** Can be overriden for custom FilterBar subclasses to load settings, currently not implemented in any gneeric Filter Bar */
void FFilter_ShowRedirectors::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) 
{

}

bool FFilter_ShowRedirectors::PassesFilter(FAssetFilterType InItem) const
{
	return true; // All items pass, this filter just communicates with the backend 
}

#undef LOCTEXT_NAMESPACE