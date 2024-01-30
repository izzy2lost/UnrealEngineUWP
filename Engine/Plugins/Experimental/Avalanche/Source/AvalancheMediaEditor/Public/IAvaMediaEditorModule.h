// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvaMediaEditor, Log, All);

class FAvaPlaylistServer;
class IAvaPlaylistFilterSuggestionFactory;
struct FAvalanchePage;
struct FAvaPlaylistTextFilterArgs;
enum class EAvaPlaylistSearchListType : uint8;

class IAvaMediaEditorModule : public IModuleInterface
{
	static constexpr const TCHAR* ModuleName = TEXT("AvalancheMediaEditor");

public:
	static bool IsLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	static IAvaMediaEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IAvaMediaEditorModule>(ModuleName);
	}

	/** Returns the tool menu name used for Page Context Menu */
	static FName GetPlaylistPageMenuName()
	{
		return TEXT("AvaPlaylistPageContextMenu");
	}

	virtual FSlateIcon GetToolbarBroadcastButtonIcon() const = 0;

	/** Returns the toolbar extensibility manager for the Broadcast Editor */
	virtual TSharedPtr<FExtensibilityManager> GetBroadcastToolBarExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the Playback Editor */
	virtual TSharedPtr<FExtensibilityManager> GetPlaybackToolBarExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the Playback Editor */
	virtual TSharedPtr<FExtensibilityManager> GetPlaylistToolBarExtensibilityManager() = 0;

	/**
	 * Returns the context menu extensibility manager for the Playlist Editor's Template Pages
	 * @remark prefer extending with the UToolMenu named after IAvaMediaEditorModule::GetPlaylistPageMenuName, and using UAvaPlaylistPageContext to retrieve context information
	 */
	virtual TSharedPtr<FExtensibilityManager> GetPlaylistMenuExtensibilityManager() = 0;

	DECLARE_MULTICAST_DELEGATE(FOnPlaylistServerStarted);
	virtual FOnPlaylistServerStarted& GetOnPlaylistServerStarted() = 0;

	DECLARE_MULTICAST_DELEGATE(FOnPlaylistServerStopped);
	virtual FOnPlaylistServerStopped& GetOnPlaylistServerStopped() = 0;

	virtual TSharedPtr<FAvaPlaylistServer> GetPlaylistServer() const = 0;

	/**
 	* Check if current playlist filter expression factory support the comparison operation
 	* @param InFilterKey Filter key to get the playlist filter expression factory needed
 	* @param InOperation Operation to check if supported
 	* @param InPlaylistSearchListType Type of the Search List either Template or Instanced
 	* @return True if operation is supported, False otherwise
 	*/
	virtual bool CanFilterSupportComparisonOperation(const FName& InFilterKey, ETextFilterComparisonOperation InOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const = 0;

	/**
 	* Evaluate the expression and return the result
 	* @param InFilterKey Filter Key to get the Factory
 	* @param InItem Item that is currently checked
 	* @param InArgs Args to evaluate the expression see FAvaPlaylistTextFilterArgs for more information
 	* @return True if the expression evaluated to True, False otherwise
 	*/
	virtual bool FilterExpression(const FName& InFilterKey, const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const = 0;

	/**
 	* Get all simple suggestions with the given type (Template/Instanced/All)
 	* @param InSuggestionType Type of suggestion to get, see EAvaPlaylistSearchListType for more information
 	* @return An Array containing all suggestions of the given type
 	*/
	virtual TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> GetSimpleSuggestions(EAvaPlaylistSearchListType InSuggestionType) const = 0;

	/**
 	* Get all complex suggestions with the given type (Template/Instanced/All)
 	* @param InSuggestionType Type of suggestion to get, see EAvaPlaylistSearchListType for more information
 	* @return An Array containing all suggestions of the given type
 	*/
	virtual TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> GetComplexSuggestions(EAvaPlaylistSearchListType InSuggestionType) const = 0;
};
