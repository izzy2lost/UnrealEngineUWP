// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioTelemetryEditor.h"

#if WITH_EDITOR

#include "StudioTelemetry.h"
#include "AnalyticsFlowTracker.h"
#include "CollectionManagerModule.h"
#include "ContentBrowserModule.h"
#include "TelemetryRouter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "AssetRegistry/AssetRegistryTelemetry.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/FeedbackContext.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "Virtualization/VirtualizationSystem.h"
#include "FileHelpers.h"
#include "Experimental/ZenServerInterface.h"
#include "ContentBrowserTelemetry.h"
#include "ShaderStats.h"
#include "UObject/ICookInfo.h"
#include "IO/IoStoreOnDemand.h"

UE_DISABLE_OPTIMIZATION_SHIP

namespace Private
{
	const FName ContentBrowserModuleName = TEXT("ContentBrowser");
	
	// Json writer subclass to allow us to avoid using a SharedPtr to write basic Json.
	typedef TCondensedJsonPrintPolicy<TCHAR> FPrintPolicy;
	class FAnalyticsJsonWriter : public TJsonStringWriter<FPrintPolicy>
	{
	public:
		explicit FAnalyticsJsonWriter(FString* Out) : TJsonStringWriter<FPrintPolicy>(Out, 0)
		{
		}
	};
}

const TCHAR* LexToString(ECollectionTelemetryAssetAddedWorkflow Enum)
{
	switch(Enum)
	{
	case ECollectionTelemetryAssetAddedWorkflow::ContextMenu: return TEXT("ContextMenu");
	case ECollectionTelemetryAssetAddedWorkflow::DragAndDrop: return TEXT("DragAndDrop");
	default: return TEXT("");
	}
}
	
const TCHAR* LexToString(ECollectionTelemetryAssetRemovedWorkflow Enum)
{
	switch(Enum)
	{
	case ECollectionTelemetryAssetRemovedWorkflow::ContextMenu: return TEXT("ContextMenu");
	default: return TEXT("");
	}
}

template<typename T>
FString AnalyticsOptionalToStringOrNull(const TOptional<T>& Opt)
{
	return Opt.IsSet() ? AnalyticsConversionToString(Opt.GetValue()) : FString(TEXT("null"));
}

FStudioTelemetryEditor& FStudioTelemetryEditor::Get()
{
	static FStudioTelemetryEditor StudioTelemetryEditorInstance = FStudioTelemetryEditor();
	return StudioTelemetryEditorInstance;
}

void FStudioTelemetryEditor::RecordEvent_Cooking(TArray<FAnalyticsEventAttribute> Attributes)
{
#if ENABLE_COOK_STATS

	const int SchemaVersion = 3;

	Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);

	TMap<FString,FAnalyticsEventAttribute> CookAttributes;
	
	// Sends each cook stat to the studio analytics system.
	auto GatherAnalyticsAttributes = [&CookAttributes](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
	{
		for (const auto& Attr : StatAttributes)
		{
			const FString FormattedAttrName = (StatName + "_" + Attr.Key).Replace(TEXT("."), TEXT("_"));

			if (CookAttributes.Find(FormattedAttrName)==nullptr)
			{
				CookAttributes.Emplace(FormattedAttrName, Attr.Value.IsNumeric() ? FAnalyticsEventAttribute(FormattedAttrName, FCString::Atof(*Attr.Value)) : FAnalyticsEventAttribute(FormattedAttrName, Attr.Value));
			}
		}
	};

	// Now actually grab the stats 
	FCookStatsManager::LogCookStats(GatherAnalyticsAttributes);

	// Add the values to the attributes
	for (TMap<FString, FAnalyticsEventAttribute>::TConstIterator it(CookAttributes); it; ++it)
	{
		Attributes.Emplace((*it).Value);
	}

	// Gather the DDC summary stats
	FDerivedDataCacheSummaryStats SummaryStats;

	GatherDerivedDataCacheSummaryStats(SummaryStats);

	// Append to the attributes
	for (const FDerivedDataCacheSummaryStat& Stat : SummaryStats.Stats)
	{
		FString AttributeName = TEXT("DDC_Summary") + Stat.Key.Replace(TEXT("."), TEXT("_"));

		if (Stat.Value.IsNumeric())
		{
			Attributes.Emplace(AttributeName, FCString::Atof(*Stat.Value));
		}
		else
		{
			Attributes.Emplace(AttributeName, Stat.Value);
		}
	}

#if UE_WITH_ZEN
	// Gather Zen analytics
	if (UE::Zen::IsDefaultServicePresent())
	{
		UE::Zen::GetDefaultServiceInstance().GatherAnalytics(Attributes);
	}
#endif

	if (UE::Virtualization::IVirtualizationSystem::Get().IsEnabled())
	{
		// Gather Virtualization analytics
		UE::Virtualization::IVirtualizationSystem::Get().GatherAnalytics(Attributes);
	}

	FShaderStatsFunctions::GatherShaderAnalytics(Attributes);
	
	FStudioTelemetry::Get().RecordEvent(TEXT("Core.Cooking"), Attributes);
#endif
}

