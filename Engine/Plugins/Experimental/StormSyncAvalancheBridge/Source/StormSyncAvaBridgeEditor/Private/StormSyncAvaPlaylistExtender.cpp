// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncAvaPlaylistExtender.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAvaMediaEditorModule.h"
#include "IAvaMediaModule.h"
#include "IStormSyncTransportClientModule.h"
#include "Playback/AvaMediaPlaybackServer.h"
#include "Playback/IAvaMediaPlaybackClient.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvalanchePlaylist.h"
#include "StormSyncAvaBridgeCommon.h"
#include "StormSyncAvaBridgeEditorLog.h"
#include "StormSyncAvaBridgeUtils.h"
#include "StormSyncEditor.h"
#include "Subsystems/StormSyncNotificationSubsystem.h"
#include "Toolkits/ToolkitManager.h"

#define LOCTEXT_NAMESPACE "FStormSyncAvaPlaylistExtender"

FStormSyncAvaPlaylistExtender::FStormSyncAvaPlaylistExtender()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FStormSyncAvaPlaylistExtender::OnPostEngineInit);
}

FStormSyncAvaPlaylistExtender::~FStormSyncAvaPlaylistExtender()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	if (IAvaMediaEditorModule::IsLoaded())
	{
		UnregisterExtension(IAvaMediaEditorModule::Get().GetPlaylistMenuExtensibilityManager(), MenuExtenderHandle);
		UnregisterExtension(IAvaMediaEditorModule::Get().GetPlaylistToolBarExtensibilityManager(), ToolbarExtenderHandle);
	}
}

void FStormSyncAvaPlaylistExtender::OnPostEngineInit()
{
	if (IAvaMediaEditorModule::IsLoaded())
	{
		RegisterMenuExtensions();
	}
}

void FStormSyncAvaPlaylistExtender::RegisterMenuExtensions()
{
	// Context menu extension
	MenuExtenderHandle = RegisterExtension(
		IAvaMediaEditorModule::Get().GetPlaylistMenuExtensibilityManager(),
		FAssetEditorExtender::CreateSP(this, &FStormSyncAvaPlaylistExtender::AddMenuExtender)
	);
	
	// Toolbar extension
	ToolbarExtenderHandle = RegisterExtension(
		IAvaMediaEditorModule::Get().GetPlaylistToolBarExtensibilityManager(),
		FAssetEditorExtender::CreateSP(this, &FStormSyncAvaPlaylistExtender::AddToolbarExtender)
	);
}

FDelegateHandle FStormSyncAvaPlaylistExtender::RegisterExtension(const TSharedPtr<FExtensibilityManager> InExtensibilityManager, const FAssetEditorExtender& InExtenderDelegate)
{
	if (!InExtensibilityManager.IsValid())
	{
		const FDelegateHandle DelegateHandle;
		return DelegateHandle;
	}

	const int32 ExtenderIndex = InExtensibilityManager->GetExtenderDelegates().Add(InExtenderDelegate);
	return InExtensibilityManager->GetExtenderDelegates()[ExtenderIndex].GetHandle();
}

void FStormSyncAvaPlaylistExtender::UnregisterExtension(const TSharedPtr<FExtensibilityManager> InExtensibilityManager, const FDelegateHandle& InHandleToRemove)
{
	if (!InExtensibilityManager.IsValid())
	{
		return;
	}

	InExtensibilityManager->GetExtenderDelegates().RemoveAll([Handle = InHandleToRemove](const FAssetEditorExtender& Extender)
	{
		return Handle == Extender.GetHandle();
	});
}

