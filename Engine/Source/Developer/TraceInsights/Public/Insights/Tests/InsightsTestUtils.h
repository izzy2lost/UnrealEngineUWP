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
	bool FileContainsString(const FString& PathToFile, const FString& ExpectedString, double Timeout) const;
	bool SetupUTS(double Timeout) const;
	bool KillUTS(double Timeout) const;
	bool StartTracing(FTraceAuxiliary::EConnectionType ConnectionType, double Timeout) const;
	void ResetSession() const;

private:
	FAutomationTestBase* Test;
};
