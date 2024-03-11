// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorPerformanceModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "SEditorPerformanceDialogs.h"
#include "SEditorPerformanceStatusBar.h"
#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "KPIValue.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "Editor/EditorPerformanceSettings.h"
#include "DerivedDataCacheUsageStats.h"
#include "Trace/Trace.h"
#include "StudioTelemetry.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "EditorPerformance"
 
IMPLEMENT_MODULE(FEditorPerformanceModule, EditorPerformance );
 
static const FName EditorPerformanceReportTabName = FName(TEXT("EditorPerformanceReportTab"));

void FEditorPerformanceModule::StartupModule()
{
	InitializeKPIs();
}

void FEditorPerformanceModule::ShutdownModule()
{
	TerminateUI();
	TerminateKPIs();
}

void FEditorPerformanceModule::InitializeUI()
{
	UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

	// Check if we want to have the tool enabled or not.
	if (EditorPerformanceSettings && EditorPerformanceSettings->bEnableEditorPeformanceTool==false )
	{
		return;
	}

	// Populate the notification list with all KPI values if it is empty. 
	if (EditorPerformanceSettings && EditorPerformanceSettings->NotificationList.IsEmpty())
	{
		for (FKPIValues::TConstIterator It(KPIRegistry.GetKPIValues()); It; ++It)
		{
			EditorPerformanceSettings->NotificationList.Emplace(It->Key);
		}

		EditorPerformanceSettings->PostEditChange();
		EditorPerformanceSettings->SaveConfig();
	}

	const FSlateIcon PerformanceReportIcon(FAppStyle::GetAppStyleSetName(), "EditorPerformance.Report.Panel");

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EditorPerformanceReportTabName, FOnSpawnTab::CreateRaw(this, &FEditorPerformanceModule::CreatePerformanceReportTab))
		.SetDisplayName(LOCTEXT("EditorPerformanceReportTabTitle", "Editor Performance"))
		.SetTooltipText(LOCTEXT("EditorPerformanceReportTabToolTipText", "Opens the Editor Performance Report tab."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsProfilingCategory())
		.SetIcon(PerformanceReportIcon);

#if WITH_RELOAD
	// This code attempts to relaunch the tabs when you reload this module
	if (IsReloadActive() && FSlateApplication::IsInitialized())
	{
		ShowPerformanceReportTab();
	}
#endif // WITH_RELOAD

	FEditorPerformanceStatusBarMenuCommands::Register();

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.StatusBar.ToolBar");

	FToolMenuSection& EditorPerfSection = Menu->AddSection("Perf", FText::GetEmpty(), FToolMenuInsert("DDC", EToolMenuInsertType::Before));

	EditorPerfSection.AddEntry(FToolMenuEntry::InitWidget("EditorPerformanceStatusBar", CreateStatusBarWidget(), FText::GetEmpty(), true, false));
}

void FEditorPerformanceModule::TerminateUI()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EditorPerformanceReportTabName);

		if (PerformanceReportTab.IsValid())
		{
			PerformanceReportTab.Pin()->RequestCloseTab();
		}
	}

	FEditorPerformanceStatusBarMenuCommands::Unregister();
}

TSharedRef<SWidget> FEditorPerformanceModule::CreateStatusBarWidget()
{
	return SNew(SEditorPerformanceStatusBarWidget);
}

TSharedPtr<SWidget> FEditorPerformanceModule::CreatePerformanceReportDialog()
{
	return SNew(SEditorPerformanceReportDialog);
}


TSharedRef<SDockTab> FEditorPerformanceModule::CreatePerformanceReportTab(const FSpawnTabArgs& Args)
{
	return SAssignNew(PerformanceReportTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CreatePerformanceReportDialog().ToSharedRef()
		];
}

void FEditorPerformanceModule::ShowPerformanceReportTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(EditorPerformanceReportTabName));
}

const FName EditorCategoryName = TEXT("Editor");
const FName PIECategoryName = TEXT("PIE");
const FName DDCCategoryName = TEXT("Cache");
const FName HardwareCategoryName = TEXT("Hardware");