void FStudioTelemetryEditor::RecordEvent_Loading(const FString& LoadingName, double LoadingSeconds, TArray<FAnalyticsEventAttribute> Attributes )
{
	const int SchemaVersion = 3;

	Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
	Attributes.Emplace(TEXT("LoadingName"), LoadingName);
	Attributes.Emplace(TEXT("LoadingSeconds"), LoadingSeconds);
	
#if ENABLE_COOK_STATS

#if UE_WITH_ZEN
	// Gather Zen analytics
	if (UE::Zen::IsDefaultServicePresent())
	{
		UE::Zen::GetDefaultServiceInstance().GatherAnalytics(Attributes);
	}
#endif

	if (UE::Virtualization::IVirtualizationSystem::Get().IsEnabled())
	{
		// Gather Virtualization analytics
		UE::Virtualization::IVirtualizationSystem::Get().GatherAnalytics(Attributes);
	}

	// Gather the DDC summary stats
	FDerivedDataCacheSummaryStats SummaryStats;

	GatherDerivedDataCacheSummaryStats(SummaryStats);

	// Append to the attributes
	for (const FDerivedDataCacheSummaryStat& Stat : SummaryStats.Stats)
	{
		FString AttributeName = TEXT("DDC_Summary_") + Stat.Key.Replace(TEXT("."), TEXT("_"));

		if (Stat.Value.IsNumeric())
		{
			Attributes.Emplace(AttributeName, FCString::Atof(*Stat.Value));
		}
		else
		{
			Attributes.Emplace(AttributeName, Stat.Value);
		}
	}
#endif			

	FStudioTelemetry::Get().RecordEvent(TEXT("Core.Loading"), Attributes);
}

void FStudioTelemetryEditor::RecordEvent_DDCResource(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
#if ENABLE_COOK_STATS
	// Gather the latest resource stats
	TArray<FDerivedDataCacheResourceStat> ResourceStats;

	GatherDerivedDataCacheResourceStats(ResourceStats);

	const int SchemaVersion = 3;

	Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
	Attributes.Emplace(TEXT("LoadingName"), Context);
	
	// Send a resource event per asset type
	for (const FDerivedDataCacheResourceStat& Stat : ResourceStats)
	{
		const int64 TotalCount = Stat.BuildCount + Stat.LoadCount;
		const double TotalTimeSec = Stat.BuildTimeSec + Stat.LoadTimeSec;
		const int64 TotalSizeMB = Stat.BuildSizeMB + Stat.LoadSizeMB;

		if (Stat.AssetType.IsEmpty() || TotalCount==0)
		{
			// Empty asset type or nothing was built or loaded for this type
			continue;
		}
	
		TArray<FAnalyticsEventAttribute> EventAttributes = Attributes;

		EventAttributes.Emplace(TEXT("AssetType"), Stat.AssetType);
		EventAttributes.Emplace(TEXT("Load_Count"), Stat.LoadCount);
		EventAttributes.Emplace(TEXT("Load_TimeSec"), Stat.LoadTimeSec);
		EventAttributes.Emplace(TEXT("Load_SizeMB"), Stat.LoadSizeMB);
		EventAttributes.Emplace(TEXT("Build_Count"), Stat.BuildCount);
		EventAttributes.Emplace(TEXT("Build_TimeSec"), Stat.BuildTimeSec);
		EventAttributes.Emplace(TEXT("Build_SizeMB"), Stat.BuildSizeMB);
		EventAttributes.Emplace(TEXT("Total_Count"), TotalCount);
		EventAttributes.Emplace(TEXT("Total_TimeSec"), TotalTimeSec);
		EventAttributes.Emplace(TEXT("Total_SizeMB"), TotalSizeMB);
		EventAttributes.Emplace(TEXT("Efficiency"), double(Stat.LoadCount)/double(TotalCount) );
		EventAttributes.Emplace(TEXT("Thread_TimeSec"), Stat.GameThreadTimeSec);

		FStudioTelemetry::Get().RecordEvent(TEXT("Core.DDC.Resource"), EventAttributes);
	}
#endif			
}


