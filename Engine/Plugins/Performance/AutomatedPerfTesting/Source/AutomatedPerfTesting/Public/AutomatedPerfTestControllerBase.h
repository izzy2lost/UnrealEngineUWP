// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GauntletTestController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#include "AutomatedPerfTestControllerBase.generated.h"

class AGameModeBase;


namespace AutomatedPerfTest
{
	static UWorld* FindCurrentWorld()
	{
		UWorld* World = nullptr;
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::Game)
			{
				World = WorldContext.World();
			}
#if WITH_EDITOR
			else if (GIsEditor && WorldContext.WorldType == EWorldType::PIE)
			{
				World = WorldContext.World();
				if (World)
				{
					return World;
				}
			}
#endif
		}

		return World;
	}
}
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

	FString GetTestName();
	FString GetDeviceProfile();
	virtual FString GetTestID();
	FString GetOverallRegionName();
	FString GetTraceChannels();
	
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
	virtual void TeardownTest(bool bExitAfterTeardown = true);
	virtual void TriggerExitAfterDelay();
	virtual void Exit();

	AGameModeBase* GetGameMode() const;
	
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
	FString TraceChannels;
	FString TestDatetime;
	FString TestName;
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
