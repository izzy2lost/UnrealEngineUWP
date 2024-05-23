// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GauntletTestController.h"
#include "Engine/World.h"

#include "AutomatedPerfTestControllerBase.generated.h"

class AGameModeBase;

/**
 * 
 */
UCLASS()
class AUTOMATEDPERFTESTING_API UAutomatedPerfTestControllerBase : public UGauntletTestController
{
	GENERATED_BODY()
public:
	void OnPreWorldInitializeInternal(UWorld* World, const UWorld::InitializationValues IVS);
	virtual void OnPreWorldInitialize(UWorld* World);

	UFUNCTION()
	void TryEarlyExec(UWorld* const World);
	
	UFUNCTION()
	void OnWorldBeginPlay();

	UFUNCTION()
	void OnGameStateSet(AGameStateBase* const GameStateBase);

// Base functionality
public:
	UAutomatedPerfTestControllerBase(const FObjectInitializer& ObjectInitializer);

	static FString GetTestName();
	static FString GetDeviceProfile();
	static FString GetExplicitTestID();
	FString GetTestID();
	FString GetOverallRegionName();
	
	bool RequestsInsightsTrace() const;
	bool RequestsCSVProfiler() const;
	bool RequestsFPSChart() const;
	bool RequestsVideoCapture() const;

	bool TryStartInsightsTrace();
	bool TryStopInsightsTrace();

	bool TryStartCSVProfiler();
	bool TryStopCSVProfiler();
	
	bool TryStartFPSChart();
	bool TryStopFPSChart();

	bool TryStartVideoCapture();
	bool TryFinalizingVideoCapture(const bool bStopAutoContinue = false);

	virtual void SetupTest();
	virtual void RunTest();
	virtual void TeardownTest();
	virtual void Exit();
	
protected:
	// ~Begin UGauntletTestController Interface
	virtual void OnInit() override;
	virtual void OnTick(float TimeDelta) override;
	virtual void OnStateChange(FName OldState, FName NewState) override;
	virtual void OnPreMapChange() override;
	virtual void BeginDestroy() override;
	// ~End UGauntletTestController Interface

	UFUNCTION()
	virtual void EndAutomatedPerfTest(const int32 ExitCode = 0);

	UFUNCTION()
	virtual void OnVideoRecordingFinalized(bool Succeeded, const FString& FilePath);
	
	virtual void UnbindAllDelegates();

private:
	FString ExplicitTestID;

	FString TraceChannels;
	FString TestDatetime;
	FString TestID;
	FString DeviceProfileOverride;
	bool bRequestsFPSChart;
	bool bRequestsInsightsTrace;
	bool bRequestsCSVProfiler;
	bool bRequestsVideoCapture;

	FText VideoRecordingTitle;
	
	const TArray<FString> CmdsToExecEarly = { };
	
	AGameModeBase* GameMode;

	FDelegateHandle CsvProfilerDelegateHandle;
};
