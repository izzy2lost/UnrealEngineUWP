// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class SSequencerFilter;
class SWidget;
class USequencerFilterMenuContext;
class UToolMenu;

class FSequencerTrackFilterContextMenu
{
public:
	TSharedRef<SWidget> CreateMenuWidget(const TSharedRef<SSequencerFilter>& InFilterWidget);

protected:
	void PopulateMenu(UToolMenu* const InMenu);

	void PopulateFilterOptionsSection(UToolMenu& InMenu);
	void PopulateCustomFilterOptionsSection(UToolMenu& InMenu);
	void PopulateBulkOptionsSection(UToolMenu& InMenu);

	FText GetFilterDisplayName() const;

	void OnDisableFilter();
	void OnResetFilters();

	void OnActivateWithFilterException();

	void OnActivateAllFilters(const bool bInActivate);

	void OnEditFilter();
	void OnDeleteFilter();

	TWeakObjectPtr<USequencerFilterMenuContext> CurrentContext;
};