TSharedRef<FExtender> FStormSyncAvaPlaylistExtender::AddMenuExtender(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> ContextSensitiveObjects)
{
	STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::AddMenuExtender - Adding in menu extensions"))
	TSharedRef<FExtender> Extender(new FExtender());

	const UAvalanchePlaylist* Playlist = ContextSensitiveObjects.IsValidIndex(0) ? Cast<UAvalanchePlaylist>(ContextSensitiveObjects[0]) : nullptr;
	if (!Playlist)
	{
		return Extender;
	}

	const TSharedPtr<IToolkit> AssetEditor = FToolkitManager::Get().FindEditorForAsset(Playlist);
	if (!AssetEditor.IsValid())
	{
		return Extender;
	}

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = StaticCastSharedPtr<FAvaPlaylistEditor>(AssetEditor);
	if (!PlaylistEditor.IsValid())
	{
		return Extender;
	}

	// Template panel extension
	Extender->AddMenuExtension(
		AvalancheExtensionHook,
		EExtensionHook::After,
		InCommandList,
		// Convert to weak ptr to prevent ownership to the playlist editor and potentially increasing its lifetime
		FMenuExtensionDelegate::CreateSP(this, &FStormSyncAvaPlaylistExtender::CreateTemplateContextMenu, Playlist, TWeakPtr<FAvaPlaylistEditor>(PlaylistEditor))
	);

	return Extender;
}

void FStormSyncAvaPlaylistExtender::CreateTemplateContextMenu(FMenuBuilder& MenuBuilder, const UAvalanchePlaylist* InPlaylist, TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	check(InPlaylist);

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = InPlaylistEditor.Pin();
	if (!PlaylistEditor.IsValid())
	{
		STORM_SYNC_AVA_EDITOR_LOG(Error, TEXT("FStormSyncAvaPlaylistExtender::CreateTemplateContextMenu - Invalid shared ptr from weak ptr delegate param"))
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("StormSyncEditor")))
	{
		STORM_SYNC_AVA_EDITOR_LOG(Error, TEXT("FStormSyncAvaPlaylistExtender::CreateTemplateContextMenu - StormSyncEditor module is not loaded. Is StormSync plugin enabled ?"))
		return;
	}

	FText DisabledTooltipReason;
	bool bIsValidSelection = false;
	TArray<FName> SelectedPackageNames;
	FCanExecuteAction DefaultCanExecuteAction;
	GetContextMenuSelectionInfos(InPlaylist, InPlaylistEditor, bIsValidSelection, DisabledTooltipReason, SelectedPackageNames, DefaultCanExecuteAction);

	// For later use with action handler
	const FStormSyncEditorModule& StormSyncEditor = FStormSyncEditorModule::Get();

	MenuBuilder.BeginSection("StormSyncOperations_Template", LOCTEXT("StormSyncOperationsHeader", "Synchronize Actions"));
	
	// Initialize action
	{
		const FText TooltipText = bIsValidSelection ?
			FText::Format(LOCTEXT("Initialize_Tooltip", "Sync asset over remote node.{0}"), DisabledTooltipReason) :
			FText::Format(LOCTEXT("Initialize_Tooltip_Invalid", "Please ensure the playlist page is using a valid Avalanche Blueprint.{0}"), DisabledTooltipReason);

		const TArray<FAvalanchePage> SelectedTemplatePages = GetSelectedPages(InPlaylist, InPlaylistEditor);
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Initialize_Label", "Initialize"),
			TooltipText,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FStormSyncAvaPlaylistExtender::HandleInitializeAction, InPlaylist, SelectedTemplatePages),
				DefaultCanExecuteAction
			)
		);
	}

	constexpr bool bOpenSubMenuOnClick = false;

	// Push action
	{
		FText LabelText;
		FText TooltipText;

		if (!bIsValidSelection)
		{
			LabelText = LOCTEXT("PushAssetsMenuEntryInvalid", "Cannot push. Page has no valid asset.");
			TooltipText = LOCTEXT("PushAssetsMenuEntryTooltipInvalid", "Please ensure the playlist pages are using a valid Avalanche Blueprint.");
		}
		else
		{
			LabelText = FText::Format(
				LOCTEXT("PushAssetsAssetName", "Push {0} selected asset{1} to"),
				FText::AsNumber(SelectedPackageNames.Num()),
				FText::FromString(SelectedPackageNames.Num() == 1 ? TEXT("") : TEXT("s"))
			);

			TooltipText = FText::Format(
				LOCTEXT("PushAssetsMenuEntryTooltip", "Push {0} selected asset{1} (and inner dependencies) to specific remote.\n\nTransfer will only proceed if changes are detected.{2}"),
				FText::AsNumber(SelectedPackageNames.Num()),
				FText::FromString(SelectedPackageNames.Num() == 1 ? TEXT("") : TEXT("s")),
				DisabledTooltipReason
			);
		}

		constexpr bool bIsPushing = true;
		MenuBuilder.AddSubMenu(
			LabelText,
			TooltipText,
			FNewMenuDelegate::CreateRaw(
				&StormSyncEditor,
				&FStormSyncEditorModule::BuildPushAssetsMenuSection,
				SelectedPackageNames,
				bIsPushing
			),
			FUIAction(FExecuteAction(), DefaultCanExecuteAction),
			NAME_None,
			EUserInterfaceActionType::Button,
			bOpenSubMenuOnClick,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll")
		);
	}

	// Compare action
	{
		FText LabelText;
		FText TooltipText;

		if (!bIsValidSelection)
		{
			LabelText = LOCTEXT("CompareAssetsMenuEntryInvalid", "Cannot compare. Page has no valid asset.");
			TooltipText = LOCTEXT("CompareAssetsMenuEntryTooltipInvalid", "Please ensure the playlist pages are using a valid Avalanche Blueprint.");
		}
		else
		{
			LabelText = LOCTEXT("CompareAssetsMenuEntry", "Compare Asset(s) with");
			TooltipText = FText::Format(
				LOCTEXT("CompareAssetsRemoteMenuEntry", "Compare asset(s) with a specific remote and see if files (and inner dependencies) are either missing or in mismatched state.{0}"),
				DisabledTooltipReason
			);
		}
		
		MenuBuilder.AddSubMenu(
			LabelText,
			TooltipText,
			FNewMenuDelegate::CreateRaw(
				&StormSyncEditor,
				&FStormSyncEditorModule::BuildCompareWithMenuSection,
				SelectedPackageNames
			),
			FUIAction(FExecuteAction(), DefaultCanExecuteAction),
			NAME_None,
			EUserInterfaceActionType::Button,
			bOpenSubMenuOnClick,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.Visualizers")
		);
	}
	MenuBuilder.EndSection();
}

