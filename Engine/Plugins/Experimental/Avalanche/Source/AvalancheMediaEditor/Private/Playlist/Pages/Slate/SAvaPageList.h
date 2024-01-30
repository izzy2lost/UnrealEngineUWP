// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/AvaRundownEditorDefines.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;
class FAvaPlaylistPageContextMenu;
class FUICommandList;
class IAvaPageViewColumn;
class ITableRow;
class SHeaderRow;
class SHorizontalBox;
template <typename ItemType>
class SListView;
class SSearchBox;
class SAssetSearchBox;
class STableViewBase;
struct FAssetSearchBoxSuggestion;
struct FAvalanchePage;
struct FAvaPageListChangeParams;
enum class EItemDropZone;
enum class EAvaPlaylistSearchListType : uint8;

class SAvaPageList : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SAvaPageList, SCompoundWidget)

public:
	SLATE_BEGIN_ARGS(SAvaPageList) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor, const FAvaPageListReference& InPageListReference, EAvaPlaylistSearchListType InPageListType);
	virtual ~SAvaPageList() override;

	const FAvaPageListReference& GetPageListReference() const { return PageListReference; }

	void OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent);

	void OnPageItemActionRequested(const TArray<int32>& InPageIds, IAvaPageView::FOnPageAction&(IAvaPageView::*InFunc)());

	void OnPageSelectionRequested(const TArray<int32>& InPageIds);

	TSharedRef<ITableRow> GeneratePageTableRow(FAvaPageViewPtr InPageView, const TSharedRef<STableViewBase>& InOwnerTable);

	void OnPageSelected(FAvaPageViewPtr InPageView, ESelectInfo::Type InSelectInfo);

	TSharedPtr<SListView<FAvaPageViewPtr>> GetPageListView() { return PageListView; }

	TSharedPtr<IAvaPageViewColumn> FindColumn(FName InColumnName) const;

	FAvaPageViewPtr GetPageViewPtr(int32 InPageId) const;

	void SelectPage(int32 InPageId, bool bInScrollIntoView = true);
	void SelectPages(const TArray<int32>& InPageIds, bool bInScrollIntoView = true);
	void DeselectPage(int32 InPageId);
	void DeselectPages();

	const TArray<int32>& GetSelectedPageIds() const { return SelectedPageIds; }
	TArray<int32> GetPlayingPageIds() const;
	const TArray<FAvaPageViewPtr>& GetPageViews() const { return PageViews; }

	TSharedPtr<FAvaPlaylistEditor> GetPlaylistEditor() const;

	UAvalanchePlaylist* GetPlaylist() const;
	UAvalanchePlaylist* GetValidPlaylist() const;

	virtual void BindCommands();

	virtual void Refresh() = 0;

	virtual void CreateColumns() = 0;

	virtual TSharedPtr<SWidget> OnContextMenuOpening() = 0;
	
	int32 GetFirstSelectedPageId() const;

	TSharedRef<SWidget> GetPageListContextMenu();

	EAvaPlaylistSearchListType GetPageListType() const { return PageListType; }

	bool CanCopySelectedPages() const;
	void CopySelectedPages();

	bool CanCutSelectedPages() const;
	void CutSelectedPages();

	bool CanPaste() const;
	void Paste();

	bool CanDuplicateSelectedPages() const;
	void DuplicateSelectedPages();

	bool CanAddPage() const;
	bool CanAddTemplate() const;
	bool CanCreateInstance() const;

	bool CanRemoveSelectedPages() const;
	void RemoveSelectedPages();

	bool CanRenameSelectedPage() const;
	void RenameSelectedPage();

	bool CanRenumberSelectedPage() const;
	void RenumberSelectedPage();

	bool CanReimportSelectedPage() const;
	void ReimportSelectedPage() const;

	bool CanEditSelectedPageSource() const;
	void EditSelectedPageSource();

	bool CanExportSelectedPagesToPlaylist();
	void ExportSelectedPagesToPlaylist();

	bool CanExportSelectedPagesToExternalFile(const TCHAR* InType);
	void ExportSelectedPagesToExternalFile(const TCHAR* InType);

	bool CanPreviewPlaySelectedPage() const;
	void PreviewPlaySelectedPage(bool bInToMark) const;

	bool CanPreviewStopSelectedPage(bool bInForce) const;
	void PreviewStopSelectedPage(bool bInForce) const;

	bool CanPreviewContinueSelectedPage() const;
	void PreviewContinueSelectedPage() const;

	bool CanPreviewPlayNextPage() const;
	void PreviewPlayNextPage();

	bool CanTakeToProgram() const;
	void TakeToProgram() const;

	/** For the given asset, check if it supported for drop operation. */
	static bool IsAssetDropSupported(const FAssetData& InAsset, const FSoftObjectPath& InDestinationPlaylistPath);
	
	/** From the given array of asset data, filter only ava assets. */
	static TArray<FSoftObjectPath> FilterAvaAssetPaths(const TArray<FAssetData>& InAssets);

	/** From the given array of asset data, keep only the playlists. */
	static TArray<FSoftObjectPath> FilterPlaylistPaths(const TArray<FAssetData>& InAssets, const FSoftObjectPath& InDestinationPlaylistPath);

	/** Helper to convert drop parameters to insert position. */
	static FAvaPageInsertPosition MakeInsertPosition(EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);

	virtual bool CanHandleDragObjects(const FDragDropEvent& InDragDropEvent) const;
	virtual bool HandleDropEvent(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);
	
	virtual bool HandleDropAvalancheAssets(const TArray<FSoftObjectPath>& InAvaAssets, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) = 0;
	virtual bool HandleDropPlaylists(const TArray<FSoftObjectPath>& InPlaylistPaths, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) = 0;
	virtual bool HandleDropPageIds(const FAvaPageListReference& PageListReference, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) = 0;
	virtual bool HandleDropExternalFiles(const TArray<FString>& InFiles, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) = 0;

	//~ Begin SWidget
	virtual FReply OnDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnDragOver(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

protected: 
	static bool IsPageIdValid(int32 InPageId)
	{
		return InPageId != FAvalanchePage::InvalidPageId;
	}

	using FFilterPageFunctionRef = TFunctionRef<TArray<int32>(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)>;
	
	TArray<int32> FilterSelectedPages(FFilterPageFunctionRef InFilterPageFunction) const;
	TArray<int32> FilterPreviewingPages(FFilterPageFunctionRef InFilterPageFunction) const;
	TArray<int32> FilterSelectedOrPreviewingPages(FFilterPageFunctionRef InFilterPageFunction, const bool bInAllowFallback) const;
	TArray<int32> FilterPageSetForPreview(FFilterPageFunctionRef InFilterPageFunction, const EAvaRundownPageSet InPageSet) const;
	
	TArray<int32> GetPagesToPreviewIn() const;
	TArray<int32> GetPagesToPreviewOut(bool bInForce) const;
	TArray<int32> GetPagesToPreviewContinue() const;
	TArray<int32> GetPagesToTakeToProgram() const;
	int32 GetPageIdToPreviewNext() const;
	
private:
	FText OnAssetSearchBoxSuggestionChosen(const FText& InSearchText, const FString& InSuggestion);

	void OnSearchTextChanged(const FText& FilterText);

	void OnSearchTextCommitted(const FText& FilterText, ETextCommit::Type CommitType);

	void OnSearchBoxSuggestionFilter(const FText& InSearchText, TArray<FAssetSearchBoxSuggestion>& OutPossibleSuggestions, FText& OutSuggestionHighlightText);

	TArray<FAvalanchePage> GetPagesByType(EAvaPlaylistSearchListType InPlaylistSearchListType) const;

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	TSharedPtr<FAvaPlaylistPageContextMenu> PageContextMenu;

	FAvaPageListReference PageListReference;

	TSharedPtr<SHorizontalBox> SearchBar;

	TSharedPtr<SHeaderRow> HeaderRow;

	TMap<FName, TSharedPtr<IAvaPageViewColumn>> Columns;

	TSharedPtr<SListView<FAvaPageViewPtr>> PageListView;

	TArray<FAvaPageViewPtr> PageViews;

	TArray<int32> SelectedPageIds;

	TSharedPtr<FUICommandList> CommandList;

	/**
	 * For template and instance lists, it will return the list of page ids created.
	 * For a sub list it will return the list of page ids that were added to the page view.
	 */
	virtual TArray<int32> AddPastedPages(const TArray<FAvalanchePage>& InPages) = 0;

private:
	TSharedPtr<SAssetSearchBox> AssetSearchBoxPtr;

	EAvaPlaylistSearchListType PageListType;
};
