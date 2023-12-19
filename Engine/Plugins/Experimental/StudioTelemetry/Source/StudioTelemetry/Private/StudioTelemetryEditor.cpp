// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioTelemetryEditor.h"

#if WITH_EDITOR

#include "StudioTelemetry.h"
#include "AnalyticsTracer.h"
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

	// Install Editor Mode callbacks
	// 
	// Start Editor and Editor Boot span. Note : this will only start when the plugin is loaded and as such will miss any activity that runs beforehand
	EditorSpan = FStudioTelemetry::Get().StartSpan(EditorSpanName);
	EditorBootSpan = FStudioTelemetry::Get().StartSpan(EditorBootSpanName);

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), TEXT("EditorBoot")));
	Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), TEXT("EditorBoot")));

	EditorBootSpan->AddAttributes(Attributes);

	FEditorDelegates::OnMapLoad.AddLambda([this](const FString& MapName, FCanLoadMap& OutCanLoadMap)
		{
			// The Editor loads a new map
			EditorLoadMapSpan = FStudioTelemetry::Get().StartSpan(EditorLoadMapSpanName);
		});

	FEditorDelegates::OnMapOpened.AddLambda([this](const FString& MapName, bool Unused)
		{
			// The new editor map was actually opened
			LevelName = FPaths::GetBaseFilename(MapName);

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), LevelName));

			EditorLoadMapSpan->AddAttributes(Attributes);

			FStudioTelemetry::Get().EndSpan(EditorLoadMapSpan);

			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("LoadMap"), EditorLoadMapSpan->GetDuration(), EditorLoadMapSpan->GetAttributes());
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("LoadMap"), EditorLoadMapSpan->GetAttributes());
		});

	FEditorDelegates::OnEditorBoot.AddLambda([this](double TimeToBootEditor)
		{	
			FStudioTelemetry::Get().EndSpan(EditorBootSpan);

			// Callback is received when the editor has booted but has not been initialized
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("BootEditor"), EditorBootSpan->GetDuration(), EditorBootSpan->GetAttributes());
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("BootEditor"), EditorBootSpan->GetAttributes());

			EditorInitilizeSpan = FStudioTelemetry::Get().StartSpan(EditorInitilizeSpanName);
		});

	FEditorDelegates::OnEditorInitialized.AddLambda([this](double TimeToInitializeEditor)
		{
			TimeToStartEditor = TimeToInitializeEditor;

			// Editor has finished initializing so start the Editor Interact span
			FStudioTelemetry::Get().EndSpan(EditorInitilizeSpan);
			EditorInteractSpan = FStudioTelemetry::Get().StartSpan(EditorInteractSpanName);
			
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
						FStudioTelemetry::Get().StartSpan(OpenAssetEditorSpan);
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

						FStudioTelemetry::Get().EndSpan(OpenAssetEditorSpan, Attributes);			
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
				// Start the SlowTask span
				GWarn->OnStartSlowTask().AddLambda([this](const FText& TaskName)
					{	
						TArray<FAnalyticsEventAttribute> Attributes;
						Attributes.Emplace(TEXT("TaskName"), TaskName.ToString());

						FStudioTelemetry::Get().StartSpan(FName(TaskName.ToString()), Attributes);
					});

				// End the SlowTask span
				GWarn->OnFinalizeSlowTask().AddLambda([this](const FText& TaskName, double TaskDurationSeconds)
					{
						TSharedPtr<IAnalyticsSpan> SlowTaskSpan = FStudioTelemetry::Get().GetSpan(FName(TaskName.ToString()));

						if (SlowTaskSpan.IsValid())
						{
							FStudioTelemetry::Get().EndSpan(SlowTaskSpan);
							FStudioTelemetryEditor::RecordEvent_Loading(TEXT("SlowTask"), SlowTaskSpan->GetDuration(), SlowTaskSpan->GetAttributes());	
						}
					});
			}
		});

	// Install PIE Mode callbacks
	FEditorDelegates::BeginPIE.AddLambda([this](bool)
		{
			// PIE mode has been started. The user has pressed the Start PIE button.
			// Finish the Editor span
			FStudioTelemetry::Get().EndSpan(EditorSpan);

			// Start PIE span
			PIESpan = FStudioTelemetry::Get().StartSpan(PIESpanName);
			PIEStartupSpan = FStudioTelemetry::Get().StartSpan(PIEStartupSpanName);		
		});

	FWorldDelegates::OnPIEMapCreated.AddLambda([this](UGameInstance* GameInstance)
		{
			// A new PIE map was created
			PIELoadMapSpan = FStudioTelemetry::Get().StartSpan(PIELoadMapSpanName);
		});

	FWorldDelegates::OnPIEMapReady.AddLambda([this](UGameInstance* GameInstance)
		{
			// PIE map is now loaded and ready to use
			const FString MapName = FPaths::GetBaseFilename(GameInstance->PIEMapName);

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
			Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), MapName));

			PIELoadMapSpan->AddAttributes(Attributes);
			
			FStudioTelemetry::Get().EndSpan(PIELoadMapSpan);

			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("PIE.LoadMapTime"), PIELoadMapSpan->GetDuration(), PIELoadMapSpan->GetAttributes());
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("PIE.LoadMapTime"), PIELoadMapSpan->GetAttributes());
		});

	FWorldDelegates::OnPIEReady.AddLambda([this](UGameInstance* GameInstance)
		{
			// PIE is now ready for user interaction
			static bool IsFirstTimeToPIE = true;

			FStudioTelemetry::Get().EndSpan(PIEStartupSpan);

			// Record the time from start PIE to PIE
			FStudioTelemetryEditor::RecordEvent_Loading(TEXT("PIE.TotalStartupTime"), PIEStartupSpan->GetDuration(), PIEStartupSpan->GetAttributes());
			FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("PIE.TotalStartupTime"), PIEStartupSpan->GetAttributes());

			if (IsFirstTimeToPIE == true)
			{
				// Record the absolute time from editor boot to PIE
				FStudioTelemetryEditor::RecordEvent_Loading(TEXT("TimeToPIE"), TimeToStartEditor + EditorLoadMapSpan->GetDuration() + PIEStartupSpan->GetDuration(), PIEStartupSpan->GetAttributes());
				FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("TimeToPIE"), PIEStartupSpan->GetAttributes());

				IsFirstTimeToPIE = false;
			}
	
			// Start PIE World Streaming span
			if (GameInstance)
			{
				if (UWorld* World = GameInstance->GetWorld())
				{
					PIEWorldStreamingSpan = FStudioTelemetry::Get().StartSpan(PIEWorldStreamingSpanName);

					World->OnWorldMatchStarting.AddLambda([this]()
						{
							FStudioTelemetry::Get().EndSpan(PIEWorldStreamingSpan);
							FStudioTelemetryEditor::RecordEvent_Loading(TEXT("PIE.WorldStreaming"), PIEWorldStreamingSpan->GetDuration(), PIEWorldStreamingSpan->GetAttributes());	
						});
				}
			}	
		});

	FEditorDelegates::EndPIE.AddLambda([this](bool)
		{
			// PIE has ended, ie. the user has pressed the Stop PIE button, and we are going back to interactive Editor mode	
			FStudioTelemetry::Get().EndSpan(PIESpan);

			// Restart the Editor span
			EditorSpan = FStudioTelemetry::Get().StartSpan(EditorSpanName);
			EditorInteractSpan = FStudioTelemetry::Get().StartSpan(EditorInteractSpanName);
		});

	
	// Install Cooking Callbacks
	UE::Cook::FDelegates::CookByTheBookStarted.AddLambda([this](UE::Cook::ICookInfo& CookInfo)
	{
		// Begin the cooking span	
		CookingSpan = FStudioTelemetry::Get().StartSpan(TEXT("Cooking"));

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(FAnalyticsEventAttribute(TEXT("LevelName"), LevelName));
		Attributes.Emplace(FAnalyticsEventAttribute(TEXT("MapName"), LevelName));

		CookingSpan->AddAttributes(Attributes);
	});

	UE::Cook::FDelegates::CookByTheBookFinished.AddLambda([this](UE::Cook::ICookInfo& CookInfo)
	{
		// End the cooking span
	
		// Suppress sending telemetry from CookWorkers for now.
		uint32 MultiprocessId = 0;
		FParse::Value(FCommandLine::Get(), TEXT("-MultiprocessId="), MultiprocessId);
		if (MultiprocessId != 0)
		{
			return;
		}

		FStudioTelemetryEditor::RecordEvent_Cooking(CookingSpan->GetAttributes());
		FStudioTelemetryEditor::RecordEvent_CoreSystems(TEXT("Cooking"), CookingSpan->GetAttributes());

		FStudioTelemetry::Get().EndSpan(CookingSpan);
	});

	// Install Content Browser callbacks
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