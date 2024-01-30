// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaTemplatePageList.h"

#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvaPlaylistEditorUtils.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "Playlist/Pages/Columns/AvaPageAssetSelectorColumn.h"
#include "Playlist/Pages/Columns/AvaPageIdColumn.h"
#include "Playlist/Pages/Columns/AvaPageNameColumn.h"
#include "Playlist/Pages/Columns/AvaPageTemplateStatusColumn.h"
#include "Playlist/Pages/Columns/AvaPageThumbnailColumn.h"
#include "Playlist/Pages/Columns/AvaPageTransitionLayerColumn.h"
#include "Playlist/Pages/PageViews/AvaTemplatePageViewImpl.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SAvaTemplatePageList"

void SAvaTemplatePageList::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{

}

void SAvaTemplatePageList::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	SAvaPageList::Construct(SAvaPageList::FArguments(), InPlaylistEditor, UAvalanchePlaylist::TemplatePageList, EAvaPlaylistSearchListType::Template);

	PlaylistEditorWeak = InPlaylistEditor;
	check(InPlaylistEditor.IsValid());

	UAvalanchePlaylist* const Playlist = InPlaylistEditor->GetPlaylist();
	check(Playlist);

	Playlist->GetOnTemplatePageListChanged().AddSP(this, &SAvaTemplatePageList::OnTemplatePageListChanged);

	Refresh();
}

SAvaTemplatePageList::~SAvaTemplatePageList()
{
	if (UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		Playlist->GetOnTemplatePageListChanged().RemoveAll(this);
	}
}

void SAvaTemplatePageList::Refresh()
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		UAvalanchePlaylist* const Playlist = PlaylistEditor->GetPlaylist();
		if (!Playlist)
		{
			return;
		}

		const FAvalanchePageCollection& PageCollection = Playlist->GetTemplatePages();
		PageViews.Reset(PageCollection.Pages.Num());

		for (const FAvalanchePage& Page : PageCollection.Pages)
		{
			if (PlaylistEditor->IsTemplatePageVisible(Page))
			{
				PageViews.Emplace(MakeShared<FAvaTemplatePageViewImpl>(Page.GetPageId(), Playlist, SharedThis(this)));
			}
		}

		PageListView->RequestListRefresh();
	}
}

void SAvaTemplatePageList::CreateColumns()
{
	HeaderRow = SNew(SHeaderRow)
		.Visibility(EVisibility::Visible)
		.CanSelectGeneratedColumn(true);

	Columns.Empty();
	HeaderRow->ClearColumns();

	//TODO: Extensibility?
	TArray<TSharedPtr<IAvaPageViewColumn>> FoundColumns;
	FoundColumns.Add(MakeShared<FAvaPageThumbnailColumn>());
	FoundColumns.Add(MakeShared<FAvaPageIdColumn>());
	FoundColumns.Add(MakeShared<FAvaPageNameColumn>());
	FoundColumns.Add(MakeShared<FAvaPageAssetSelectorColumn>());
	FoundColumns.Add(MakeShared<FAvaPageTransitionLayerColumn>());
	FoundColumns.Add(MakeShared<FAvaPageTemplateStatusColumn>());

	for (const TSharedPtr<IAvaPageViewColumn>& Column : FoundColumns)
	{
		const FName ColumnId = Column->GetColumnId();
		Columns.Add(ColumnId, Column);
		HeaderRow->AddColumn(Column->ConstructHeaderRowColumn());
		HeaderRow->SetShowGeneratedColumn(ColumnId, false);
	}
}

TSharedPtr<SWidget> SAvaTemplatePageList::OnContextMenuOpening()
{
	if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		return GetPageListContextMenu();
	}

	return SNullWidget::NullWidget;
}