void FStudioTelemetryEditor::RecordEvent_DDCSummary(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
#if ENABLE_COOK_STATS
	const int SchemaVersion = 3;

	Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
	Attributes.Emplace(TEXT("LoadingName"), Context);

	// Gather the summary stats
	FDerivedDataCacheSummaryStats SummaryStats;

	GatherDerivedDataCacheSummaryStats(SummaryStats);

	// Append to the attributes
	for (const FDerivedDataCacheSummaryStat& Stat : SummaryStats.Stats)
	{
		FString AttributeName = Stat.Key.Replace(TEXT("."), TEXT("_"));

		if (Stat.Value.IsNumeric())
		{
			Attributes.Emplace(AttributeName, FCString::Atof(*Stat.Value));
		}
		else
		{
			Attributes.Emplace(AttributeName, Stat.Value);
		}
	}

	FStudioTelemetry::Get().RecordEvent(TEXT("Core.DDC.Summary"), Attributes);
#endif			
}

void FStudioTelemetryEditor::RecordEvent_IAS(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
	// Gather the summary stats
	FDerivedDataCacheSummaryStats SummaryStats;

	using namespace UE::IO::IAS;
	if (FIoStoreOnDemandModule* OnDemandModule = FModuleManager::Get().GetModulePtr<FIoStoreOnDemandModule>("IoStoreOnDemand"))
	{
		if (OnDemandModule->IsEnabled())
		{
			const int SchemaVersion = 1;

			Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
			Attributes.Emplace(TEXT("LoadingName"), Context);

			OnDemandModule->ReportAnalytics(Attributes);
			FStudioTelemetry::Get().RecordEvent(TEXT("Core.IAS"), Attributes);
		}		
	}
}

void FStudioTelemetryEditor::RecordEvent_Zen(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
#if UE_WITH_ZEN
	// Gather Zen analytics
	if (UE::Zen::IsDefaultServicePresent())
	{
		const int SchemaVersion = 1;

		Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
		Attributes.Emplace(TEXT("LoadingName"), Context);

		UE::Zen::GetDefaultServiceInstance().GatherAnalytics(Attributes);
		FStudioTelemetry::Get().RecordEvent(TEXT("Core.Zen"), Attributes);
	}
#endif
}

void FStudioTelemetryEditor::RecordEvent_VirtualAssets(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
	if (UE::Virtualization::IVirtualizationSystem::Get().IsEnabled())
	{
		const int SchemaVersion = 1;

		Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
		Attributes.Emplace(TEXT("LoadingName"), Context);

		// Gather Virtualization analytics
		UE::Virtualization::IVirtualizationSystem::Get().GatherAnalytics(Attributes);

		FStudioTelemetry::Get().RecordEvent(TEXT("Core.VirtualAssets"), Attributes);
	}
}

void FStudioTelemetryEditor::RecordEvent_CoreSystems(const FString& Context, TArray<FAnalyticsEventAttribute> Attributes)
{
	FStudioTelemetryEditor::RecordEvent_DDCResource(Context, Attributes);
	FStudioTelemetryEditor::RecordEvent_DDCSummary(Context, Attributes);
	FStudioTelemetryEditor::RecordEvent_IAS(Context, Attributes);
	FStudioTelemetryEditor::RecordEvent_Zen(Context, Attributes);
	FStudioTelemetryEditor::RecordEvent_VirtualAssets(Context, Attributes);
}

