// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistDefines.h"
#include "Playlist/AvaRundownMacroCollection.h"
#include "Playlist/AvalanchePlaylist.h"
#include "TickableEditorObject.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class FAvaPageTextFilter;
class FAvaPlaylistEditorInputProcessor;
class FAvalancheManagedInstance;
class SAvaInstancedPageList;
class SAvaPageList;
class SAvaReadPage;
enum class EAvaPlaylistSearchListType : uint8;

class AVALANCHEMEDIAEDITOR_API FAvaPlaylistEditor : public FWorkflowCentricApplication
{
public:
	FAvaPlaylistEditor();

	virtual ~FAvaPlaylistEditor() override;

	void InitPlaylistEditor(const EToolkitMode::Type InMode
		, const TSharedPtr<IToolkitHost>& InitToolkitHost
		, UAvalanchePlaylist* InPlaylist);

	//~ Begin IToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ End IToolkit

	//~ Begin FAssetEditorToolkit
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	//~ End FAssetEditorToolkit
	
	TSharedRef<SWidget> MakeReadPageWidget();

	void ExtendToolBar(TSharedPtr<FExtender> InExtender);
	void FillPageToolBar(FToolBarBuilder& OutToolBarBuilder);

	bool IsPlaylistValid() const { return AvalanchePlaylist.IsValid(); }
	UAvalanchePlaylist* GetPlaylist() const;
	void MarkAsModified();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPageEvent, const TArray<int32>&, UE::AvalanchePlaylist::EPageEvent);
	FOnPageEvent& GetOnPageEvent() { return OnPageEvent; }

	static TArray<TSharedPtr<FAvalancheManagedInstance>> GetManagedInstancesForPage(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage);
	static bool MergeDefaultRemoteControlValues(const TArray<TSharedPtr<FAvalancheManagedInstance>>& InManagedInstances, FAvalancheRemoteControlValues& OutMergedValues);

	bool IsKeyRelevant(const FKeyEvent& InKeyEvent) const;
	bool HandleKeyDownEvent(const FKeyEvent& InKeyEvent);

	TSharedPtr<SAvaPageList> GetTemplateListWidget() const;
	TSharedPtr<SAvaPageList> GetInstanceListWidget() const;
	TSharedPtr<SAvaPageList> GetListWidget(const FAvaPageListReference& InPageListReference) const;
	TSharedPtr<SAvaPageList> GetListWidget(const FName& InTabId) const;

	/** Returns the currently active Instanced Page List. */
	TSharedPtr<SAvaInstancedPageList> GetActiveListWidget() const;
	
	/** Returns the currently focused Page List, including templates, instances or views. */
	TSharedPtr<SAvaPageList> GetFocusedListWidget() const;

	int32 GetFirstSelectedPageOnActiveSubListWidget() const;
	TConstArrayView<int32> GetSelectedPagesOnActiveSubListWidget() const;
	TConstArrayView<int32> GetSelectedPagesOnFocusedWidget() const;

	bool CanAddTemplate() const;
	void AddTemplate();

	bool CanPlaySelectedPage() const;
	void PlaySelectedPage();

	bool CanUpdateValuesOnSelectedPage() const;
	void UpdateValuesOnSelectedPage();
	
	bool CanStopSelectedPage(bool bInForce) const;
	void StopSelectedPage(bool bInForce);

	bool CanContinueSelectedPage() const;
	void ContinueSelectedPage();

	bool CanPlayNextPage() const;
	void PlayNextPage();

	bool CanPreviewPlaySelectedPage() const;
	void PreviewPlaySelectedPage(bool bInToMark);

	bool CanPreviewContinueSelectedPage() const;
	void PreviewContinueSelectedPage();

	bool CanPreviewStopSelectedPage(bool bInForce) const;
	void PreviewStopSelectedPage(bool bInForce);

	bool CanPreviewPlayNextPage() const;
	void PreviewPlayNextPage();

	bool CanTakeToProgram() const;
	void TakeToProgram() const;

	bool CanCreateInstancesFromSelectedTemplates() const;
	void CreateInstancesFromSelectedTemplates();

	bool CanRemoveSelectedPages() const;
	void RemoveSelectedPages();

	void RefreshTemplateVisibility();
	void RefreshInstancedVisibility();

	bool IsTemplatePageVisible(const FAvalanchePage& InPage) const;
	bool IsInstancedPageVisible(const FAvalanchePage& InPage) const;

	void SetSearchText(const FText& InText, EAvaPlaylistSearchListType& InPageListType);