void SAvaTemplatePageList::BindCommands()
{
	SAvaPageList::BindCommands();

	//Playlist Commands
	{
		const FAvaPlaylistCommands& PlaylistCommands = FAvaPlaylistCommands::Get();

		CommandList->MapAction(PlaylistCommands.AddTemplate,
			FExecuteAction::CreateSP(this, &SAvaTemplatePageList::AddTemplate),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanAddTemplate));

		CommandList->MapAction(PlaylistCommands.CreatePageInstanceFromTemplate,
			FExecuteAction::CreateSP(this, &SAvaTemplatePageList::CreateInstance),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanCreateInstance));

		CommandList->MapAction(PlaylistCommands.CreateComboTemplate,
			FExecuteAction::CreateSP(this, &SAvaTemplatePageList::CreateComboTemplate),
			FCanExecuteAction::CreateSP(this, &SAvaTemplatePageList::CanCreateComboTemplate));

		CommandList->MapAction(PlaylistCommands.RemovePage,
			FExecuteAction::CreateSP(this, &SAvaPageList::RemoveSelectedPages),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanRemoveSelectedPages));

		CommandList->MapAction(PlaylistCommands.RenumberPage,
			FExecuteAction::CreateSP(this, &SAvaPageList::RenumberSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanRenumberSelectedPage));

		CommandList->MapAction(PlaylistCommands.ReimportPage,
			FExecuteAction::CreateSP(this, &SAvaPageList::ReimportSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanReimportSelectedPage));

		CommandList->MapAction(PlaylistCommands.EditPageSource,
			FExecuteAction::CreateSP(this, &SAvaPageList::EditSelectedPageSource),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanEditSelectedPageSource));

		CommandList->MapAction(PlaylistCommands.PreviewFrame,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewPlaySelectedPage, true),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewPlaySelectedPage));

		CommandList->MapAction(PlaylistCommands.PreviewPlay,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewPlaySelectedPage, false),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewPlaySelectedPage));

		CommandList->MapAction(PlaylistCommands.PreviewStop,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewStopSelectedPage, false),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewStopSelectedPage, false));

		CommandList->MapAction(PlaylistCommands.PreviewForceStop,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewStopSelectedPage, true),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewStopSelectedPage, true));

		CommandList->MapAction(PlaylistCommands.PreviewContinue,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewContinueSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewContinueSelectedPage));

		CommandList->MapAction(PlaylistCommands.PreviewPlayNext,
			FExecuteAction::CreateSP(this, &SAvaPageList::PreviewPlayNextPage),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPreviewPlayNextPage));

		CommandList->MapAction(PlaylistCommands.TakeToProgram,
			FExecuteAction::CreateSP(this, &SAvaPageList::TakeToProgram),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanTakeToProgram));
	}
}

bool SAvaTemplatePageList::HandleDropAvalancheAssets(const TArray<FSoftObjectPath>& InAvaAssets, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();
	if (!Playlist)
	{
		return false;
	}

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	TArray<FSoftObjectPath> NewAvaAssets = InAvaAssets;
	TArray<int32> NewTemplateIds;

	// If we are adding above, they should be reversed, so the last is added first
	// and the next to last added above that, etc.
	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(NewAvaAssets);
	}

	bool bHasValidAssets = false;
	Playlist->Modify();

	for (const FSoftObjectPath& AvaAsset : NewAvaAssets)
	{
		if (AvaAsset.IsNull())
		{
			continue;
		}

		int32 NewTemplateId = Playlist->AddTemplate(FAvaPageIdGeneratorParams::FromInsertPosition(InsertAt));
		
		if (NewTemplateId == FAvalanchePage::InvalidPageId)
		{
			continue;
		}

		Playlist->GetPage(NewTemplateId).UpdateAvalancheAsset(AvaAsset);
		InsertAt.ConditionalUpdateAdjacentId(NewTemplateId);
		NewTemplateIds.Add(NewTemplateId);

		bHasValidAssets = true;
	}

	if (bHasValidAssets)
	{
		Refresh();
		DeselectPages();
		SelectPages(NewTemplateIds);
	}

	return bHasValidAssets;
}

