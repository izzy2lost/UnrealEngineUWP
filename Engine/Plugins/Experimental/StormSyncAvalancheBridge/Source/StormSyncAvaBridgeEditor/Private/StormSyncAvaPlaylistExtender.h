// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MessageEndpoint.h"
#include "Toolkits/AssetEditorToolkit.h"

class FAvaPlaylistEditor;
class FExtensibilityManager;
class UAvalanchePlaylist;
struct FAvalanchePage;

/** This class handles Avalanche Playlist assets UI extensions for context menu / toolbars */
class FStormSyncAvaPlaylistExtender : public TSharedFromThis<FStormSyncAvaPlaylistExtender>
{
public:
	FStormSyncAvaPlaylistExtender();
	~FStormSyncAvaPlaylistExtender();

private:
	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Name of the extension point we're providing extension next to*/
	static constexpr const TCHAR* AvalancheExtensionHook = TEXT("PageListOperations");
	
	/** Menu extender for avalanche playlist editor context menu */
	FDelegateHandle MenuExtenderHandle;
	
	/** Toolbar extender for avalanche playlist editor */
	FDelegateHandle ToolbarExtenderHandle;

	/** Register Context menu / toolbar extensions when editor is fully loaded */
	void OnPostEngineInit();
	
	/** Startup module handler for UI extensions */
	void RegisterMenuExtensions();

	/** Startup module handler to register an UI extensions from one of Avalanche exposed extensibility hooks */
	static FDelegateHandle RegisterExtension(const TSharedPtr<FExtensibilityManager> InExtensibilityManager, const FAssetEditorExtender& InExtenderDelegate);
	
	/** Shutdown module handler to unregister any UI extensions added here */
	static void UnregisterExtension(const TSharedPtr<FExtensibilityManager> InExtensibilityManager, const FDelegateHandle& InHandleToRemove);
	
	/** Gets the extender to use for playlist context sensitive menus */
	TSharedRef<FExtender> AddMenuExtender(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> ContextSensitiveObjects);

	/* UI Menu Extension handler for template panel */
	void CreateTemplateContextMenu(FMenuBuilder& MenuBuilder, const UAvalanchePlaylist* InPlaylist, TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor);

	/** Context menu handler for initialize action */
	void HandleInitializeAction(const UAvalanchePlaylist* InPlaylist, TArray<FAvalanchePage> InSelectedTemplatePages);
	
	/** Gets the extender to use for playlist context sensitive menus */
	TSharedRef<FExtender> AddToolbarExtender(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> ContextSensitiveObjects);
	
	/** Construct toolbar widgets for playlist sync actions */
	void FillToolbar(FToolBarBuilder& ToolbarBuilder, TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor);

	/** Creates widget for toolbar content */
	TSharedRef<SWidget> GenerateToolbarMenu(TWeakPtr<FAvaPlaylistEditor> InPlaylistEditor);

	/** Storm sync push a list of package names to a remote address ID */
	static void PushPackagesToRemote(const FString& RemoteAddressId, const TArray<FName>& InPackageNames);

	/** Gather a list of package names from currently selected pages in editor */
	static TArray<FName> GetSelectedPackagesNames(const UAvalanchePlaylist* InPlaylist, const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	/**
	 * Returns the list of currently selected pages in editor
	 *
	 * It will only return selection if the page is referencing a valid Avalanche Blueprint (not "None" path)
	 */
	static TArray<FAvalanchePage> GetSelectedPages(const UAvalanchePlaylist* InPlaylist, const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor);

	/** Returns a unique list of channel names, gather from instanced pages, that are matching the passed in template page selection and selected asset name */
	static TArray<FString> GetChannelNamesForTemplatePage(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InTemplatePage);

	/**
	 * Provides a way for context menu builders to gather information about current selection.
	 *
	 * @param InPlaylist The playlist UObject we're working with
	 * @param InPlaylistEditor The playlist asset editor we're working with
	 * @param bOutIsValidSelection Output indicating whether selection is valid - eg. current selection returns a list of valid package names
	 * @param OutDisabledReasonTooltip Output holding the tooltip to use in case the action is disabled - eg. in case the selection contains unsaved dirty assets
	 * @param OutSelectedPackageNames Output list of package names of the selection, as returned by GetSelectedPackagesNames()
	 * @param OutCanExecuteAction Output delegate to use as a default CanExecuteAction - eg. disabling the action if selection is invalid
	 *
	 * @return false if StormSyncEditor module is not available, and we failed to determine selection state
	 */
	static bool GetContextMenuSelectionInfos(
		const UAvalanchePlaylist* InPlaylist,
		const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditor,
		bool& bOutIsValidSelection,
		FText& OutDisabledReasonTooltip,
		TArray<FName>& OutSelectedPackageNames,
		FCanExecuteAction& OutCanExecuteAction
	);
};