void FStormSyncAvaPlaylistExtender::HandleInitializeAction(const UAvalanchePlaylist* InPlaylist, TArray<FAvalanchePage> InSelectedTemplatePages)
{
	// Build a list of package names to push grouped by channel name (and remote)
	// Key is the remote address id, value is the list of package names to synchronize
	TMap<FString, TArray<FName>> PackageNamesPerChannel;
	
	for (const FAvalanchePage& SelectedPage : InSelectedTemplatePages)
	{
		const FString AssetName = SelectedPage.GetAvalancheAssetPath(InPlaylist).GetLongPackageName();
		TArray<FString> ChannelNames = GetChannelNamesForTemplatePage(InPlaylist, SelectedPage);

		// From the list of channel names that match this asset to sync, build the list of server names
		// Note: there may be more than one server per channel (channel support multiple outputs).
		
		STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("ChannelNames for asset \"%s\" are: %s"), *AssetName, *FString::Join(ChannelNames, TEXT(", ")));

		TArray<FString> RemoteServerNames;
		for (FString ChannelName : ChannelNames)
		{
			const TArray<FString> ServerNames = FStormSyncAvaBridgeUtils::GetServerNamesForChannel(ChannelName);
			if (ServerNames.IsEmpty())
			{
				STORM_SYNC_AVA_EDITOR_LOG(Warning, TEXT("FStormSyncAvaPlaylistExtender::HandleInitializeActions - Unable to determine playback servers for channel \"%s\""), *ChannelName)
				continue;
			}

			RemoteServerNames.Append(ServerNames);
		}

		if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted())
		{
			const IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
			for (const FString& ServerName : RemoteServerNames)
			{
				// Get address id for storm sync client on playback host
				FString ClientAddress = PlaybackClient.GetServerUserData(ServerName, UE::StormSync::AvaBridgeCommon::StormSyncClientAddressKey);
				STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("ClientAddress on playback server %s is %s"), *ServerName, *ClientAddress);

				if (ClientAddress.IsEmpty())
				{
					STORM_SYNC_AVA_EDITOR_LOG(Warning, TEXT("Storm Sync Server Adress id for playback server %s is empty"), *ServerName);
					continue;
				}
				
				TArray<FName>& PackageNames = PackageNamesPerChannel.FindOrAdd(ClientAddress);
				PackageNames.Add(FName(*AssetName));
			}
		}
	}

	for (const TPair<FString, TArray<FName>>& NamesPerChannel : PackageNamesPerChannel)
	{
		FString AddressId = NamesPerChannel.Key;
		TArray<FName> PackageNames = NamesPerChannel.Value;
		
		STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("Names per channel - Channel: %s"), *AddressId)
		for (const FName& PackageName : PackageNames)
		{
			STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("\t PackageName: %s"), *PackageName.ToString())
		}

		PushPackagesToRemote(AddressId, PackageNames);
	}
}

