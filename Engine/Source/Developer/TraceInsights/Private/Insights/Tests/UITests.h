// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

//Driver
#include "IAutomationDriver.h"
#include "IAutomationDriverModule.h"
#include "DriverConfiguration.h"
#include "IDriverElement.h"
#include "IDriverSequence.h"
#include "LocateBy.h"

#include "Misc/AutomationTest.h"

DECLARE_LOG_CATEGORY_EXTERN(UITests, Log, All);

#if !WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHideAndShowAllTimingViewTabs, "Insights.HideAndShowAllTimingViewTabs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
BEGIN_DEFINE_SPEC(FAutomaticRenamingAndDeletingOfSymbolCacheFilesInsightsTest, "Insights.SessionBrowser.AutomaticRenamingAndDeletingOfSymbolCacheFiles", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
FAutomationDriverPtr Driver;
END_DEFINE_SPEC(FAutomaticRenamingAndDeletingOfSymbolCacheFilesInsightsTest)

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemoryFilterValueConverterTest, "Insights.FMemoryFilterValueConverterTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeFilterValueConverterTest, "Insights.FTimeFilterValueConverterTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