void FStudioTelemetryEditor::RegisterCollectionWorkflowDelegates(FTelemetryRouter& Router)
{
	Router.OnTelemetry<FAssetAddedToCollectionTelemetryEvent>([](const FAssetAddedToCollectionTelemetryEvent& Event)
	{
		const int SchemaVersion = 1;
		
		FStudioTelemetry::Get().RecordEvent(TEXT("Editor.Collections.AssetsAdded"),
		{
			{ TEXT("SchemaVersion"), SchemaVersion},
			{ TEXT("DurationSec"), Event.DurationSec},
			{ TEXT("ObjectCount"), Event.NumAdded},
			{ TEXT("Workflow"), Event.Workflow},
			{ TEXT("CollectionShareType"), ECollectionShareType::ToString(Event.CollectionShareType)},
		});
	});

	Router.OnTelemetry<FAssetRemovedFromCollectionTelemetryEvent>([](const FAssetRemovedFromCollectionTelemetryEvent& Event)
	{
		const int SchemaVersion = 1;
		
		FStudioTelemetry::Get().RecordEvent(TEXT("Editor.Collections.AssetsRemoved"),
		{
			{ TEXT("SchemaVersion"), SchemaVersion},
			{ TEXT("DurationSec"), Event.DurationSec},
			{ TEXT("ObjectCount"), Event.NumRemoved},
			{ TEXT("Workflow"), Event.Workflow},
			{ TEXT("CollectionShareType"), ECollectionShareType::ToString(Event.CollectionShareType) },
		});
	});

	Router.OnTelemetry<FCollectionCreatedTelemetryEvent>([](const FCollectionCreatedTelemetryEvent& Event)
	{
		const int SchemaVersion = 1;

		FStudioTelemetry::Get().RecordEvent(TEXT("Editor.Collections.CollectionCreated"),
		{
			{ TEXT("SchemaVersion"), SchemaVersion},
			{ TEXT("DurationSec"), Event.DurationSec},
			{ TEXT("CollectionShareType"), ECollectionShareType::ToString(Event.CollectionShareType)},
		});
	});

	Router.OnTelemetry<FCollectionsDeletedTelemetryEvent>([](const FCollectionsDeletedTelemetryEvent& Event)
	{
		const int SchemaVersion = 1;

		FStudioTelemetry::Get().RecordEvent(TEXT("Editor.Collections.CollectionDeleted"),
		{
			{ TEXT("SchemaVersion"), SchemaVersion},
			{ TEXT("DurationSec"), Event.DurationSec},
			{ TEXT("ObjectCount"), Event.CollectionsDeleted},
		});
	});
}