bool SAvaTemplatePageList::HandleDropPlaylists(const TArray<FSoftObjectPath>& InPlaylistPaths, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	// Not supported directly.
	// The templates will import automatically when the playlist pages are imported.
	return false;	
}

bool SAvaTemplatePageList::HandleDropPageIds(const FAvaPageListReference& InPageListReference, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	// Can only drop templates onto the templates list.
	if (InPageListReference.Type != EAvaPageListType::Template)
	{
		return false;
	}

	// Nothing to do.
	if (InPageIds.IsEmpty())
	{
		return true;
	}

	UAvalanchePlaylist* Playlist = GetValidPlaylist();
	if (!Playlist || !Playlist->CanChangePageOrder())
	{
		return false;
	}
	
	const FAvalanchePageCollection& TemplatePageCollection = Playlist->GetTemplatePages();
	int32 DroppedOnPageIndex = INDEX_NONE;

	if (InItem.IsValid() && TemplatePageCollection.PageIndices.Contains(InItem->GetPageId()))
	{
		DroppedOnPageIndex = TemplatePageCollection.PageIndices[InItem->GetPageId()];

		// Nothing to do.
		if (InPageIds.Num() == 1 && DroppedOnPageIndex == InPageIds[0])
		{
			return true;
		}
	}

	TArray<int32> MovedPageIdIndices;
	TArray<int32> NewPageOrder;
	TArray<int32> NewSelectedIds;

	MovedPageIdIndices.Reserve(InPageIds.Num());
	NewPageOrder.Reserve(PageViews.Num());
	NewSelectedIds.Reserve(InPageIds.Num());

	// This has the byproduct of removing invalid ids.
	for (int32 PageId : InPageIds)
	{
		if (const int32* PageIndexPtr = TemplatePageCollection.PageIndices.Find(PageId))
		{
			MovedPageIdIndices.Add(*PageIndexPtr);
			NewSelectedIds.Add(PageId);
		}
	}

	// Nothing to do.
	if (MovedPageIdIndices.IsEmpty())
	{
		return true;
	}

	for (int32 PageViewIndex = 0; PageViewIndex < PageViews.Num(); ++PageViewIndex)
	{
		const bool bHasMovedThisPage = MovedPageIdIndices.Contains(PageViewIndex);

		if (PageViewIndex == DroppedOnPageIndex)
		{
			const bool bAddBefore = InDropZone == EItemDropZone::AboveItem;

			if (bAddBefore)
			{
				NewPageOrder.Append(MovedPageIdIndices);
			}

			// If we moved the page we dropped onto, it will already be in the list
			if (!bHasMovedThisPage)
			{
				NewPageOrder.Add(PageViewIndex);
			}

			if (!bAddBefore)
			{
				NewPageOrder.Append(MovedPageIdIndices);
			}

			continue;
		}

		if (bHasMovedThisPage)
		{
			continue;
		}

		NewPageOrder.Add(PageViewIndex);
	}

	Playlist->ChangePageOrder(PageListReference, NewPageOrder);
	SelectPages(NewSelectedIds, true);

	return true;
}

bool SAvaTemplatePageList::HandleDropExternalFiles(const TArray<FString>& InFiles, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	// Not supported directly.
	// The templates will import automatically when the playlist pages are imported.
	return false;
}

void SAvaTemplatePageList::AddTemplate()
{
	if (UAvalanchePlaylist* Playlist = GetValidPlaylist())
	{
		int32 LastIndex = FAvalanchePage::InvalidPageId;

		for (const int32 SelectedIndex : SelectedPageIds)
		{
			LastIndex = FMath::Max(LastIndex, SelectedIndex);
		}

		const int32 NewTemplateId = Playlist->AddTemplate(FAvaPageIdGeneratorParams(LastIndex));

		if (NewTemplateId != FAvalanchePage::InvalidPageId)
		{
			DeselectPages();
			SelectPage(NewTemplateId);
		}
	}
}