protected:
	void RegisterApplicationModes();

	void CreatePlaylistCommands();

	FText GetCurrentProfileName() const;

	TSharedRef<SWidget> MakeProfileComboButton();

	void StartAutoPlayCommand(const TArray<FString>& InArgs);
	void StopAutoPlayCommand(const TArray<FString>& InArgs);
	void CancelAutoPlay();
	
	void LoadPageCommand(const TArray<FString>& InArgs);
	void UnloadPageCommand(const TArray<FString>& InArgs);
	void PlayPageCommand(const TArray<FString>& InArgs, bool bInPreview);
	void ContinuePageCommand(const TArray<FString>& InArgs, bool bInPreview);
	void PlayNextPageCommand(const TArray<FString>& InArgs, bool bInPreview);
	void StopPageCommand(const TArray<FString>& InArgs, bool bInPreview, bool bInForce);
	void TakeToProgramCommand(const TArray<FString>& InArgs);
	void StartChannelCommand(const TArray<FString>& InArgs);
	void StopChannelCommand(const TArray<FString>& InArgs);
	
	TArray<int32> ArgumentsToPageIds(const TArray<FString>& InArgs, bool bInPreview, FString& OutErrors) const;

	using FMacroCommandFunction = TFunction<void(const TArray<FString>& InArgs)>;
	using FBindableMacroCommands = TMap<FName, FMacroCommandFunction>;
	const FBindableMacroCommands& GetBindableMacroCommands();

private:
	void SetTemplateSearchText(const FText& InText);

	void SetInstancedSearchText(const FText& InText);

	void OnTemplateFilterChanged();

	void OnInstancedFilterChanged();

	void InitVisibilityTemplatePages();

	void InitVisibilityInstancedPages();

protected:
	/**
	 *	Tickable object that keeps track of the play time of a page and will automatically
	 *	play the next page once the specified play time has elapsed. The measure of play time
	 *	begins when the page has started playing, the loading time is ignored.
	 *	This is tracked from the page's status.
	 */
	struct FAutoPlayTicker : public FTickableEditorObject
	{
		static constexpr double DefaultPlayInterval = 10;

		TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
		double PlayInterval = DefaultPlayInterval;
		double NextPageStartTime = 0;
		bool bIsPagePlaying = false;
		bool bIsCancelled = false;

		FAutoPlayTicker(TWeakPtr<FAvaPlaylistEditor> InPlaylistEditorWeak, double InPlayInterval);

		//~ Begin FTickableEditorObject
		virtual void Tick(float DeltaTime) override;
		virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
		virtual TStatId GetStatId() const override;
		//~ End FTickableEditorObject
	};
	TUniquePtr<FAutoPlayTicker> AutoPlayTicker;

	TSharedPtr<SDockTab> CreateSubListTab(int32 InSubListIndex);
	void CreateSubListTabs();

	void OnActiveSubListChanged();
	void HandleOnPagePlayerAdded(UAvalanchePlaylist* InPlaylist, UAvalanchePagePlayer* InPagePlayer);

	TWeakObjectPtr<UAvalanchePlaylist> AvalanchePlaylist;

	FOnPageEvent OnPageEvent;

	TMap<FName, TWeakPtr<SDockTab>> SubListTabs;

private:
	TSharedPtr<SAvaReadPage> ReadPageWidget;

	TSharedPtr<FAvaPlaylistEditorInputProcessor> InputProcessor;

	TSharedRef<FAvaPageTextFilter> TextFilterTemplatePage;
	TArray<int32> VisibleTemplatePageIds;

	TSharedRef<FAvaPageTextFilter> TextFilterInstancedPage;
	TArray<int32> VisibleInstancedPageIds;

	/**
	 * All playlist editor instances share the same command handlers.
	 * The commands get dispatched only to the active editor.
	 */
	class FSharedConsoleCommands;
	TSharedPtr<FSharedConsoleCommands> SharedConsoleCommands;

	FBindableMacroCommands BindableMacroCommands;
	
	/** Optional user specified action for stopping pages. */
	TOptional<bool> bStopPagesOnCloseOverride;
};
