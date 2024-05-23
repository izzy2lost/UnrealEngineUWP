// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerFilter.h"

#include "Compatibility/TedsCompatibilityUtils.h"

FTEDSOutlinerFilter::FTEDSOutlinerFilter(const FName& InFilterName, const FText& InFilterDisplayName,
	TSharedPtr<FFilterCategory> InCategory, TSharedRef<FTedsOutlinerImpl> InTedsOutlinerImpl,
	const TypedElementDataStorage::FQueryDescription& InFilterQuery)
	: FFilterBase(InCategory)
	, FilterName(InFilterName)
	, FilterDisplayName(InFilterDisplayName)
	, TedsOutlinerImpl(InTedsOutlinerImpl)
	, FilterQuery(InFilterQuery)
{
	
}

FString FTEDSOutlinerFilter::GetName() const
{
	return FilterName.ToString();
}

FText FTEDSOutlinerFilter::GetDisplayName() const
{
	return FilterDisplayName;
}

FText FTEDSOutlinerFilter::GetToolTipText() const
{
	return FText::FromName(FilterName);
}

FLinearColor FTEDSOutlinerFilter::GetColor() const
{
	return FLinearColor();	
}

FName FTEDSOutlinerFilter::GetIconName() const
{
	return FName();
}

bool FTEDSOutlinerFilter::IsInverseFilter() const
{
	return false;
}

void FTEDSOutlinerFilter::ActiveStateChanged(bool bActive)
{
	if(bActive)
	{
		TedsOutlinerImpl->AddExternalQuery(FilterName, FilterQuery);
	}
	else
	{
		TedsOutlinerImpl->RemoveExternalQuery(FilterName);
	}
}

void FTEDSOutlinerFilter::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	
}

void FTEDSOutlinerFilter::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	
}

void FTEDSOutlinerFilter::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	
}

bool FTEDSOutlinerFilter::PassesFilter(SceneOutliner::FilterBarType InItem) const
{
	// If this item is not compatible with the owning Table Viewer - it does not pass any filter queries
	// If it is compatible, this is simply a dummy filter for the UI while the actual filter is applied through the TEDS query
	if(TedsOutlinerImpl->IsItemCompatible().IsBound())
	{
		return TedsOutlinerImpl->IsItemCompatible().Execute(InItem);
	}

	// The filter is applied through a TEDS query and this is just a dummy to activate it, so we can simply return true otherwise
	return false;
}
