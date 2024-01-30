// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAvaPageList.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/AvaPlaylistDefines.h"

class FAvaPlaylistEditor;
class IAvaPageViewColumn;
class ITableRow;
class SHeaderRow;
template <typename ItemType> class SListView;
class SSearchBox;
class STableViewBase;
struct FAvalanchePage;

class SAvaTemplatePageList : public SAvaPageList
{
	SLATE_DECLARE_WIDGET(SAvaTemplatePageList, SAvaPageList)

public:
	SLATE_BEGIN_ARGS(SAvaTemplatePageList) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor);
	virtual ~SAvaTemplatePageList() override;

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

	void AddTemplate();

	void CreateInstance();
	void CreateComboTemplate();
	bool CanCreateComboTemplate();

protected:
	virtual TArray<int32> AddPastedPages(const TArray<FAvalanchePage>& InPages) override;

private:
	void OnTemplatePageListChanged(const FAvaPageListChangeParams& InParams);
};
