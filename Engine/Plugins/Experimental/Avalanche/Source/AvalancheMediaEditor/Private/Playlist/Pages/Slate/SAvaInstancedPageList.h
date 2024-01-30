// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAvaPageList.h"
#include "Playlist/AvaPlaylistDefines.h"

class SDockTab;
enum ETabActivationCause : uint8;

class SAvaInstancedPageList : public SAvaPageList
{
	SLATE_DECLARE_WIDGET(SAvaInstancedPageList, SAvaPageList)

public:
	SLATE_BEGIN_ARGS(SAvaInstancedPageList) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor, const FAvaPageListReference& PageListReference);
	virtual ~SAvaInstancedPageList() override;

	//~ Begin SAvaPageList
	virtual void Refresh() override;
	virtual void CreateColumns() override;
	virtual TSharedPtr<SWidget> OnContextMenuOpening() override;
	virtual void BindCommands() override;
	virtual bool HandleDropAvalancheAssets(const TArray<FSoftObjectPath>& InAvaAssets, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) override;
	virtual bool HandleDropPlaylists(const TArray<FSoftObjectPath>& InPlaylistPaths, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) override;
	virtual bool HandleDropPageIds(const FAvaPageListReference& InPageListReference, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) override;
	virtual bool HandleDropExternalFiles(const TArray<FString>& InFiles, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem) override;
	//~ End SAvaPageList

	bool HandleDropPageIdsOnMainListFromTemplates(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);
	bool HandleDropPageIdsOnMainListFromMainList(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);
	bool HandleDropPageIdsOnSubListFromTemplates(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);
	bool HandleDropPageIdsOnSubListFromMainList(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);
	bool HandleDropPageIdsOnSubListFromSubList(int32 InFromList, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem);

	void OnTabActivated(TSharedRef<SDockTab> InDockTab, ETabActivationCause InActivationCause);

	FName GetTabId() const { return TabId; }

	TSharedPtr<SDockTab> GetMyTab() const { return MyTabWeak.Pin(); }
	void SetMyTab(TSharedRef<SDockTab> InTab) { MyTabWeak = InTab; }

	/** Plays the currently selected page. */
	void PlaySelectedPage() const;
	bool CanPlaySelectedPage() const;

	bool CanUpdateValuesOnSelectedPage() const;
	void UpdateValuesOnSelectedPage();
	
	void ContinueSelectedPage() const;
	bool CanContinueSelectedPage() const;

	void StopSelectedPage(bool bInForce) const;
	bool CanStopSelectedPage(bool bInForce) const;

	TArray<int32> PlayNextPage() const;
	bool CanPlayNextPage() const;

	TArray<int32> GetPageIdsToTakeNext() const
	{
		const int32 NextPageId = GetPageIdToTakeNext();
		if (IsPageIdValid(NextPageId))
		{
			return { NextPageId };
		}
		return {};
	}
	
private:
	void OnInstancedPageListChanged(const FAvaPageListChangeParams& InParams);

	TArray<int32> FilterPlayingPages(FFilterPageFunctionRef InFilterPageFunction) const;
	TArray<int32> FilterSelectedOrPlayingPages(FFilterPageFunctionRef InFilterPageFunction, const bool bInAllowFallback) const;
	TArray<int32> FilterPageSetForProgram(FFilterPageFunctionRef InFilterPageFunction, const EAvaRundownPageSet InPageSet) const;

	TArray<int32> GetPagesToTakeIn() const;
	TArray<int32> GetPagesToTakeOut(bool bInForce) const;
	TArray<int32> GetPagesToContinue() const;
	TArray<int32> GetPagesToUpdate() const;
	int32 GetPageIdToTakeNext() const;

protected:
	FName TabId;
	TWeakPtr<SDockTab> MyTabWeak;

	FReply MakeActive();
	bool CanMakeActive() const;
	FSlateColor GetMakeActiveButtonColor() const;

	FText GetPageViewName() const;
	void OnPageViewNameCommitted(const FText& InNewText, ETextCommit::Type InCommitType);

	void PlayNextPageNoReturn() const { PlayNextPage(); }

	virtual TArray<int32> AddPastedPages(const TArray<FAvalanchePage>& InPages) override;
};
