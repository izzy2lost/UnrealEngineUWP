// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMediaEditorModule.h"
#include "AvaMediaEditorStyle.h"
#include "Broadcast/AvaBroadcastEditor.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvaMediaModule.h"
#include "LevelEditor.h"
#include "LevelEditor/AvaLevelEditorUtils.h"
#include "LevelEditorViewport.h"
#include "OutputDevices/AvaMediaIOOutputConfigurationCustomization.h"
#include "Playback/AvaPlaybackCommands.h"
#include "Playback/Graph/AvaPlaybackConnectionDrawingPolicy.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaRundownEditorSettings.h"
#include "Playlist/AvaRundownMacroCollection.h"
#include "Playlist/Customization/AvaRundownMacroCommandCustomization.h"
#include "Playlist/Customization/AvaRundownMacroKeyBindingCustomization.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterChannelExpressionFactory.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterIdExpressionFactory.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterNameExpressionFactory.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterPathExpressionFactory.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterStatusExpressionFactory.h"
#include "Playlist/Factories/Filters/AvaPlaylistFilterTransitionLayerExpressionFactory.h"
#include "Playlist/Factories/Filters/IAvaPlaylistFilterExpressionFactory.h"
#include "Playlist/Factories/Filters/IAvaPlaylistFilterSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterChannelSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterIdSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterNameSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterPathSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterStatusSuggestionFactory.h"
#include "Playlist/Factories/Suggestions/AvaPlaylistFilterTransitionLayerSuggestionFactory.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AvaMediaEditorModule"

DEFINE_LOG_CATEGORY(LogAvaMediaEditor);

namespace UE::AvaMediaEditorModule::Private
{
	namespace BroadcastEditorEntry
	{
		static const FName MenuName(TEXT("LevelEditor.StatusBar.ToolBar"));
		static const FName SectionName(TEXT("AvalancheMedia"));
	}
	
	// Command line parsing helper.
	bool IsPlaylistServerManuallyStarted(FString& OutPlaylistServerName)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("AvaMediaPlaylistServerStart="), OutPlaylistServerName) ||
			FParse::Param(FCommandLine::Get(), TEXT("AvaMediaPlaylistServerStart"));
	}

	static void GetEditorViewportClient(FCommonViewportClient** OutViewportClient)
	{
		// Replicating the logic from UUnrealEdEngine::Exec.
		FCommonViewportClient* ViewportClient = nullptr;
		if (!GStatProcessingViewportClient && (!GEngine->GameViewport || GEngine->GameViewport->IsSimulateInEditorViewport() ))
		{
			ViewportClient = GLastKeyLevelEditingViewportClient ? GLastKeyLevelEditingViewportClient : GCurrentLevelEditingViewportClient;
		}

		if (ViewportClient)
		{
			*OutViewportClient = ViewportClient;
		}
	}
}

void FAvaMediaEditorModule::StartupModule()
{
	using namespace UE::AvaMediaEditorModule::Private;

	InitExtensibilityManagers();

	FAvaPlaybackCommands::Register();
	FAvaPlaylistCommands::Register();

	FAvaMediaEditorStyle::Initialize();

	if (FSlateApplication::IsInitialized())
	{
		AddEditorToolbarButtons();
	}

	// Register the Avalanche Playback Graph connection policy with the graph editor
	PlaybackConnectionFactory = MakeShared<FAvaPlaybackConnectionDrawingPolicyFactory>();
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(PlaybackConnectionFactory);

	// MediaIOEditor module is loaded in PostEngineInit phase,
	// so we need in order to have our customizations override theirs, we need to
	// register ours after, i.e. once all modules are loaded.
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FAvaMediaEditorModule::RegisterCustomizations);
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAvaMediaEditorModule::PostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FAvaMediaEditorModule::EnginePreExit);

	// Register Map Change Events
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FAvaMediaEditorModule::HandleMapChanged);

	ConsoleCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AvaPlaylistServer.Start"),
		TEXT("Starts the playlist server."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaMediaEditorModule::StartPlaylistServerCommand),
		ECVF_Default
	));
	ConsoleCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AvaPlaylistServer.Stop"),
		TEXT("Stops the playlist server."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaMediaEditorModule::StopPlaylistServerCommand),
		ECVF_Default
	));

	IAvaMediaModule::Get().GetEditorViewportClientDelegate().BindStatic(&GetEditorViewportClient);

	FString DummyServerName;
	if (IsPlaylistServerManuallyStarted(DummyServerName))
	{
		// Prevent throttling when the server is started.
		// This has to be done before any SLevelViewport are ticked since the cvar value is cached on first tick.
		static const FSlateThrottleManager& ThrottleManager = FSlateThrottleManager::Get();
		if (IConsoleVariable* AllowThrottling = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.bAllowThrottling")))
		{
			AllowThrottling->Set(0);
			UE_LOG(LogAvaMediaEditor, Log, TEXT("Setting Slate.bAllowThrottling to false."));
		}
	}
	RegisterPlaylistFilterExpressionFactories();
	RegisterPlaylistFilterSuggestionFactories();
}