const FName EditorBootKPIName = TEXT("Boot");
const FName EditorStartupKPIName = TEXT("Startup");
const FName EditorLoadMapKPIName = TEXT("Load Map");
const FName TotalTimeToPIEKPIName = TEXT("Total Time To PIE");
const FName PIEFirstTransitionKPIName = TEXT("First Transition");
const FName PIETransitionKPIName = TEXT("Iterative Transition");
const FName PIEShutdownKPIName = TEXT("Shutdown");
const FName CloudDDCLatencyKPIName = TEXT("Unreal Cloud DDC Latency");
const FName CloudDDCReadSpeedKPIName = TEXT("Unreal Cloud DDC Speed");
const FName TotalDDCEfficiencyKPIName = TEXT("Effective Efficiency");
const FName LocalDDCEfficiencyKPIName = TEXT("Local Efficiency");
const FName CoreCountKPIName = TEXT("Core Count");
const FName TotalMemoryKPIName = TEXT("Total Memory");
const FName AvailableMemoryKPIName = TEXT("Available Memory");
const FName HitchrateKPIName = TEXT("Hitch Rate");

float EditorBootKPILimit = 100;
float EditorStartupKPILimit = 160;
float EditorLoadMapKPILimit = 120;
float PIEFirstTransitionKPILimit = 220;
float PIETransitionKPILimit = 40;
float PIEShutdownKPILimit = 10;
float TotalTimeToPIEKPILimit = 600;
float CloudDDCLatencyKPILimit = 100;
float CloudDDCReadSpeedKPILimit = 10;
float TotalDDCEffciencyKPILimit = 90;
float LocalDDCEffciencyKPILimit = 85;
float HitchrateKPILimit = 25;
float CoreCountKPILimit = 32;
float TotalMemoryKPILimit = 64;
float AvailableMemoryKPILimit = 16;