void FStudioTelemetryEditor::Initialize()
{
	SessionStartTime = FPlatformTime::Seconds();

	TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

	if (FlowTracker.IsValid())
	{
		// Start Editor Boot Flow immediately
		EditorBootFlowGuid = FlowTracker->StartFlow(TEXT("Editor Boot"));
		FlowTracker->StartSubFlow(TEXT("Editor Boot"), EditorBootFlowGuid);
	}

	FEditorDelegates::BeginPIE.AddLambda([this](bool)
		{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				// Finish the Editor Interaction Flow
				FlowTracker->EndFlow(InteractiveEditorFlowGuid);

				// Start PIE Flow
				PIEStartTime = FPlatformTime::Seconds();
				PIEFlowGuid = FlowTracker->StartFlow(TEXT("Play In Editor"));
				PIEInitializeSubFlowGuid = FlowTracker->StartSubFlow(TEXT("PIE Initialize"), PIEFlowGuid);
			}			
		});

	FEditorDelegates::PostPIEStarted.AddLambda([this](bool)
		{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				// Called when PIE has finally started
				FlowTracker->EndSubFlow(PIEInitializeSubFlowGuid);
			}
		});

	FEditorDelegates::EndPIE.AddLambda([this](bool)
		{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				// End PIE Flow and any SubFlows within it
				FlowTracker->EndFlow(PIEFlowGuid, true);

				// Restart the Interactive Editor Flow
				FlowTracker->StartFlow(TEXT("Interactive Editor"));
			}
		});

	FWorldDelegates::OnPIEStarted.AddLambda([this](UGameInstance* GameInstance)
		{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				PIELoadMapStartTime = FPlatformTime::Seconds();
				PIELoadMapSubFlowGuid = FlowTracker->StartSubFlow(TEXT("PIE Load Map"), PIEFlowGuid);
			}
		});

	FWorldDelegates::OnPIEReady.AddLambda([this](UGameInstance* GameInstance)
		{	
			const FString MapName = FPaths::GetBaseFilename(GameInstance->PIEMapName);

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), MapName));
			
			// Currently there is no obvious way to know when PIE has finished loading everything but loading map FrontEnd is good enough for Epic products.
			if (MapName.Contains(TEXT("Frontend")))
			{
				static bool IsFirstTimeToPIE = true;

				PIEStartupTime = FPlatformTime::Seconds() - PIEStartTime;

				if (IsFirstTimeToPIE == true)
				{
					// Record the absolute time from editor boot to PIE
					FStudioTelemetryEditor::RecordEvent_Loading(TEXT("TimeToPIE"), EditorStartupTime + LoadMapTime + PIEStartupTime, Attributes);
					FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("TimeToPIE"), Attributes);

					IsFirstTimeToPIE = false;
				}
				
				// Record the time from start PIE to PIE
				FStudioTelemetryEditor::RecordEvent_Loading(TEXT("PIE.TotalStartupTime"), PIEStartupTime, Attributes);
				FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("PIE.TotalStartupTime"), Attributes);
			}

			PIELoadMapTime = FPlatformTime::Seconds() - PIELoadMapStartTime;

			// Record the sub map load time
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("PIE.LoadMapTime"), PIELoadMapTime, Attributes);
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("PIE.LoadMapTime"), Attributes);

			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				FlowTracker->EndSubFlow(PIELoadMapSubFlowGuid, true, Attributes);

				// Start tracking World Streaming
				if (UWorld* World = GameInstance->GetWorld())
				{
					FGuid WorldStreamingSubFlowGuid = FlowTracker->StartSubFlow(TEXT("World Streaming"));

					WorldStreamingStartTime = FPlatformTime::Seconds();

					World->OnWorldMatchStarting.AddLambda([this, WorldStreamingSubFlowGuid, MapName, FlowTracker]()
						{
							TArray<FAnalyticsEventAttribute> Attributes;
							Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
							Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), MapName));

							FlowTracker->EndSubFlow(WorldStreamingSubFlowGuid, true, Attributes);
							
							FStudioTelemetryEditor::RecordEvent_Loading(TEXT("WorldStreaming"), FPlatformTime::Seconds() - WorldStreamingStartTime, Attributes);
						});
				}
			}
		});

	FEditorFileUtils::GetOnLoadMapStartDelegate().AddLambda([this]()
		{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				// Load Map SubFlow will inherit the current Flow scope
				LoadMapStartTime = FPlatformTime::Seconds();
				LoadMapSubFlowGuid = FlowTracker->StartSubFlow(TEXT("Load Map"));
			}
		});

	FEditorFileUtils::GetOnLoadMapEndDelegate().AddLambda([this](const FString& MapName)
		{
			LevelName = MapName;

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), MapName));

			LoadMapTime = FPlatformTime::Seconds() - LoadMapStartTime;
			
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("LoadMap"), LoadMapTime, Attributes);
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("LoadMap"), Attributes);

			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				FlowTracker->EndSubFlow(LoadMapSubFlowGuid, true, Attributes);
			}
		});

	FEditorDelegates::OnEditorBoot.AddLambda([this](double TimeToBootEditor)
		{
			// Editor Boot Flow has finished
			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), TEXT("EditorBoot")));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), TEXT("EditorBoot")));
			
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("BootEditor"), TimeToBootEditor, Attributes);
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("BootEditor"), Attributes);

			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				FlowTracker->EndFlow(EditorBootFlowGuid, true, Attributes);

				// Start the Editor Interact Flow
				InteractiveEditorFlowGuid = FlowTracker->StartFlow(TEXT("Interactive Editor"));
			}
		});

	FEditorDelegates::OnEditorInitialized.AddLambda([this](double TimeToInitializeEditor)
		{
			EditorStartupTime = TimeToInitializeEditor;

			// Editor has initialized
			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), TEXT("EditorInitialize")));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), TEXT("EditorInitialize")));
			
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("TotalEditorStartup"), TimeToInitializeEditor, Attributes);
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("TotalEditorStartup"), Attributes);

			ensureMsgf(GEditor, TEXT("GEditor was not valid"));

			if (GEditor != nullptr)
			{
				// Install callbacks for Open Asset Dialogue
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetEditorRequestedOpen().AddLambda([this](UObject* Asset)
					{
						AssetOpenStartTime = FPlatformTime::Seconds();
						TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

						if (FlowTracker.IsValid())
						{
							FlowTracker->StartSubFlow(TEXT("Open Asset Editor"));
						}
					});

				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetOpenedInEditor().AddLambda([this](UObject* Asset, IAssetEditorInstance*)
					{
						TArray<FAnalyticsEventAttribute> Attributes;
						Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
						
						if (Asset != nullptr)
						{
							Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), Asset->GetFullName()));
							Attributes.Emplace(TEXT("AssetPath"), Asset->GetFullName());
							Attributes.Emplace(TEXT("AssetType"), Asset->GetClass()->GetName());

							FStudioTelemetryEditor::RecordEvent_Loading(TEXT("OpenAssetEditor"), FPlatformTime::Seconds() - AssetOpenStartTime, Attributes);
						}

						TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

						if (FlowTracker.IsValid())
						{
							FlowTracker->EndSubFlow(TEXT("Open Asset Editor"), true, Attributes);
						}
				});
			}

			ensureMsgf(GUnrealEd, TEXT("GUnrealEd was not valid"));
			
			if (GUnrealEd!=nullptr && GUnrealEd->CookServer != nullptr )
			{
				UE::Cook::FDelegates::CookByTheBookFinished.AddLambda([this](UE::Cook::ICookInfo& CookInfo)
					{
						TArray<FAnalyticsEventAttribute> Attributes;
						Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
						Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), LevelName));
						
						FStudioTelemetryEditor::RecordEvent_Cooking(Attributes);
						FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("Cooking"), Attributes);
					});
			}

			ensureMsgf(GWarn, TEXT("GWarn was not valid"));

			if (GWarn != nullptr)
			{
				GWarn->OnStartSlowTask().AddLambda([this](const FText& TaskName)
					{	
						TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

						if (FlowTracker.IsValid())
						{
							FlowTracker->StartSubFlow(FName(TaskName.ToString()));
						}
					});

				GWarn->OnFinalizeSlowTask().AddLambda([this](const FText& TaskName, double TaskDurationSeconds)
					{
						TArray<FAnalyticsEventAttribute> Attributes;
						Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
						Attributes.Emplace(TEXT("TaskName"), TaskName.ToString());
						
						TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

						if (FlowTracker.IsValid())
						{
							FlowTracker->EndSubFlow(FName(TaskName.ToString()), true, Attributes);
						}
						
						FStudioTelemetryEditor::RecordEvent_Loading(TEXT("SlowTaskDialog"), TaskDurationSeconds, Attributes);
					});
			}
		});
	
	// Install Cooking Callbacks
	UE::Cook::FDelegates::CookByTheBookStarted.AddLambda([this](UE::Cook::ICookInfo& CookInfo)
	{
			TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

			if (FlowTracker.IsValid())
			{
				CookByTheBookFlowGuid = FlowTracker->StartFlow("Cook By The Book");
			}
	});

	UE::Cook::FDelegates::CookByTheBookFinished.AddLambda([this](UE::Cook::ICookInfo& CookInfo)
	{
		// Suppress sending telemetry from CookWorkers for now.
		uint32 MultiprocessId = 0;
		FParse::Value(FCommandLine::Get(), TEXT("-MultiprocessId="), MultiprocessId);
		if (MultiprocessId != 0)
		{
			return;
		}

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
		Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), LevelName));

		TSharedPtr<FAnalyticsFlowTracker> FlowTracker = FStudioTelemetry::Get().GetFlowTracker().Pin();

		if (FlowTracker.IsValid())
		{
			FlowTracker->EndFlow(CookByTheBookFlowGuid, true, Attributes);
		}

		FStudioTelemetryEditor::RecordEvent_Cooking(Attributes);
		FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("Cooking"), Attributes);
	});

	FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>( TEXT("ContentBrowser") );
	
	FTelemetryRouter& Router = FTelemetryRouter::Get();
	{
		using namespace UE::Telemetry::ContentBrowser;
		Router.OnTelemetry<FBackendFilterTelemetry>([this](const FBackendFilterTelemetry& Data)
		{
			FString DataFilterText = LexToString(FJsonNull{});
			if (Data.DataFilter)
			{
				Private::FAnalyticsJsonWriter J(&DataFilterText);
				J.WriteObjectStart();
				J.WriteValue("RecursivePaths", Data.DataFilter->bRecursivePaths);
				J.WriteValue("ItemTypeFilter", UEnum::GetValueOrBitfieldAsString(Data.DataFilter->ItemTypeFilter));
				J.WriteValue("ItemCategoryFilter", UEnum::GetValueOrBitfieldAsString(Data.DataFilter->ItemCategoryFilter));
				J.WriteValue("ItemAttributeFilter", UEnum::GetValueOrBitfieldAsString(Data.DataFilter->ItemAttributeFilter));
				TArray<const UScriptStruct*> FilterTypes = Data.DataFilter->ExtraFilters.GetFilterTypes();
				if (FilterTypes.Num() > 0)
				{
					J.WriteArrayStart("FilterTypes");
					for (const UScriptStruct* Type : FilterTypes)
					{
						J.WriteValue(Type->GetPathName());
					}
					J.WriteArrayEnd();				
				}
				J.WriteObjectEnd();
				J.Close();
			}
			
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetView.BackendFilter"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("ViewCorrelationGuid"), Data.ViewCorrelationGuid },
				{ TEXT("FilterSessionCorrelationGuid"), Data.FilterSessionCorrelationGuid },
				{ TEXT("HasCustomItemSources"), Data.bHasCustomItemSources },
				{ TEXT("RefreshSourceItemsDurationSeconds"), Data.RefreshSourceItemsDurationSeconds },
				{ TEXT("NumBackendItems"), Data.NumBackendItems },
				{ TEXT("DataFilter"), FJsonFragment(MoveTemp(DataFilterText)) },
			});
		});

		Router.OnTelemetry<FFrontendFilterTelemetry>([this](const FFrontendFilterTelemetry& Data)
		{
			FString FilterText = LexToString(FJsonNull{});
			if (Data.FrontendFilters.IsValid() && Data.FrontendFilters->Num())
			{
				Private::FAnalyticsJsonWriter J(&FilterText);
				J.WriteArrayStart();
				for (int32 i=0; i < Data.FrontendFilters->Num(); ++i)
				{
					TSharedPtr<IFilter<FAssetFilterType>> Filter = Data.FrontendFilters->GetFilterAtIndex(i);
					J.WriteValue(Filter->GetName());
				}
				J.WriteArrayEnd();
				J.Close();
			}
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetView.FrontendFilter"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("ViewCorrelationGuid"), Data.ViewCorrelationGuid },
				{ TEXT("FilterSessionCorrelationGuid"), Data.FilterSessionCorrelationGuid },
				{ TEXT("TotalItemsToFilter"), Data.TotalItemsToFilter },
				{ TEXT("PriorityItemsToFilter"), Data.PriorityItemsToFilter },
				{ TEXT("TotalResults"), Data.TotalResults },
				{ TEXT("AmortizeDurationSeconds"), Data.AmortizeDuration },
				{ TEXT("WorkDurationSeconds"), Data.WorkDuration },
				{ TEXT("ResultLatency"), AnalyticsOptionalToStringOrNull(Data.ResultLatency) },
				{ TEXT("TimeUntilInteractionSeconds"), AnalyticsOptionalToStringOrNull(Data.TimeUntilInteraction) },
				{ TEXT("Completed"), Data.bCompleted },
				{ TEXT("FrontendFilters"), FJsonFragment(MoveTemp(FilterText)) },
			});
		});

		RegisterCollectionWorkflowDelegates(Router);
	}
	{
		using namespace UE::Telemetry::AssetRegistry;

		Router.OnTelemetry<FStartupTelemetry>([this](const FStartupTelemetry& Data){
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetRegistry.Startup"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("Duration"), Data.StartupDuration },
				{ TEXT("StartedAsyncGather"), Data.bStartedAsyncGather },
			});
		});
		Router.OnTelemetry<FSynchronousScanTelemetry>([this](const FSynchronousScanTelemetry& Data){
			if (Data.Duration < 0.5)
			{
				return;
			}
			FString DirectoriesText;
			{
				Private::FAnalyticsJsonWriter J(&DirectoriesText);
				J.WriteArrayStart();
				for (const FString& Directory : MakeArrayView(Data.Directories).Left(100))
				{
					J.WriteValue(Directory);
				}
				J.WriteArrayEnd();
				J.Close();
			}
			FString FilesText;
			{
				Private::FAnalyticsJsonWriter J(&FilesText);
				J.WriteArrayStart();
				for (const FString& File : MakeArrayView(Data.Files).Left(100))
				{
					J.WriteValue(File);
				}
				J.WriteArrayEnd();
				J.Close();
			}
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetRegistry.SynchronousScan"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("Directories"), FJsonFragment(MoveTemp(DirectoriesText)) },
				{ TEXT("Files"), FJsonFragment(MoveTemp(FilesText)) },
				{ TEXT("Flags"), LexToString(Data.Flags) },
				{ TEXT("NumFoundAssets"), Data.NumFoundAssets },
				{ TEXT("DurationSeconds"), Data.Duration },
				{ TEXT("InitialSearchStarted"), Data.bInitialSearchStarted },
				{ TEXT("InitialSearchCompleted"), Data.bInitialSearchCompleted },
			});
		});
		Router.OnTelemetry<FGatherTelemetry>([this](const FGatherTelemetry& Data){
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetRegistry.InitialScan"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("TotalDurationSeconds"), Data.TotalSearchDurationSeconds },
				{ TEXT("TotalWorkSeconds"), Data.TotalWorkTimeSeconds },
				{ TEXT("DiscoverySeconds"), Data.DiscoveryTimeSeconds },
				{ TEXT("GatherSeconds"), Data.GatherTimeSeconds },
				{ TEXT("StoreSeconds"), Data.StoreTimeSeconds },
				{ TEXT("NumCachedDirectories"), Data.NumCachedDirectories },
				{ TEXT("NumUncachedDirectories"), Data.NumUncachedDirectories },
				{ TEXT("NumCachedAssetFiles"), Data.NumCachedAssetFiles },
				{ TEXT("NumUncachedAssetFiles"), Data.NumUncachedAssetFiles },
			});
		});
		Router.OnTelemetry<FDirectoryWatcherUpdateTelemetry>([this](const FDirectoryWatcherUpdateTelemetry& Data){
			if (Data.DurationSeconds < 0.5)
			{
				return;
			}
			FStudioTelemetry::Get().RecordEvent(TEXT("Editor.AssetRegistry.DirectoryWatcherUpdate"), 
			{
				{ TEXT("SchemaVersion"), 1 },
				{ TEXT("NumChanges"), Data.Changes.Num() },
				{ TEXT("DurationSeconds"), Data.DurationSeconds },
				{ TEXT("InitialSearchStarted"), Data.bInitialSearchStarted },
				{ TEXT("InitialSearchCompleted"), Data.bInitialSearchCompleted },
			});
		});
	}
}

void FStudioTelemetryEditor::Shutdown()
{
}

UE_ENABLE_OPTIMIZATION_SHIP

#endif // WITH_EDITOR