void FAvaMediaEditorModule::ShutdownModule()
{
	StopAllServices();

	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

	// Unregister Map Change Events
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnMapChanged().RemoveAll(this);
	}

	ResetExtensibilityManagers();
	if (FSlateApplication::IsInitialized())
	{
		RemoveEditorToolbarButtons();
	}
	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UnregisterCustomizations();
	}

	FAvaMediaEditorStyle::Shutdown();
	FAvaPlaybackCommands::Unregister();
	FAvaPlaylistCommands::Unregister();

	if (PlaybackConnectionFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinConnectionFactory(PlaybackConnectionFactory);
		PlaybackConnectionFactory.Reset();
	}

	for (IConsoleObject* ConsoleCmd : ConsoleCmds)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleCmd);
	}
	ConsoleCmds.Empty();
}

TSharedPtr<FExtensibilityManager> FAvaMediaEditorModule::GetBroadcastToolBarExtensibilityManager()
{
	return BroadcastToolBarExtensibility;
}

TSharedPtr<FExtensibilityManager> FAvaMediaEditorModule::GetPlaybackToolBarExtensibilityManager()
{
	return PlaybackToolBarExtensibility;
}

TSharedPtr<FExtensibilityManager> FAvaMediaEditorModule::GetPlaylistToolBarExtensibilityManager()
{
	return PlaylistToolBarExtensibility;
}

TSharedPtr<FExtensibilityManager> FAvaMediaEditorModule::GetPlaylistMenuExtensibilityManager()
{
	return PlaylistMenuExtensibility;
}

FSlateIcon FAvaMediaEditorModule::GetToolbarBroadcastButtonIcon() const
{
	const IAvaMediaModule& MediaModule = IAvaMediaModule::Get();

	if (MediaModule.IsMediaPlaybackClientStarted())
	{
		return FSlateIcon(FAvaMediaEditorStyle::GetStyleSetName(), "AvalancheMediaEditor.BroadcastClient", "AvalancheMediaEditor.BroadcastClient.Small");
	}
	else if (MediaModule.IsMediaPlaybackServerStarted())
	{
		return FSlateIcon(FAvaMediaEditorStyle::GetStyleSetName(), "AvalancheMediaEditor.BroadcastServer", "AvalancheMediaEditor.BroadcastServer.Small");
	}
	return FSlateIcon(FAvaMediaEditorStyle::GetStyleSetName(), "AvalancheMediaEditor.BroadcastIcon");
}

bool FAvaMediaEditorModule::CanFilterSupportComparisonOperation(const FName& InFilterKey, ETextFilterComparisonOperation InOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	if (const TSharedPtr<IAvaPlaylistFilterExpressionFactory>* FilterExpressionFactory = FilterExpressionFactories.Find(InFilterKey))
	{
		return FilterExpressionFactory->Get()->SupportsComparisonOperation(InOperation, InPlaylistSearchListType);
	}
	return false;
}

bool FAvaMediaEditorModule::FilterExpression(const FName& InFilterKey, const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	if (const TSharedPtr<IAvaPlaylistFilterExpressionFactory>* FilterExpressionFactory = FilterExpressionFactories.Find(InFilterKey))
	{
		return FilterExpressionFactory->Get()->FilterExpression(InItem, InArgs);
	}
	return false;
}

TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> FAvaMediaEditorModule::GetSimpleSuggestions(EAvaPlaylistSearchListType InSuggestionType) const
{
	TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> OutArray;

	for (const TPair<FName, TSharedPtr<IAvaPlaylistFilterSuggestionFactory>>& Suggestion : FilterSuggestionFactories)
	{
		if (Suggestion.Value->SupportSuggestionType(InSuggestionType) && Suggestion.Value->IsSimpleSuggestion())
		{
			OutArray.Add(Suggestion.Value);
		}
	}

	return OutArray;
}

TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> FAvaMediaEditorModule::GetComplexSuggestions(EAvaPlaylistSearchListType InSuggestionType) const
{
	TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> OutArray;

	for (const TPair<FName, TSharedPtr<IAvaPlaylistFilterSuggestionFactory>>& Suggestion : FilterSuggestionFactories)
	{
		if (Suggestion.Value->SupportSuggestionType(InSuggestionType) && !Suggestion.Value->IsSimpleSuggestion())
		{
			OutArray.Add(Suggestion.Value);
		}
	}

	return OutArray;
}

void FAvaMediaEditorModule::AddEditorToolbarButtons()
{
	using namespace UE::AvaMediaEditorModule::Private;

	FToolMenuEntry OpenBroadcastButtonEntry = FToolMenuEntry::InitToolBarButton(TEXT("OpenBroadcastToolbarButton")
		, FExecuteAction::CreateStatic(&FAvaBroadcastEditor::OpenBroadcastEditor)
		, LOCTEXT("OpenBroadcast_Title", "Broadcast")
		, LOCTEXT("OpenBroadcast_Tooltip", "Opens the Motion Design Broadcast Editor Window")
		, TAttribute<FSlateIcon>::Create([]() { return IAvaMediaEditorModule::Get().GetToolbarBroadcastButtonIcon(); })
	);
	OpenBroadcastButtonEntry.StyleNameOverride = TEXT("CalloutToolbar"); // Display Labels
	
	if (UToolMenu* const Menu = UToolMenus::Get()->ExtendMenu(BroadcastEditorEntry::MenuName))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection(BroadcastEditorEntry::SectionName);
		Section.AddEntry(OpenBroadcastButtonEntry);
	}
}

void FAvaMediaEditorModule::RemoveEditorToolbarButtons()
{
	using namespace UE::AvaMediaEditorModule::Private;

	if (GIsEditor && UObjectInitialized())
	{
		UToolMenus::Get()->RemoveSection(BroadcastEditorEntry::MenuName, BroadcastEditorEntry::SectionName);
	}
}

void FAvaMediaEditorModule::OpenBroadcastEditor(const TArray<FString>& InArguments)
{
	FAvaBroadcastEditor::OpenBroadcastEditor();
}

void FAvaMediaEditorModule::InitExtensibilityManagers()
{
	BroadcastToolBarExtensibility = MakeShared<FExtensibilityManager>();
	PlaybackToolBarExtensibility  = MakeShared<FExtensibilityManager>();
	PlaylistToolBarExtensibility  = MakeShared<FExtensibilityManager>();
	PlaylistMenuExtensibility     = MakeShared<FExtensibilityManager>();
}

void FAvaMediaEditorModule::ResetExtensibilityManagers()
{
	BroadcastToolBarExtensibility = nullptr;
	PlaybackToolBarExtensibility  = nullptr;
	PlaylistToolBarExtensibility  = nullptr;
	PlaylistMenuExtensibility     = nullptr;
}

void FAvaMediaEditorModule::RegisterCustomizations() const
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FMediaIOOutputConfiguration::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaMediaIOOutputConfigurationCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaRundownMacroCommand::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaRundownMacroCommandCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaRundownMacroKeyBinding::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaRundownMacroKeyBindingCustomization::MakeInstance));
}

void FAvaMediaEditorModule::UnregisterCustomizations() const
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout(FMediaIOOutputConfiguration::StaticStruct()->GetFName());
	PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaRundownMacroCommand::StaticStruct()->GetFName());
	PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaRundownMacroKeyBinding::StaticStruct()->GetFName());
}

void FAvaMediaEditorModule::StartPlaylistServerCommand(const TArray<FString>& Args)
{
	if (PlaylistServer)
	{
		UE_LOG(LogAvaMediaEditor, Log, TEXT("Playlist Server is already started."));
		return;
	}
	
	PlaylistServer = MakeShared<FAvaPlaylistServer>();
	
	// Remark: Only the module's playlist server register console commands to avoid
	// conflicts with temporary servers (for testing).
	PlaylistServer->RegisterConsoleCommands();

	PlaylistServer->Init(Args.Num() > 0 ? Args[0] : TEXT(""));
	OnPlaylistServerStarted.Broadcast();

	UE_LOG(LogAvaMediaEditor, Log, TEXT("Playlist Server Started."));
}

void FAvaMediaEditorModule::StopPlaylistServerCommand(const TArray<FString>& Args)
{
	if (PlaylistServer)
	{
		UE_LOG(LogAvaMediaEditor, Log, TEXT("Stopping Playlist Server..."));
		OnPlaylistServerStopped.Broadcast();
	}
	PlaylistServer.Reset();
}

