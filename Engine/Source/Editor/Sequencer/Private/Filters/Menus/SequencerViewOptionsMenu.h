// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FSequencer;
class SSequencer;
class SWidget;
class USequencerMenuContext;
class UToolMenu;
enum class EFilterBarLayout : uint8;

class FSequencerViewOptionsMenu : public TSharedFromThis<FSequencerViewOptionsMenu>
{
public:
	TSharedRef<SWidget> CreateMenu(const TWeakPtr<FSequencer>& InSequencerWeak);

protected:
	void PopulateMenu(UToolMenu* const InMenu);
	void PopulateFiltersSection(UToolMenu& InMenu);
	void PopulateSortAndOrganizeSection(UToolMenu& InMenu);
	void PopulateFilterOptionssSection(UToolMenu& InMenu);
	void PopulateLayoutSection(UToolMenu& InMenu);

	bool IsFilterLayout(const EFilterBarLayout InLayout) const;
	void SetFilterLayout(const EFilterBarLayout InLayout);

	bool IsIncludePinnedInFilter() const;
	void ToggleIncludePinnedInFilter();

	bool IsAutoExpandPassedFilterNodes() const;
	void ToggleAutoExpandPassedFilterNodes();

	TSharedPtr<SSequencer> GetSequencerWidget() const;

	TWeakObjectPtr<USequencerMenuContext> CurrentContext;
};
