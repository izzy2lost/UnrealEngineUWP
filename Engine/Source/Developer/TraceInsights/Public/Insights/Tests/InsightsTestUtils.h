// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

class TRACEINSIGHTS_API FInsightsTestUtils
{
public:
	FInsightsTestUtils(FAutomationTestBase* Test);

	bool AnalyzeTrace(const TCHAR* Path) const;
	bool FileContainsString(const FString& PathToFile, const FString& ExpectedString, const float Timeout) const;

private:
	FAutomationTestBase* Test;
};
