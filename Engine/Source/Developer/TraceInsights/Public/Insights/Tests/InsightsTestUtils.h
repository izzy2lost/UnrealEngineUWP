// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

class FAutomationTestBase;

class TRACEINSIGHTS_API FInsightsTestUtils
{
public:
	FInsightsTestUtils(FAutomationTestBase* Test);

	bool AnalyzeTrace(const TCHAR* Path) const;
	bool FileContainsString(const FString& PathToFile, const FString& ExpectedString, const float& Timeout) const;
	bool SetupUTS(const float& Timeout) const;
	bool KillUTS(const float& Timeout) const;
	bool StartTracing(FTraceAuxiliary::EConnectionType ConnectionType, const float& Timeout) const;
	void ResetSession() const;

private:
	FAutomationTestBase* Test;
};
