// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaMediaEditorModule.h"
#include "Playlist/AvaPlaylistServer.h"
#include "Templates/UnrealTypeTraits.h"

class IAvaPlaylistFilterExpressionFactory;
class IAvaPlaylistFilterSuggestionFactory;
struct FGraphPanelPinConnectionFactory;
enum class EAvaPlaylistSearchListType : uint8;

class FAvaMediaEditorModule : public IAvaMediaEditorModule
{

public:

	//IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~IModuleInterface

	//IAvaMediaEditorModule
	virtual TSharedPtr<FExtensibilityManager> GetBroadcastToolBarExtensibilityManager() override;
	virtual TSharedPtr<FExtensibilityManager> GetPlaybackToolBarExtensibilityManager() override;
	virtual TSharedPtr<FExtensibilityManager> GetPlaylistToolBarExtensibilityManager() override;
	virtual TSharedPtr<FExtensibilityManager> GetPlaylistMenuExtensibilityManager() override;
	virtual FOnPlaylistServerStarted& GetOnPlaylistServerStarted() override { return OnPlaylistServerStarted; }
	virtual FOnPlaylistServerStopped& GetOnPlaylistServerStopped() override { return OnPlaylistServerStopped; }
	virtual TSharedPtr<FAvaPlaylistServer> GetPlaylistServer() const override { return PlaylistServer; }
	virtual bool CanFilterSupportComparisonOperation(const FName& InFilterKey, ETextFilterComparisonOperation InOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const override;
	virtual bool FilterExpression(const FName& InFilterKey, const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const override;
	virtual TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> GetSimpleSuggestions(EAvaPlaylistSearchListType InSuggestionType) const override;
	virtual TArray<TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> GetComplexSuggestions(EAvaPlaylistSearchListType InSuggestionType) const override;
	//~IAvaMediaEditorModule

	void AddEditorToolbarButtons();
	void RemoveEditorToolbarButtons();
	virtual FSlateIcon GetToolbarBroadcastButtonIcon() const override;

	static void OpenBroadcastEditor(const TArray<FString>& InArguments);

protected:
	void InitExtensibilityManagers();
	void ResetExtensibilityManagers();

	/** Register details view customizations. */
	void RegisterCustomizations() const;

	/** Unregister details view customizations. */
	void UnregisterCustomizations() const;

	void StartPlaylistServerCommand(const TArray<FString>& Args);
	void StopPlaylistServerCommand(const TArray<FString>& Args);

private:
	void PostEngineInit();
	void EnginePreExit();
	void StopAllServices();
	void HandleMapChanged(UWorld* InWorld, EMapChangeType InMapChangeType);
	
	template <
		typename InPlaylistFilterExpressionFactoryType,
		typename... InArgsType
		UE_REQUIRES(TIsDerivedFrom<InPlaylistFilterExpressionFactoryType, IAvaPlaylistFilterExpressionFactory>::Value)
	>
	void RegisterPlaylistFilterExpressionFactory(InArgsType&&... InArgs);

	template <
		typename InPlaylistSuggestionFactoryType,
		typename... InArgsType
		UE_REQUIRES(TIsDerivedFrom<InPlaylistSuggestionFactoryType, IAvaPlaylistFilterSuggestionFactory>::Value)
	>
	void RegisterPlaylistFilterSuggestionFactory(InArgsType&&... InArgs);

	void RegisterPlaylistFilterExpressionFactories();

	void RegisterPlaylistFilterSuggestionFactories();

private:
	TSharedPtr<FExtensibilityManager> BroadcastToolBarExtensibility;
	TSharedPtr<FExtensibilityManager> PlaybackToolBarExtensibility;
	TSharedPtr<FExtensibilityManager> PlaylistToolBarExtensibility;
	TSharedPtr<FExtensibilityManager> PlaylistMenuExtensibility;

	TSharedPtr<FGraphPanelPinConnectionFactory> PlaybackConnectionFactory;

	TSharedPtr<FAvaPlaylistServer> PlaylistServer;

	FOnPlaylistServerStarted OnPlaylistServerStarted;
	FOnPlaylistServerStopped OnPlaylistServerStopped;

	TArray<IConsoleObject*> ConsoleCmds;

	/** Holds all the PlaylistFilterExpressionFactory */
	TMap<FName, TSharedPtr<IAvaPlaylistFilterExpressionFactory>> FilterExpressionFactories;

	/** Holds all the PlaylistFilterSuggestionFactory */
	TMap<FName, TSharedPtr<IAvaPlaylistFilterSuggestionFactory>> FilterSuggestionFactories;
};