void FAvaMediaEditorModule::PostEngineInit()
{
	using namespace UE::AvaMediaEditorModule::Private;

	// This needs to happen late in the loading process, otherwise it fails.
	const UAvaRundownEditorSettings* Settings = UAvaRundownEditorSettings::Get();

	// Allow for specification of the server name in the command line.
	// Command line has priority over project settings.
	FString ServerName;
	if (IsPlaylistServerManuallyStarted(ServerName))
	{
		StartPlaylistServerCommand({ServerName});
	}
	else if (Settings && Settings->bAutoStartRundownServer)
	{
		StartPlaylistServerCommand({Settings->RundownServerName});
	}
}

void FAvaMediaEditorModule::EnginePreExit()
{
	StopAllServices();
}

void FAvaMediaEditorModule::StopAllServices()
{
	StopPlaylistServerCommand({});
}

void FAvaMediaEditorModule::HandleMapChanged(UWorld* InWorld, EMapChangeType InMapChangeType)
{
	EAvaMediaMapChangeType EventType;
	switch (InMapChangeType)
	{
		case EMapChangeType::LoadMap:
			EventType = EAvaMediaMapChangeType::LoadMap;
			break;

		case EMapChangeType::SaveMap:
			EventType = EAvaMediaMapChangeType::SaveMap;
			break;

		case EMapChangeType::NewMap:
			EventType = EAvaMediaMapChangeType::NewMap;
			break; 

		case EMapChangeType::TearDownWorld:
			EventType = EAvaMediaMapChangeType::TearDownWorld;
			break; 

		default:
			EventType = EAvaMediaMapChangeType::None;
			break; 
	}

	// Propagate the editor event to the runtime module.
	IAvaMediaModule::Get().NotifyMapChangedEvent(InWorld, EventType);
}

template <
	typename InPlaylistFilterExpressionFactoryType,
	typename ... InArgsType
	UE_REQUIRES(TIsDerivedFrom<InPlaylistFilterExpressionFactoryType, IAvaPlaylistFilterExpressionFactory>::Value)
>
void FAvaMediaEditorModule::RegisterPlaylistFilterExpressionFactory(InArgsType&&... InArgs)
{
	const TSharedRef<IAvaPlaylistFilterExpressionFactory> FilterExpressionFactory =
		IAvaPlaylistFilterExpressionFactory::MakeInstance<InPlaylistFilterExpressionFactoryType>(Forward<InArgsType>(InArgs)...);

	if (!FilterExpressionFactories.Contains(FilterExpressionFactory->GetFilterIdentifier()))
	{
		FilterExpressionFactories.Add(FilterExpressionFactory->GetFilterIdentifier(), FilterExpressionFactory);
	}
}

template <
	typename InPlaylistSuggestionFactoryType,
	typename ... InArgsType
	UE_REQUIRES(TIsDerivedFrom<InPlaylistSuggestionFactoryType, IAvaPlaylistFilterSuggestionFactory>::Value)
>
void FAvaMediaEditorModule::RegisterPlaylistFilterSuggestionFactory(InArgsType&&... InArgs)
{
	const TSharedRef<IAvaPlaylistFilterSuggestionFactory> FilterSuggestionFactory =
		IAvaPlaylistFilterSuggestionFactory::MakeInstance<InPlaylistSuggestionFactoryType>(Forward<InArgsType>(InArgs)...);

	if (!FilterSuggestionFactories.Contains(FilterSuggestionFactory->GetSuggestionIdentifier()))
	{
		FilterSuggestionFactories.Add(FilterSuggestionFactory->GetSuggestionIdentifier(), FilterSuggestionFactory);
	}
}

void FAvaMediaEditorModule::RegisterPlaylistFilterExpressionFactories()
{
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterNameExpressionFactory>();
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterIdExpressionFactory>();
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterPathExpressionFactory>();
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterChannelExpressionFactory>();
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterStatusExpressionFactory>();
	RegisterPlaylistFilterExpressionFactory<FAvaPlaylistFilterTransitionLayerExpressionFactory>();
}

void FAvaMediaEditorModule::RegisterPlaylistFilterSuggestionFactories()
{
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterNameSuggestionFactory>();
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterIdSuggestionFactory>();
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterPathSuggestionFactory>();
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterChannelSuggestionFactory>();
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterStatusSuggestionFactory>();
	RegisterPlaylistFilterSuggestionFactory<FAvaPlaylistFilterTransitionLayerSuggestionFactory>();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvaMediaEditorModule, AvalancheMediaEditor)
