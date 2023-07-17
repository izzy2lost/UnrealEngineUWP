// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "IPlacementModeModule.h"

#if WITH_AUTOMATION_TESTS

// TestRail: C27425803
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterRiverBodyExposedInContentMenu, "Editor.Plugins.Tools.Water.RiverBodyExposedInContentMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWaterRiverBodyExposedInContentMenu::RunTest(const FString& Parameters)
{
	// Getting the module
	const IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	// Water Body River is only accessible through All Classes
	const FName PlacementModeCategoryHandle = TEXT("AllClasses");
	FString WaterBodyRiverName = TEXT("Water Body River");
	
	TArray<TSharedPtr<FPlaceableItem>> OutItems;
	// We need to manually load All Classes to search for Water Body River
	PlacementModeModule.Get().RegenerateItemsForCategory(PlacementModeCategoryHandle);

	PlacementModeModule.GetItemsForCategory(PlacementModeCategoryHandle, OutItems);
	bool RiverBodyFound = OutItems.ContainsByPredicate([&WaterBodyRiverName](const TSharedPtr<FPlaceableItem>& Item) {
		return Item->DisplayName.ToString().Contains(WaterBodyRiverName);
		});
	
	TestTrue(TEXT("Water Body River is not found in Create Menu"), RiverBodyFound);

	return true;
}

#endif