TSharedRef<FExtender> FStormSyncAvaPlaylistExtender::AddToolbarExtender(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> ContextSensitiveObjects)
{
	STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::AddToolbarExtender - Adding in toolbar extensions"))
	TSharedRef<FExtender> Extender(new FExtender());

	const UAvalanchePlaylist* Playlist = ContextSensitiveObjects.IsValidIndex(0) ? Cast<UAvalanchePlaylist>(ContextSensitiveObjects[0]) : nullptr;
	if (!Playlist)
	{
		return Extender;
	}

	const TSharedPtr<IToolkit> AssetEditor = FToolkitManager::Get().FindEditorForAsset(Playlist);
	if (!AssetEditor.IsValid())
	{
		return Extender;
	}

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = StaticCastSharedPtr<FAvaPlaylistEditor>(AssetEditor);
	if (!PlaylistEditor.IsValid())
	{
		return Extender;
	}

	Extender->AddToolBarExtension(
		"Pages",
		EExtensionHook::After,
		InCommandList,
		FToolBarExtensionDelegate::CreateSP(this, &FStormSyncAvaPlaylistExtender::FillToolbar, TWeakPtr<FAvaPlaylistEditor>(PlaylistEditor))
	);
	
	STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::AddToolbarExtender - Playlist: %s"), *GetNameSafe(Playlist))
	
	return Extender;
}