void SAvaTemplatePageList::CreateInstance()
{
	if (SelectedPageIds.IsEmpty())
	{
		return;
	}

	if (UAvalanchePlaylist* Playlist = GetValidPlaylist())
	{
		const TArray<int32> NewPageIds = Playlist->AddPagesFromTemplates(SelectedPageIds);

		if (!NewPageIds.IsEmpty())
		{
			DeselectPages();
			SelectPages(NewPageIds);
		}
	}
}

void SAvaTemplatePageList::CreateComboTemplate()
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();
	
	if (SelectedPageIds.IsEmpty() || !Playlist)
	{
		return;
	}

	TSet<FAvaTagId> LayerIds;
	TArray<int32> TemplateIds;
	FAvalancheRemoteControlValues MergedValues;
	
	for (const int32 SelectedPageId : SelectedPageIds)
	{
		const FAvalanchePage& Page = Playlist->GetPage(SelectedPageId);
		if (!Page.IsValidPage())
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d is not valid."), SelectedPageId);
			continue;
		}
		if (!Page.IsTemplate())
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Page %d is not a template."), SelectedPageId);
			continue;
		}
		if (Page.IsComboTemplate())
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d is already a combo template."), SelectedPageId);
			continue;
		}
		if (!Page.HasTransitionLogic(Playlist))
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d doesn't have transition logic."), SelectedPageId);
			continue;
		}
		if (!Page.GetTransitionLayer(Playlist).IsValid())
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d doesn't have a valid transition logic layer."), SelectedPageId);
			continue;
		}
		if (LayerIds.Contains(Page.GetTransitionLayer(Playlist).TagId))
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d's layer %s is already in the selection."), SelectedPageId, *Page.GetTransitionLayer(Playlist).ToString());
			continue;
		}

		// Make sure the RC values can merge correctly. If not, original template needs fixing.
		if (MergedValues.HasIdCollisions(Page.GetRemoteControlValues()))
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Template %d's RemoteControl values have Id collisions with other templates in the selection."), SelectedPageId);
			continue;
		}
		
		MergedValues.Merge(Page.GetRemoteControlValues());
		LayerIds.Add(Page.GetTransitionLayer(Playlist).TagId);
		TemplateIds.Add(Page.GetPageId());
	}

	if (TemplateIds.Num() > 1)
	{
		Playlist->AddComboTemplate(TemplateIds);
	}
	else
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("CreateComboTemplate: Need at least 2 suitable templates to create a combo template."));
	}
}

bool SAvaTemplatePageList::CanCreateComboTemplate()
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();
	
	if (SelectedPageIds.Num() < 2 || !Playlist)
	{
		return false;
	}

	TSet<FAvaTagId> LayerIds;

	for (const int32 SelectedPageId : SelectedPageIds)
	{
		const FAvalanchePage& Page = Playlist->GetPage(SelectedPageId);
		if (Page.IsValidPage()
			&& Page.IsTemplate()
			&& !Page.IsComboTemplate()
			&& Page.HasTransitionLogic(Playlist)
			&& Page.GetTransitionLayer(Playlist).IsValid()
			&& !LayerIds.Contains(Page.GetTransitionLayer(Playlist).TagId))
		{
			LayerIds.Add(Page.GetTransitionLayer(Playlist).TagId);
		}
	}
	// Need more than one template to create a combo template.
	return LayerIds.Num() > 1;
}

TArray<int32> SAvaTemplatePageList::AddPastedPages(const TArray<FAvalanchePage>& InPages)
{
	if (UAvalanchePlaylist* Playlist = GetValidPlaylist())
	{
		using namespace UE::AvaPlaylistEditor::Utils;
		FImportTemplateMap ImportedTemplateIds;
		return ImportTemplatePages(Playlist, InPages, ImportedTemplateIds);
	}
	return {};
}

void SAvaTemplatePageList::OnTemplatePageListChanged(const FAvaPageListChangeParams& InParams)
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		PlaylistEditor->RefreshTemplateVisibility();
	}
	Refresh();
}

#undef LOCTEXT_NAMESPACE
