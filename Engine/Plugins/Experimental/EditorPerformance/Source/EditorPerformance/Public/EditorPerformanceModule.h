// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "KPIValue.h"

class FSpawnTabArgs;
class SDockTab;
class SWidget;
class SWindow;
struct FTimerHandle;

/**
 * The module holding all of the UI related pieces for EditorPerformance
 */
class EDITORPERFORMANCE_API FEditorPerformanceModule : public IModuleInterface
{
public:

	enum EEditorState : uint8
	{
		Editor_Boot,
		Editor_Initialize,
		Editor_Interact,
		PIE_Startup,
		PIE_Interact,
		PIE_Shutdown,
	};

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	TSharedRef<SWidget>	CreateStatusBarWidget();
	void ShowPerformanceReportTab();

	void UpdateKPIs(float DeltaTime);

	const FKPIRegistry& GetKPIRegistry() const;
	const FString& GetKPIProfileName() const;

	bool RecordInsightsSnaphshot(const FKPIValue& Value);
	bool RecordTelemetryEvent(const FKPIValue& Value);
	bool IsHotLocalCacheCase() const;
	FEditorPerformanceModule::EEditorState GetEditorState() const;

private:

	TSharedPtr<SWidget> CreatePerformanceReportDialog();
	TSharedRef<SDockTab> CreatePerformanceReportTab(const FSpawnTabArgs& Args);	
	TWeakPtr<SDockTab> PerformanceReportTab;

	void InitializeUI();
	void TerminateUI();

	void InitializeKPIs();
	void TerminateKPIs();

	void HitchSamplerCallback();

	FKPIRegistry					KPIRegistry;
	TMap<FString,FKPIProfile>		KPIProfiles;
	FString							KPIProfileName=TEXT("Default");
	FDateTime						LoadMapStartTime;
	FDateTime						PIEStartTime;
	FDateTime						PIEEndTime;
	FDateTime						AssetRegistryScanStartTime;
	bool							IsFirstTimeToPIE = true;
	bool							IsLoadingMap = false;
	EEditorState					EditorState = EEditorState::Editor_Boot;
	float							BootToPIETime=0;
	float							EditorBootTime = 0;
	float							EditorStartUpTime = 0;
	float							EditorLoadMapTime = 0;
	float							EditorAssetRegistryScanTime = 0;
	uint32							EditorAssetRegistryScanCount = 0;
	FString							EditorMapName=TEXT("Boot");
	FTimerHandle					HitchSamplerTimerHandle;
	const float						HitchSamplerIntervalSeconds = 0.1f;
	const float						MinFPSForHitching = 5.0f;
	double							HitchAvergageFPS = 0;
	uint32							HitchSampleCount = 0;
	uint32							EditorHitchCount = 0;
	uint32							PIEHitchCount = 0;
	uint32							TotalPluginCount = 0;

	FGuid							EditorBootKPI;
	FGuid							EditorInitializeKPI;
	FGuid							EditorLoadMapKPI;
	FGuid							EditorHitchrateKPI;
	FGuid							EditorAssetRegistryScanKPI;
	FGuid							EditorPluginCountKPI;
	FGuid							TotalTimeToEditorKPI;
	FGuid							TotalTimeToPIEKPI;
	FGuid							PIEFirstTransitionKPI;
	FGuid							PIETransitionKPI;
	FGuid							PIEShutdownKPI;
	FGuid							PIEHitchrateKPI;
	FGuid							CloudDDCLatencyKPI;
	FGuid							CloudDDCReadSpeedKPI;
	FGuid							TotalDDCEfficiencyKPI;
	FGuid							LocalDDCEfficiencyKPI;
	FGuid							CoreCountKPI;
	FGuid							TotalMemoryKPI;
	FGuid							AvailableMemoryKPI;
};