void FStormSyncAvaPlaylistExtender::FillToolbar(FToolBarBuilder& ToolbarBuilder, TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	ToolbarBuilder.BeginSection(TEXT("StormSync"));
	{
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &FStormSyncAvaPlaylistExtender::GenerateToolbarMenu, InPlaylistEditor),
			LOCTEXT("ToolbarLabel", "Synchronize Actions"),
			LOCTEXT("ToolbarToolTip", "Synchronize the playlist assets over the network"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
			false
		);
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SWidget> FStormSyncAvaPlaylistExtender::GenerateToolbarMenu(TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	TArray<FName> PackageNames;

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = InPlaylistEditor.Pin();

	if (PlaylistEditor.IsValid() && PlaylistEditor->IsPlaylistValid())
	{
		const UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist(); 

		// Gather the list of all package names from Avalanche Blueprints in this Playlist pages
		const FAvalanchePageCollection& AvalanchePageCollection = Playlist->GetTemplatePages();
		for (const FAvalanchePage& AvalanchePage : AvalanchePageCollection.Pages)
		{
			if (FString PackageName = AvalanchePage.GetAvalancheAssetPath(Playlist).GetLongPackageName(); !PackageName.IsEmpty())
			{
				PackageNames.AddUnique(FName(*PackageName));
			}
		}
	}
	
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(TEXT("StormSyncActions"));
	
	if (FModuleManager::Get().IsModuleLoaded(TEXT("StormSyncEditor")))
	{
		FStormSyncEditorModule& StormSyncEditor = FStormSyncEditorModule::Get();
		
		const int32 PackagesCount = PackageNames.Num();

		TArray<FString> AssetList;
		for (const FName PackageName : PackageNames)
		{
			AssetList.Add(FString::Printf(TEXT("- %s"), *PackageName.ToString()));
		}

		FText LabelText = FText::Format(LOCTEXT("PushAssetsToolbarMenuEntry", "Push {0} asset(s) to"), FText::AsNumber(PackagesCount));
		FText TooltipText = FText::Format(LOCTEXT(
			"PushAssetsToolbarMenuEntryTooltip",
			"Push {0} asset(s) (and inner dependencies) to specific remote.\n\nTransfer will only proceed if changes are detected.\n\n{1}"
		), FText::AsNumber(PackagesCount), FText::FromString(FString::Join(AssetList, LINE_TERMINATOR)));
		
		const bool bIsPushEnabled = !PackageNames.IsEmpty();
		if (!bIsPushEnabled)
		{
			LabelText = LOCTEXT("PushAssetsToolbarMenuEntryInvalid", "Cannot push. Playlist pages have no valid assets.");
			TooltipText = LOCTEXT("PushAssetsToolbarMenuEntryTooltipInvalid", "Please ensure the playlist pages are using a valid Avalanche Blueprint");
		}

		constexpr bool bIsPushing = true;
		constexpr bool bOpenSubMenuOnClick = false;
		MenuBuilder.AddSubMenu(
			LabelText,
			TooltipText,
			FNewMenuDelegate::CreateRaw(
				&StormSyncEditor,
				&FStormSyncEditorModule::BuildPushAssetsMenuSection,
				PackageNames,
				bIsPushing
			),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([bIsPushEnabled]() { return bIsPushEnabled; })
			),
			NAME_None,
			EUserInterfaceActionType::Button,
			bOpenSubMenuOnClick,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll")
		);

		MenuBuilder.AddSubMenu(
			FText::Format(LOCTEXT("CompareAssetsToolbarMenuEntry", "Compare {0} asset(s) with"), FText::AsNumber(PackagesCount)),
			LOCTEXT("CompareAssetsToolbarMenuEntryTooltip", "Compare asset(s) with a specific remote and see if files (and inner dependencies) are either missing or in mismatched state."),
			FNewMenuDelegate::CreateRaw(
				&StormSyncEditor,
				&FStormSyncEditorModule::BuildCompareWithMenuSection,
				PackageNames
			),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([bIsPushEnabled]() { return bIsPushEnabled; })
			),
			NAME_None,
			EUserInterfaceActionType::Button,
			bOpenSubMenuOnClick,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.Visualizers")
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FStormSyncAvaPlaylistExtender::PushPackagesToRemote(const FString& RemoteAddressId, const TArray<FName>& InPackageNames)
{
	STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::PushPackagesToRemote - InSelectedPage: (InPackageNames: %d, RemoteAddressId: %s)"), InPackageNames.Num(), *RemoteAddressId)

	FMessageAddress RemoteMessageAddress;
	if (!FMessageAddress::Parse(RemoteAddressId, RemoteMessageAddress))
	{
		STORM_SYNC_AVA_EDITOR_LOG(Error, TEXT("Unable to parse %s into a Message Address"), *RemoteAddressId);
		return;
	}

	// Note: We sync with a dummy package descriptor, next iterations could add in there an additional UI step.
	// Something that could be handled with a bit more UI integration, like some kind of popup window or wizard.
	const FStormSyncPackageDescriptor PackageDescriptor;

	const FOnStormSyncPushComplete Delegate = FOnStormSyncPushComplete::CreateLambda([](const TSharedPtr<FStormSyncTransportPushResponse>& Response)
	{
		check(Response.IsValid())
		STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::PushPackagesToRemote - Got a response: %s"), *Response->ToString());
		UStormSyncNotificationSubsystem::Get().HandlePushResponse(Response);
	});
	
	IStormSyncTransportClientModule::Get().PushPackages(PackageDescriptor, InPackageNames, RemoteMessageAddress, Delegate);
}

TArray<FName> FStormSyncAvaPlaylistExtender::GetSelectedPackagesNames(const UAvalanchePlaylist* InPlaylist, const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	check(InPlaylist);
	
	TArray<FName> Result;

	const TArray<FAvalanchePage> SelectedPages = GetSelectedPages(InPlaylist, InPlaylistEditor);
	Algo::Transform(SelectedPages, Result, [InPlaylist](const FAvalanchePage& Page)
	{
		return Page.GetAvalancheAssetPath(InPlaylist).GetLongPackageFName();
	});

	return Result;
}

TArray<FAvalanchePage> FStormSyncAvaPlaylistExtender::GetSelectedPages(const UAvalanchePlaylist* InPlaylist, const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	check(InPlaylist);
	
	TArray<FAvalanchePage> Result;
	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = InPlaylistEditor.Pin();
	if (!PlaylistEditor.IsValid())
	{
		STORM_SYNC_AVA_EDITOR_LOG(Display, TEXT("FStormSyncAvaPlaylistExtender::GetSelectedPages - Invalid shared ptr from weak ptr delegate param"))
		return Result;
	}

	const TConstArrayView<int32> SelectedPageIds = PlaylistEditor->GetSelectedPagesOnFocusedWidget();
	for (const int32 SelectedPageId : SelectedPageIds)
	{
		FAvalanchePage AvalanchePage = InPlaylist->GetPage(SelectedPageId);
		if (!AvalanchePage.IsValidPage())
		{
			continue;
		}

		FSoftObjectPath SoftAvalancheAssetPath = AvalanchePage.GetAvalancheAssetPath(InPlaylist);
		FString LongPackageName = SoftAvalancheAssetPath.GetLongPackageName();
		FString AssetName = SoftAvalancheAssetPath.GetAssetName();

		if (!LongPackageName.IsEmpty())
		{
			Result.Add(AvalanchePage);
		}
	}
	
	return Result;
}

TArray<FString> FStormSyncAvaPlaylistExtender::GetChannelNamesForTemplatePage(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InTemplatePage)
{
	check(InPlaylist);

	TArray<FString> ChannelNames;

	// Here, we try to determine the list of channels to consider for a sync operation, from the selected template page,
	// with instanced pages that are using the selected package name (Avalanche Blueprint)
	const FString PackageName = InTemplatePage.GetAvalancheAssetPath(InPlaylist).GetLongPackageName();

	// Build the list of instanced pages matching the asset name we want to sync
	TArray<FAvalanchePage> Pages = InPlaylist->GetInstancedPages().Pages.FilterByPredicate([InPlaylist, PackageName](const FAvalanchePage& Page)
	{
		return Page.GetAvalancheAssetPath(InPlaylist).GetLongPackageName() == PackageName;
	});

	// From there, build a unique list of channel outputs
	for (const FAvalanchePage& AvalanchePage : Pages)
	{
		ChannelNames.AddUnique(AvalanchePage.GetChannelName().ToString());
	}
	
	return ChannelNames;
}

bool FStormSyncAvaPlaylistExtender::GetContextMenuSelectionInfos(const UAvalanchePlaylist* InPlaylist, const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor, bool& bOutIsValidSelection, FText& OutDisabledReasonTooltip, TArray<FName>& OutSelectedPackageNames, FCanExecuteAction& OutCanExecuteAction)
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("StormSyncEditor")))
	{
		STORM_SYNC_AVA_EDITOR_LOG(Error, TEXT("FStormSyncAvaPlaylistExtender::GetContextMenuSelectionInfos - StormSyncEditor module is not loaded. Is StormSync plugin enabled ?"))
		return false;
	}
	
	const FStormSyncEditorModule& StormSyncEditor = FStormSyncEditorModule::Get();

	TArray<FName> SelectedPackagesNames = GetSelectedPackagesNames(InPlaylist, InPlaylistEditor);
	bool bIsValidSelection = !SelectedPackagesNames.IsEmpty();

	// Figure out if selection is containing dirty (unsaved) assets
	FText DisabledTooltipReason;
	const TArray<FAssetData> DirtyAssets = StormSyncEditor.GetDirtyAssets(SelectedPackagesNames, DisabledTooltipReason);
	
	bool bContainsDirtyAssets = !DirtyAssets.IsEmpty();
	FCanExecuteAction DefaultCanExecuteAction = FCanExecuteAction::CreateLambda([bIsValidSelection, bContainsDirtyAssets]() { return bIsValidSelection && !bContainsDirtyAssets; });

	bOutIsValidSelection = bIsValidSelection;
	OutDisabledReasonTooltip = DisabledTooltipReason;
	OutSelectedPackageNames = MoveTemp(SelectedPackagesNames);
	OutCanExecuteAction = MoveTemp(DefaultCanExecuteAction);
	
	return true;
}

#undef LOCTEXT_NAMESPACE