void FEditorPerformanceModule::InitializeKPIs()
{
	// Declare the KPIs 
	KPIRegistry.DeclareKPIValue(EditorCategoryName, EditorBootKPIName, 0.0, EditorBootKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(EditorCategoryName, EditorStartupKPIName, 0.0, EditorStartupKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(EditorCategoryName, EditorLoadMapKPIName, 0.0, EditorLoadMapKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(PIECategoryName, PIEFirstTransitionKPIName, 0.0, PIEFirstTransitionKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(PIECategoryName, PIETransitionKPIName, 0.0, PIETransitionKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(PIECategoryName, PIEShutdownKPIName, 0.0, PIEShutdownKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(PIECategoryName, TotalTimeToPIEKPIName, 0.0, TotalTimeToPIEKPILimit, FKPIValue::LessThan, FKPIValue::Minutes);
	KPIRegistry.DeclareKPIValue(DDCCategoryName, CloudDDCLatencyKPIName, 0.0, CloudDDCLatencyKPILimit, FKPIValue::LessThan, FKPIValue::Milliseconds);
	KPIRegistry.DeclareKPIValue(DDCCategoryName, CloudDDCReadSpeedKPIName, 100.0, CloudDDCReadSpeedKPILimit, FKPIValue::GreaterThan, FKPIValue::MegaBitsPerSecond);
	KPIRegistry.DeclareKPIValue(DDCCategoryName, TotalDDCEfficiencyKPIName, 100.0, TotalDDCEffciencyKPILimit, FKPIValue::GreaterThan, FKPIValue::Percent);
	KPIRegistry.DeclareKPIValue(DDCCategoryName, LocalDDCEfficiencyKPIName, 100.0, LocalDDCEffciencyKPILimit, FKPIValue::GreaterThan, FKPIValue::Percent);
	//KPIRegistry.DeclareKPIValue(HitchrateKPIName, 0.0, HitchrateKPILimit, FKPIValue::LessThan, FKPIValue::Percent);
	KPIRegistry.DeclareKPIValue(HardwareCategoryName, CoreCountKPIName, 128.0, CoreCountKPILimit, FKPIValue::GreaterThanOrEqual, FKPIValue::Decimal);
	KPIRegistry.DeclareKPIValue(HardwareCategoryName, TotalMemoryKPIName, 128.0, TotalMemoryKPILimit, FKPIValue::GreaterThanOrEqual, FKPIValue::GigaBytes);
	KPIRegistry.DeclareKPIValue(HardwareCategoryName, AvailableMemoryKPIName, 128.0, AvailableMemoryKPILimit, FKPIValue::GreaterThanOrEqual, FKPIValue::GigaBytes);

	// Load the KPI profiles
	KPIRegistry.LoadKPIProfiles(TEXT("EditorPerformance.Profile"), GEditorIni);

	// Load the KPI hints
	KPIRegistry.LoadKPIHints(TEXT("EditorPerformance.Hints"), GEditorIni);

	// Apply any non map specific profiles
	for (FKPIProfiles::TConstIterator It(KPIRegistry.GetKPIProfiles()); It; ++It)
	{
		const FKPIProfile& Profile = It->Value;

		if (Profile.MapName.IsEmpty())
		{
			KPIProfileName = It->Key;
			KPIRegistry.ApplyKPIProfile(It->Value);
		}
	}

	// Gather hardware stats
	KPIRegistry.SetKPIValue(CoreCountKPIName, float(FPlatformMisc::NumberOfCores()));
	KPIRegistry.SetKPIValue(TotalMemoryKPIName, static_cast<float>(FPlatformMemory::GetStats().TotalPhysical) / (1024.0f * 1024.0f * 1024.0f));

	// Register the delegates
	FEditorDelegates::OnEditorBoot.AddLambda([this](double TimeToBootEditor )
		{
			KPIRegistry.SetKPIValue(EditorBootKPIName, (float)TimeToBootEditor);
		});

	FEditorDelegates::OnEditorInitialized.AddLambda([this](double TimeToInitializeEditor)
		{
			EditorStartUpTime = (float)TimeToInitializeEditor;
			EditorMapWasLoadedOnStartup = EditorLoadMapTime > 0.0 ? true : false;

			KPIRegistry.SetKPIValue(EditorStartupKPIName, EditorStartUpTime);

			InitializeUI();
		});

	FEditorDelegates::OnMapLoad.AddLambda([this](const FString& MapName, FCanLoadMap& OutCanLoadMap)
		{
			LoadMapStartTime = FDateTime::UtcNow();
		});

	FEditorDelegates::OnMapOpened.AddLambda([this](const FString& MapName, bool Unused)
		{
			EditorMapName = FPaths::GetBaseFilename(MapName);
			EditorLoadMapTime = float((FDateTime::UtcNow() - LoadMapStartTime).GetTotalSeconds());
			BootToPIETime += EditorLoadMapTime;
			KPIRegistry.SetKPIValue(EditorLoadMapKPIName, EditorLoadMapTime);
	
			// Apply any profile that matches the currently loaded map
			for (FKPIProfiles::TConstIterator It(KPIRegistry.GetKPIProfiles()); It; ++It)
			{
				const FKPIProfile& Profile = It->Value;

				if (Profile.MapName == EditorMapName)
				{
					KPIProfileName = It->Key;
					KPIRegistry.ApplyKPIProfile(It->Value);
				}
			}
		});

	FEditorDelegates::StartPIE.AddLambda([this](bool)
		{
			PIEStartTime = FDateTime::UtcNow();
		});

	FWorldDelegates::OnPIEReady.AddLambda([this](UGameInstance* GameInstance)
		{
			static bool IsFirstTimeToPIE = true;

			const float PIETransitionTime = float((FDateTime::UtcNow() - PIEStartTime).GetTotalSeconds());

			if (IsFirstTimeToPIE)
			{
				BootToPIETime = EditorStartUpTime + ( EditorMapWasLoadedOnStartup? EditorLoadMapTime : 0.0f ) + PIETransitionTime;

				KPIRegistry.SetKPIValue(PIEFirstTransitionKPIName, PIETransitionTime);
				KPIRegistry.SetKPIValue(TotalTimeToPIEKPIName, BootToPIETime);
			}
			else
			{
				KPIRegistry.SetKPIValue(PIETransitionKPIName, PIETransitionTime);
			}

			IsFirstTimeToPIE = false;
		});

	FEditorDelegates::EndPIE.AddLambda([this](bool)
		{
			PIEEndTime = FDateTime::UtcNow();
		});

	FEditorDelegates::ShutdownPIE.AddLambda([this](bool)
		{
			const float PIEShutdownTime = float((FDateTime::UtcNow() - PIEEndTime).GetTotalSeconds());
			KPIRegistry.SetKPIValue(PIEShutdownKPIName, PIEShutdownTime);
		});
}

void FEditorPerformanceModule::UpdateKPIs()
{
	// Gather live hardware stats
	KPIRegistry.SetKPIValue(AvailableMemoryKPIName, static_cast<float>(FPlatformMemory::GetStats().AvailablePhysical)/(1024.0f * 1024.0f * 1024.0f));
	
	// Gather the DDC summary stats
	FDerivedDataCacheSummaryStats SummaryStats;
	GatherDerivedDataCacheSummaryStats(SummaryStats);

	int64 CloudGetHits = 0;

	for (const FDerivedDataCacheSummaryStat& Stat : SummaryStats.Stats)
	{
		if (Stat.Key == TEXT("CloudGetHits"))
		{
			CloudGetHits = FCString::Atoi(*Stat.Value);
		}
		else if (Stat.Key == TEXT("CloudLatency"))
		{
			const float Value = FCString::Atof(*Stat.Value);

			if (Value > 0.0f)
			{
				KPIRegistry.SetKPIValue(CloudDDCLatencyKPIName, Value);
			}
		}
		else if (Stat.Key == TEXT("CloudReadSpeed"))
		{
			const float Value = FCString::Atof(*Stat.Value) * 8.0f;

			if (Value > 0.0f)
			{
				KPIRegistry.SetKPIValue(CloudDDCReadSpeedKPIName, Value);
			}
		}
		else if (Stat.Key == TEXT("TotalGetHitPct"))
		{
			const float Value = FCString::Atof(*Stat.Value) * 100.0f;

			if (Value > 0.0f)
			{
				KPIRegistry.SetKPIValue(TotalDDCEfficiencyKPIName, Value);
			}
		}
		else if (Stat.Key == TEXT("LocalGetHitPct"))
		{
			const float Value = FCString::Atof(*Stat.Value) * 100.0f;

			if (Value > 0.0f)
			{
				KPIRegistry.SetKPIValue(LocalDDCEfficiencyKPIName, Value);
			}
		}
	}

	if (CloudGetHits == 0)
	{
		KPIRegistry.InvalidateKPIValue(CloudDDCLatencyKPIName);
		KPIRegistry.InvalidateKPIValue(CloudDDCReadSpeedKPIName);
	}
	
}

bool FEditorPerformanceModule::IsHotLocalCacheCase() const
{
	FKPIValue KPIValue;

	if (KPIRegistry.GetKPIValue(LocalDDCEfficiencyKPIName, KPIValue))
	{
		return KPIValue.GetState()==FKPIValue::Good;
	}

	return false;
}

bool FEditorPerformanceModule::RecordInsightsSnaphshot(const FKPIValue& KPIValue)
{
	// Create the snapshot file
	FString FileName = KPIValue.Name.ToString() + TEXT(".utrace");
	FString FolderPath = FPaths::ProjectSavedDir() / TEXT("EditorPerformance");

	// Create the full output path
	FString FilePath = FolderPath / FileName;

	// Delete the existing trace file if it already exists
	if (FPaths::FileExists(FilePath))
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath))
		{
			// File existed but could not be overwritten
			return false;
		}
	}
		
	// Write the snapshot to a file
	return FTraceAuxiliary::WriteSnapshot(*FilePath);
}

bool FEditorPerformanceModule::RecordTelemetryEvent(const FKPIValue& KPIValue)
{
	// Record a new telemetry event for this KPI
	if (FStudioTelemetry::IsAvailable())
	{
		const int SchemaVersion = 2;
		TArray<FAnalyticsEventAttribute> Attributes;

		Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
		Attributes.Emplace(TEXT("MapName"), EditorMapName);
		Attributes.Emplace(TEXT("DDC_IsHotLocalCache"), IsHotLocalCacheCase());
		Attributes.Emplace(TEXT("KPI_Name"), *KPIValue.Name.ToString());
		Attributes.Emplace(TEXT("KPI_Category"), *KPIValue.Category.ToString());
		Attributes.Emplace(TEXT("KPI_CurrentValue"), KPIValue.CurrentValue);
		Attributes.Emplace(TEXT("KPI_ThresholdValue"), KPIValue.ThresholdValue);
		Attributes.Emplace(TEXT("KPI_DisplayType"), *FKPIValue::GetDisplayTypeAsString(KPIValue.DisplayType));
		Attributes.Emplace(TEXT("KPI_Profile"), KPIProfileName);
		
		FStudioTelemetry::Get().RecordEvent(TEXT("Editor.PerformanceWarning"), Attributes);

		return true;
	}

	return false;
}

void FEditorPerformanceModule::TerminateKPIs()
{
}

const FKPIRegistry& FEditorPerformanceModule::GetKPIRegistry() const
{
	return KPIRegistry;
}

const FString& FEditorPerformanceModule::GetKPIProfileName() const
{
	return KPIProfileName;
}

#undef LOCTEXT_NAMESPACE
