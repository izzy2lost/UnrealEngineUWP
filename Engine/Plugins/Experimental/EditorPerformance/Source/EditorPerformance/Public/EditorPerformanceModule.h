// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "KPIValue.h"

class FSpawnTabArgs;
class SDockTab;
class SWidget;
class SWindow;

/**
 * The module holding all of the UI related pieces for EditorPerformance
 */
class EDITORPERFORMANCE_API FEditorPerformanceModule : public IModuleInterface
{
public:

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

	void UpdateKPIs();

	const FKPIRegistry& GetKPIRegistry() const;
	const FString& GetKPIProfileName() const;

	bool RecordInsightsSnaphshot(const FKPIValue& Value);
	bool RecordTelemetryEvent(const FKPIValue& Value);
	bool IsHotLocalCacheCase() const;

private:

	TSharedPtr<SWidget> CreatePerformanceReportDialog();
	TSharedRef<SDockTab> CreatePerformanceReportTab(const FSpawnTabArgs& Args);	
	TWeakPtr<SDockTab> PerformanceReportTab;

	void InitializeUI();
	void TerminateUI();

	void InitializeKPIs();
	void TerminateKPIs();

	FKPIRegistry					KPIRegistry;
	TMap<FString,FKPIProfile>		KPIProfiles;
	FString							KPIProfileName=TEXT("Default");
	FDateTime						LoadMapStartTime;
	FDateTime						PIEStartTime;
	FDateTime						PIEEndTime;
	float							BootToPIETime=0;
	float							EditorStartUpTime = 0;
	float							EditorLoadMapTime = 0;
	bool							EditorMapWasLoadedOnStartup = false;
	FString							EditorMapName=TEXT("Boot");
};


