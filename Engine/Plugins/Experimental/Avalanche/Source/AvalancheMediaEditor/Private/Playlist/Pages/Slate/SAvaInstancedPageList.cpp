// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaInstancedPageList.h"

#include "Misc/PathViews.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvaPlaylistEditorUtils.h"
#include "Playlist/AvaPlaylistPlaybackUtils.h"
#include "Playlist/AvaRundownEditorSettings.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "Playlist/Pages/Columns/AvaPageAssetNameColumn.h"
#include "Playlist/Pages/Columns/AvaPageChannelSelectorColumn.h"
#include "Playlist/Pages/Columns/AvaPageEnabledColumn.h"
#include "Playlist/Pages/Columns/AvaPageIdColumn.h"
#include "Playlist/Pages/Columns/AvaPageNameColumn.h"
#include "Playlist/Pages/Columns/AvaPageStatusColumn.h"
#include "Playlist/Pages/Columns/AvaPageTemplateNameColumn.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Playlist/TabFactories/AvaInstancedPageListTabFactory.h"
#include "Playlist/TabFactories/AvaSubListDocumentTabFactory.h"

#include "SAvaPageList.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SAvaInstancedPageList"

void SAvaInstancedPageList::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{

}

void SAvaInstancedPageList::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor, const FAvaPageListReference& InPageListReference)
{
	SAvaPageList::Construct(SAvaPageList::FArguments(), InPlaylistEditor, InPageListReference, EAvaPlaylistSearchListType::Instanced);

	UAvalanchePlaylist* const Playlist = InPlaylistEditor->GetPlaylist();
	check(Playlist);

	check(InPageListReference.Type == EAvaPageListType::Instance || Playlist->IsValidSubList(PageListReference));

	if (PageListReference.Type == EAvaPageListType::Instance)
	{
		TabId = FAvaInstancedPageListTabFactory::TabID;
		Playlist->GetOnInstancedPageListChanged().AddSP(this, &SAvaInstancedPageList::OnInstancedPageListChanged);
	}
	else
	{
		FAvaSubListDocumentTabFactory::GetTabId(InPageListReference.SubListIndex);
		Playlist->GetSubList(InPageListReference.SubListIndex).OnPageListChanged.AddSP(this, &SAvaInstancedPageList::OnInstancedPageListChanged);
	}

	if (Playlist->IsValidSubList(PageListReference))
	{
		SearchBar->InsertSlot(0)
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(5.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Name", "Name:"))
			];

		SearchBar->InsertSlot(1)
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(5.f, 0.f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("PageViewName", "Page View Name"))
				.OnTextCommitted(this, &SAvaInstancedPageList::OnPageViewNameCommitted)
				.Text(this, &SAvaInstancedPageList::GetPageViewName)
				.MinDesiredWidth(150.f)
			];
	}

	SearchBar->AddSlot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(5.f, 0.f)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("MakeActiveTooltip", "Page List used for Show Controls (Take In, etc.)"))
			.OnClicked(this, &SAvaInstancedPageList::MakeActive)
			.IsEnabled(this, &SAvaInstancedPageList::CanMakeActive)
			.Visibility_Lambda([this]()
			{
				if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
				{
					UAvalanchePlaylist* const Playlist = PlaylistEditor->GetPlaylist();
			
					if (IsValid(Playlist))
					{
						return Playlist->GetSubLists().IsEmpty()? EVisibility::Collapsed : EVisibility::Visible;
					}
				}
				return EVisibility::Collapsed;
			})
			.ButtonColorAndOpacity(this, &SAvaInstancedPageList::GetMakeActiveButtonColor)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ActivePage", "Show-Control Pages"))
			]
		];

	Refresh();
}

SAvaInstancedPageList::~SAvaInstancedPageList()
{
	if (UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		if (PageListReference.Type == EAvaPageListType::Instance)
		{
			Playlist->GetOnInstancedPageListChanged().RemoveAll(this);
		}
		else if (Playlist->IsValidSubList(PageListReference))
		{
			Playlist->GetSubList(PageListReference.SubListIndex).OnPageListChanged.RemoveAll(this);
		}
	}
}

void SAvaInstancedPageList::Refresh()
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		UAvalanchePlaylist* const Playlist = PlaylistEditor->GetPlaylist();
		check(Playlist);

		TArray<FAvalanchePage> Pages = Playlist->GetInstancedPages().Pages;

		if (Playlist->IsValidSubList(PageListReference))
		{
			Pages.Empty();

			for (const int32 PageId : Playlist->GetSubList(PageListReference.SubListIndex).PageIds)
			{
				if (const int32* Index = Playlist->GetInstancedPages().PageIndices.Find(PageId))
				{
					Pages.Add(Playlist->GetInstancedPages().Pages[*Index]);
				}
			}
		}

		PageViews.Reset(Pages.Num());

		for (const FAvalanchePage& Page : Pages)
		{
			if (PlaylistEditor->IsInstancedPageVisible(Page))
			{
				PageViews.Emplace(MakeShared<FAvaInstancedPageViewImpl>(Page.GetPageId(), Playlist, SharedThis(this)));
			}
		}

		PageListView->RequestListRefresh();
	}
}

void SAvaInstancedPageList::CreateColumns()
{
	HeaderRow = SNew(SHeaderRow)
		.Visibility(EVisibility::Visible)
		.CanSelectGeneratedColumn(true);

	Columns.Empty();
	HeaderRow->ClearColumns();

	//TODO: Extensibility?
	TArray<TSharedPtr<IAvaPageViewColumn>> FoundColumns;
	FoundColumns.Add(MakeShared<FAvaPageEnabledColumn>());
	FoundColumns.Add(MakeShared<FAvaPageIdColumn>());
	FoundColumns.Add(MakeShared<FAvaPageTemplateNameColumn>());
	FoundColumns.Add(MakeShared<FAvaPageNameColumn>());
	FoundColumns.Add(MakeShared<FAvaPageAssetNameColumn>());
	FoundColumns.Add(MakeShared<FAvaPageChannelSelectorColumn>());
	FoundColumns.Add(MakeShared<FAvaPageStatusColumn>());

	for (const TSharedPtr<IAvaPageViewColumn>& Column : FoundColumns)
	{
		const FName ColumnId = Column->GetColumnId();
		Columns.Add(ColumnId, Column);
		HeaderRow->AddColumn(Column->ConstructHeaderRowColumn());
		HeaderRow->SetShowGeneratedColumn(ColumnId, false);
	}
}

TSharedPtr<SWidget> SAvaInstancedPageList::OnContextMenuOpening()
{
	if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		return GetPageListContextMenu();
	}

	return SNullWidget::NullWidget;
}

void SAvaInstancedPageList::BindCommands()
{
	SAvaPageList::BindCommands();

	//Playlist Commands
	{
		const FAvaPlaylistCommands& PlaylistCommands = FAvaPlaylistCommands::Get();

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

		CommandList->MapAction(PlaylistCommands.ExportPagesToPlaylist,
			FExecuteAction::CreateSP(this, &SAvaPageList::ExportSelectedPagesToPlaylist),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanExportSelectedPagesToPlaylist));

		CommandList->MapAction(PlaylistCommands.ExportPagesToJson,
			FExecuteAction::CreateSP(this, &SAvaPageList::ExportSelectedPagesToExternalFile, TEXT("json")),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanExportSelectedPagesToExternalFile, TEXT("json")));

		CommandList->MapAction(PlaylistCommands.ExportPagesToXml,
			FExecuteAction::CreateSP(this, &SAvaPageList::ExportSelectedPagesToExternalFile, TEXT("xml")),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanExportSelectedPagesToExternalFile, TEXT("xml")));

		CommandList->MapAction(PlaylistCommands.Play,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::PlaySelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanPlaySelectedPage));

		CommandList->MapAction(PlaylistCommands.UpdateValues,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::UpdateValuesOnSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanUpdateValuesOnSelectedPage));

		CommandList->MapAction(PlaylistCommands.Stop,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::StopSelectedPage, false),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanStopSelectedPage, false));

		CommandList->MapAction(PlaylistCommands.ForceStop,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::StopSelectedPage, true),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanStopSelectedPage, true));

		CommandList->MapAction(PlaylistCommands.Continue,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::ContinueSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanContinueSelectedPage));

		CommandList->MapAction(PlaylistCommands.PlayNext,
			FExecuteAction::CreateSP(this, &SAvaInstancedPageList::PlayNextPageNoReturn),
			FCanExecuteAction::CreateSP(this, &SAvaInstancedPageList::CanPlayNextPage));

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

bool SAvaInstancedPageList::HandleDropAvalancheAssets(const TArray<FSoftObjectPath>& InAvaAssets, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist)
	{
		return false;
	}
	
	if (PageListReference.Type == EAvaPageListType::View && !Playlist->IsValidSubList(PageListReference))
	{
		return false;
	}

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	
	TArray<FSoftObjectPath> NewAvaAssets = InAvaAssets;
	TArray<int32> NewPageIds;

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

		// Create Template
		const int32 NewTemplateId = Playlist->AddTemplate();

		if (NewTemplateId == FAvalanchePage::InvalidPageId)
		{
			continue;
		}

		Playlist->GetPage(NewTemplateId).UpdateAvalancheAsset(AvaAsset);

		int32 NewInstanceId = FAvalanchePage::InvalidPageId;

		if (PageListReference.Type == EAvaPageListType::Instance && InsertAt.IsValid())
		{
			NewInstanceId = Playlist->AddPageFromTemplate(NewTemplateId, FAvaPageIdGeneratorParams::FromInsertPosition(InsertAt), InsertAt);
		}
		else
		{
			NewInstanceId = Playlist->AddPageFromTemplate(NewTemplateId);
		}

		if (NewInstanceId == FAvalanchePage::InvalidPageId)
		{
			continue;
		}

		if (PageListReference.Type == EAvaPageListType::View)
		{
			Playlist->AddPageToSubList(PageListReference.SubListIndex, NewInstanceId, InsertAt);
		}

		InsertAt.ConditionalUpdateAdjacentId(NewInstanceId);
		NewPageIds.Add(NewInstanceId);
		bHasValidAssets = true;
	}

	if (bHasValidAssets)
	{
		Refresh();
		DeselectPages();
		SelectPages(NewPageIds);
	}

	return bHasValidAssets;
}

bool SAvaInstancedPageList::HandleDropPlaylists(const TArray<FSoftObjectPath>& InPlaylistPaths, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist)
	{
		return false;
	}

	FAvaPageInsertPosition InsertPosition = MakeInsertPosition(InDropZone, InItem);
	
	TArray<FSoftObjectPath> NewPlaylistPaths = InPlaylistPaths;
	TArray<int32> NewPageIds;
	
	// If we are adding above, they should be reversed, so the last is added first
	// and the next to last added above that, etc.
	if (!InsertPosition.bAddBelow)
	{
		Algo::Reverse(NewPlaylistPaths);
	}
	
	Playlist->Modify();

	for (const FSoftObjectPath& PlaylistPath : NewPlaylistPaths)
	{
		if (PlaylistPath.IsNull())
		{
			continue;
		}

		if (const UAvalanchePlaylist* SourcePlaylist = Cast<UAvalanchePlaylist>(PlaylistPath.TryLoad()))
		{
			TArray<int32> ImportedPageIds = UE::AvaPlaylistEditor::Utils::ImportInstancedPagesFromPlaylist(Playlist, SourcePlaylist, InsertPosition);
			if (!ImportedPageIds.IsEmpty())
			{
				NewPageIds.Append(ImportedPageIds);
				InsertPosition.ConditionalUpdateAdjacentId(NewPageIds.Last());	// todo: last or first, depends on AddBelow.
			}
		}
	}

	if (!NewPageIds.IsEmpty())
	{
		Refresh();
		DeselectPages();
		SelectPages(NewPageIds);
	}

	return !NewPageIds.IsEmpty();
}

bool SAvaInstancedPageList::HandleDropPageIds(const FAvaPageListReference& InPageListReference, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	// Nothing to do.
	if (InPageIds.IsEmpty())
	{
		return true;
	}
	
	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist)
	{
		return false;
	}

	const bool bFromTemplates = InPageListReference.Type == EAvaPageListType::Template;
	const bool bFromMainList = InPageListReference.Type == EAvaPageListType::Instance;
	const bool bFromSubList = Playlist->IsValidSubList(InPageListReference);

	if (!bFromTemplates && !bFromMainList && !bFromSubList)
	{
		return false;
	}

	const bool bToMainList = PageListReference.Type == EAvaPageListType::Instance;
	const bool bToSubList = Playlist->IsValidSubList(PageListReference);

	if (!bToMainList && !bToSubList)
	{
		return false;
	}

	FScopedTransaction DropTransaction(LOCTEXT("DropTransaction", "Drop Pages"));
	Playlist->Modify();
	bool bDidSomething = false;

	if (bToMainList)
	{
		if (bFromTemplates)
		{
			bDidSomething = HandleDropPageIdsOnMainListFromTemplates(InPageIds, InDropZone, InItem);
		}
		else if (bFromMainList)
		{
			bDidSomething = HandleDropPageIdsOnMainListFromMainList(InPageIds, InDropZone, InItem);
		}
		// Cannot drop from sub list to main list
	}
	else if (bToSubList)
	{
		if (bFromTemplates)
		{
			bDidSomething = HandleDropPageIdsOnSubListFromTemplates(InPageIds, InDropZone, InItem);
		}
		else if (bFromMainList)
		{
			bDidSomething = HandleDropPageIdsOnSubListFromMainList(InPageIds, InDropZone, InItem);
		}
		else if (bFromSubList)
		{
			bDidSomething = HandleDropPageIdsOnSubListFromSubList(InPageListReference.SubListIndex, InPageIds, InDropZone, InItem);
		}
	}

	if (!bDidSomething)
	{
		// We didn't do anything.
		DropTransaction.Cancel();
	}

	return bDidSomething;
}

bool SAvaInstancedPageList::HandleDropPageIdsOnMainListFromTemplates(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (PageListReference.Type != EAvaPageListType::Instance)
	{
		return false;
	}

	// Nothing to do.
	if (InPageIds.IsEmpty())
	{
		return true;
	}

	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist || !Playlist->CanAddPage())
	{
		return false;
	}

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);	

	if (InsertAt.IsValid())
	{
		const int32 DroppedOnPageIndex = Playlist->GetInstancedPages().GetPageIndex(InsertAt.AdjacentId);

		if (DroppedOnPageIndex == INDEX_NONE)
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
	}
	
	TArray<int32> ActualPageIds = InPageIds;
	TArray<int32> NewPageIds;
	NewPageIds.Reserve(InPageIds.Num());

	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(ActualPageIds);
	}

	for (const int32 PageId : ActualPageIds)
	{
		int32 NewInstanceId = Playlist->AddPageFromTemplate(PageId, FAvaPageIdGeneratorParams::FromInsertPosition(InsertAt), InsertAt);

		if (NewInstanceId != FAvalanchePage::InvalidPageId)
		{
			NewPageIds.Add(NewInstanceId);
			InsertAt.ConditionalUpdateAdjacentId(NewInstanceId);
		}
	}

	if (NewPageIds.IsEmpty())
	{
		return false;
	}

	SelectPages(NewPageIds, true);

	return true;
}

bool SAvaInstancedPageList::HandleDropExternalFiles(const TArray<FString>& InFiles, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist)
	{
		return false;
	}
	
	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	
	TArray<FString> NewFiles = InFiles;
	TArray<int32> NewPageIds;

	// If we are adding above, they should be reversed, so the last is added first
	// and the next to last added above that, etc.
	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(NewFiles);
	}
	
	Playlist->Modify();
	
	for (const FString& File : InFiles)
	{
		if (FPathViews::GetExtension(File).Equals(TEXT("json"), ESearchCase::IgnoreCase))
		{
			using namespace UE::AvaPlaylistEditor::Utils;
			const TStrongObjectPtr<UAvalanchePlaylist> TmpPlaylist(NewObject<UAvalanchePlaylist>());
			
			if (LoadPlaylistFromJson(TmpPlaylist.Get(), *File))
			{
				if (!TmpPlaylist->GetInstancedPages().Pages.IsEmpty())
				{
					TArray<int32> ImportedPageIds = ImportInstancedPagesFromPlaylist(GetPlaylist(), TmpPlaylist.Get(), InsertAt);
					if (!ImportedPageIds.IsEmpty())
					{
						NewPageIds.Append(ImportedPageIds);
						InsertAt.ConditionalUpdateAdjacentId(NewPageIds.Last());	// todo: last or first, depends on AddBelow.
					}
					else
					{
						UE_LOG(LogAvaPlaylist, Error, TEXT("Failed to merge %s in current playlist."), *File);	
					}
				}
				else
				{
					UE_LOG(LogAvaPlaylist, Warning, TEXT("Merging %s has no instanced pages to merge."), *File);
				}
			}
			else
			{
				UE_LOG(LogAvaPlaylist, Error, TEXT("%s is not a valid playlist."), *File);
			}
		}
		else
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("%s is not a supported file type. Only playlists in \"json\" format are supported."), *File);
		}
	}
	
	if (!NewPageIds.IsEmpty())
	{
		Refresh();
		DeselectPages();
		SelectPages(NewPageIds);
	}

	return !NewPageIds.IsEmpty();
}

bool SAvaInstancedPageList::HandleDropPageIdsOnMainListFromMainList(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (PageListReference.Type != EAvaPageListType::Instance)
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

	const FAvalanchePageCollection& InstancedPageCollection = Playlist->GetInstancedPages();
	int32 DroppedOnPageIndex = INDEX_NONE;

	if (InItem.IsValid() && InstancedPageCollection.PageIndices.Contains(InItem->GetPageId()))
	{
		DroppedOnPageIndex = InstancedPageCollection.PageIndices[InItem->GetPageId()];

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
		if (const int32* PageIndexPtr = InstancedPageCollection.PageIndices.Find(PageId))
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

	Playlist->ChangePageOrder(UAvalanchePlaylist::InstancePageList, NewPageOrder);
	SelectPages(NewSelectedIds, true);

	return true;
}

bool SAvaInstancedPageList::HandleDropPageIdsOnSubListFromTemplates(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (PageListReference.Type != EAvaPageListType::View)
	{
		return false;
	}

	// Nothing to do.
	if (InPageIds.IsEmpty())
	{
		return true;
	}

	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist || !Playlist->CanAddPage() || !Playlist->IsValidSubList(PageListReference))
	{
		return false;
	}

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	
	if (InsertAt.IsValid())
	{
		// Make sure the sublist has the dropped on item. (It should)
		const FAvalancheSubList& ToSubList = Playlist->GetSubList(PageListReference.SubListIndex);
		if (!ToSubList.PageIds.Contains(InsertAt.AdjacentId))
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
	}
	
	TArray<int32> ActualPageIds = InPageIds;
	TArray<int32> NewPageIds;
	NewPageIds.Reserve(InPageIds.Num());

	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(ActualPageIds);
	}

	for (const int32 PageId : ActualPageIds)
	{
		const int32 NewInstanceId = Playlist->AddPageFromTemplate(PageId, FAvaPageIdGeneratorParams::FromInsertPosition(InsertAt), InsertAt);

		if (NewInstanceId != FAvalanchePage::InvalidPageId)
		{
			NewPageIds.Add(NewInstanceId);
		}
	}

	if (NewPageIds.IsEmpty())
	{
		return false;
	}

	for (const int32 PageId : NewPageIds)
	{
		if (Playlist->AddPageToSubList(PageListReference.SubListIndex, PageId, InsertAt))
		{
			InsertAt.ConditionalUpdateAdjacentId(PageId);
		}
	}

	SelectPages(NewPageIds, true);

	return true;
}

bool SAvaInstancedPageList::HandleDropPageIdsOnSubListFromMainList(const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (PageListReference.Type != EAvaPageListType::View)
	{
		return false;
	}

	// Nothing to do.
	if (InPageIds.IsEmpty())
	{
		return true;
	}

	UAvalanchePlaylist* Playlist = GetValidPlaylist();

	if (!Playlist || !Playlist->CanAddPage() || !Playlist->IsValidSubList(PageListReference))
	{
		return false;
	}

	FAvalancheSubList& ToSubList = Playlist->GetSubList(PageListReference.SubListIndex);

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	
	int32 DroppedOnPageIndex = INDEX_NONE;

	if (InsertAt.IsValid())
	{
		DroppedOnPageIndex = ToSubList.PageIds.IndexOfByKey(InsertAt.AdjacentId);
		if (DroppedOnPageIndex == INDEX_NONE)
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
	}

	TArray<int32> ActualPageIds;
	ActualPageIds.Reserve(InPageIds.Num());

	// Do not add pages already in this sub list
	for (int32 PageId : InPageIds)
	{
		if (!ToSubList.PageIds.Contains(PageId))
		{
			ActualPageIds.Add(PageId);
		}
	}

	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(ActualPageIds);
	}

	if (InsertAt.IsValid() && InPageIds.Contains(InsertAt.AdjacentId))
	{
		// If we have a valid index and we're adding above, iterate up until we find one we're not removing
		if (DroppedOnPageIndex != INDEX_NONE && InsertAt.IsAddAbove())
		{
			for (DroppedOnPageIndex = DroppedOnPageIndex - 1; DroppedOnPageIndex > INDEX_NONE; --DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}

		// If we have no valid index, iterate from the end of the array until we find out that we aren't removing
		if (DroppedOnPageIndex == INDEX_NONE)
		{
			for (DroppedOnPageIndex = ToSubList.PageIds.Num() - 1; DroppedOnPageIndex > INDEX_NONE; --DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}
		else if (InsertAt.IsAddBelow())
		{
			for (DroppedOnPageIndex = DroppedOnPageIndex + 1; DroppedOnPageIndex < ToSubList.PageIds.Num(); ++DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}

		// If we still have no valid index, just forget about dropping on a page
		if (DroppedOnPageIndex == INDEX_NONE || !ToSubList.PageIds.IsValidIndex(DroppedOnPageIndex))
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
		// Make sure our id is correct.
		else
		{
			InsertAt.AdjacentId = ToSubList.PageIds[DroppedOnPageIndex];
		}
	}

	for (const int32 PageId : ActualPageIds)
	{
		Playlist->AddPageToSubList(PageListReference.SubListIndex, PageId, InsertAt);
		InsertAt.ConditionalUpdateAdjacentId(PageId);
	}

	SelectPages(ActualPageIds, true);

	return true;
}

bool SAvaInstancedPageList::HandleDropPageIdsOnSubListFromSubList(int32 InFromList, const TArray<int32>& InPageIds, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (PageListReference.Type != EAvaPageListType::View)
	{
		return false;
	}

	UAvalanchePlaylist* Playlist = GetValidPlaylist();
	if (!Playlist || !Playlist->IsValidSubList(PageListReference) || !Playlist->IsValidSubListIndex(InFromList))
	{
		return false;
	}

	if (PageListReference.SubListIndex == InFromList)
	{
		if (!Playlist->CanChangePageOrder())
		{
			return false;
		}
	}
	else
	{
		if (!Playlist->CanAddPage())
		{
			return false;
		}
	}

	FAvalancheSubList& ToSubList = Playlist->GetSubList(PageListReference.SubListIndex);

	FAvaPageInsertPosition InsertAt = MakeInsertPosition(InDropZone, InItem);
	
	int32 DroppedOnPageIndex = INDEX_NONE;

	if (InsertAt.IsValid())
	{
		DroppedOnPageIndex = ToSubList.PageIds.IndexOfByKey(InsertAt.AdjacentId);

		if (DroppedOnPageIndex == INDEX_NONE)
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
	}

	TArray<int32> ActualPageIds;
	ActualPageIds.Reserve(InPageIds.Num());

	// Nothing to do
	if (PageListReference.SubListIndex == InFromList)
	{
		if (ToSubList.PageIds.Num() <= 1)
		{
			return true;
		}

		// Remove and re-add all pages
		ActualPageIds = InPageIds;
	}
	else
	{
		// Do not add pages already in this sub list
		for (int32 PageId : InPageIds)
		{
			if (!ToSubList.PageIds.Contains(PageId))
			{
				ActualPageIds.Add(PageId);
			}
		}
	}

	// Nothing to do
	if (ActualPageIds.IsEmpty())
	{
		return true;
	}

	if (InsertAt.IsAddAbove())
	{
		Algo::Reverse(ActualPageIds);
	}

	if (InsertAt.IsValid() && InPageIds.Contains(InsertAt.AdjacentId))
	{
		// If we have a valid index and we're adding above, iterate up until we find one we're not removing
		if (DroppedOnPageIndex != INDEX_NONE && InsertAt.IsAddAbove())
		{
			for (DroppedOnPageIndex = DroppedOnPageIndex - 1; DroppedOnPageIndex > INDEX_NONE; --DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}

		// If we have no valid index, iterate from the end of the array until we find out that we aren't removing
		if (DroppedOnPageIndex == INDEX_NONE)
		{
			for (DroppedOnPageIndex = ToSubList.PageIds.Num() - 1; DroppedOnPageIndex > INDEX_NONE; --DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}
		else if (InsertAt.IsAddBelow())
		{
			for (DroppedOnPageIndex = DroppedOnPageIndex + 1; DroppedOnPageIndex < ToSubList.PageIds.Num(); ++DroppedOnPageIndex)
			{
				if (InPageIds.Contains(ToSubList.PageIds[DroppedOnPageIndex]) == false)
				{
					break;
				}
			}
		}

		// If we still have no valid index, just forget about dropping on a page
		if (DroppedOnPageIndex == INDEX_NONE || !ToSubList.PageIds.IsValidIndex(DroppedOnPageIndex))
		{
			InsertAt.AdjacentId = FAvalanchePage::InvalidPageId;
		}
		// Make sure our id is correct.
		else
		{
			InsertAt.AdjacentId = ToSubList.PageIds[DroppedOnPageIndex];
		}
	}

	Playlist->RemovePagesFromSubList(InFromList, InPageIds);

	for (const int32 PageId : ActualPageIds)
	{
		Playlist->AddPageToSubList(PageListReference.SubListIndex, PageId, InsertAt);
		InsertAt.ConditionalUpdateAdjacentId(PageId);
	}

	SelectPages(ActualPageIds, true);

	return true;
}

void SAvaInstancedPageList::OnTabActivated(TSharedRef<SDockTab> InDockTab, ETabActivationCause InActivationCause)
{
	MakeActive();
}

void SAvaInstancedPageList::PlaySelectedPage() const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const TArray<int32> PageIds = GetPagesToTakeIn();
		Playlist->PlayPages(PageIds, EAvaPlayType::PlayFromStart);
	}
}

bool SAvaInstancedPageList::CanPlaySelectedPage() const
{
	const TArray<int32> PageIds = GetPagesToTakeIn();
	return !PageIds.IsEmpty();
}

bool SAvaInstancedPageList::CanUpdateValuesOnSelectedPage() const
{
	const TArray<int32> PageIds = GetPagesToUpdate();
	return !PageIds.IsEmpty();
}

void SAvaInstancedPageList::UpdateValuesOnSelectedPage()
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const TArray<int32> PageIds = GetPagesToUpdate();
		for (const int32 PageId : PageIds)
		{
			Playlist->PushRuntimeRemoteControlValues(PageId, false);
		}
	}
}

void SAvaInstancedPageList::ContinueSelectedPage() const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const TArray<int32> PageIds = GetPagesToContinue();
		for (const int32 PageId : PageIds)
		{
			Playlist->ContinuePage(PageId, false);
		}
	}
}

bool SAvaInstancedPageList::CanContinueSelectedPage() const
{
	const TArray<int32> PageIds = GetPagesToContinue();
	return !PageIds.IsEmpty();
}

void SAvaInstancedPageList::StopSelectedPage(bool bInForce) const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const EAvaPlaylistPageStopOptions StopOptions = bInForce ? EAvaPlaylistPageStopOptions::ForceNoTransition : EAvaPlaylistPageStopOptions::Default;
		const TArray<int32> PageIds = GetPagesToTakeOut(bInForce);
		Playlist->StopPages(PageIds, StopOptions, false);
	}
}

bool SAvaInstancedPageList::CanStopSelectedPage(bool bInForce) const
{
	const TArray<int32> PageIds = GetPagesToTakeOut(bInForce);
	return !PageIds.IsEmpty();
}

TArray<int32> SAvaInstancedPageList::PlayNextPage() const
{
	TArray<int32> NextPageIds;
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const int32 PageIdToTakeNext = GetPageIdToTakeNext();
		if (IsPageIdValid(PageIdToTakeNext))
		{
			Playlist->PlayPage(PageIdToTakeNext, EAvaPlayType::PlayFromStart);
			NextPageIds.Add(PageIdToTakeNext);
		}
	}
	return NextPageIds;
}

bool SAvaInstancedPageList::CanPlayNextPage() const
{
	const int32 PageIdToTakeNext = GetPageIdToTakeNext();
	return IsPageIdValid(PageIdToTakeNext);
}

void SAvaInstancedPageList::OnInstancedPageListChanged(const FAvaPageListChangeParams& InParams)
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		PlaylistEditor->RefreshInstancedVisibility();
	}
	Refresh();
}

FReply SAvaInstancedPageList::MakeActive()
{
	if (CanMakeActive())
	{
		GetPlaylist()->SetActivePageList(PageListReference);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SAvaInstancedPageList::CanMakeActive() const
{
	if (const UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		if (Playlist->IsPlaying())
		{
			return false;
		}
		return (Playlist->GetActivePageListReference() != PageListReference);
	}
	return false;
}

FSlateColor SAvaInstancedPageList::GetMakeActiveButtonColor() const
{
	static const FSlateColor Inactive(FStyleColors::AccentGray.GetSpecifiedColor());
	static const FSlateColor Active(FStyleColors::PrimaryHover.GetSpecifiedColor());

	if (const UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		if (Playlist->GetActivePageListReference() == PageListReference)
		{
			return Active;
		}
	}

	return Inactive;
}

FText SAvaInstancedPageList::GetPageViewName() const
{
	if (UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		if (Playlist->IsValidSubList(PageListReference))
		{
			return Playlist->GetSubList(PageListReference.SubListIndex).Name;
		}
	}

	return FText::GetEmpty();
}

void SAvaInstancedPageList::OnPageViewNameCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::Default || InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (UAvalanchePlaylist* const Playlist = GetValidPlaylist())
	{
		if (Playlist->IsValidSubList(PageListReference))
		{
			Playlist->GetSubList(PageListReference.SubListIndex).Name = InNewText;
			Playlist->GetSubList(PageListReference.SubListIndex).OnPageListChanged.Broadcast({Playlist, EAvaPageListChange::RenamedPageView, {}});

			if (TSharedPtr<SDockTab> MyTab = MyTabWeak.Pin())
			{
				MyTab->SetLabel(InNewText);
			}
		}
	}
}

TArray<int32> SAvaInstancedPageList::AddPastedPages(const TArray<FAvalanchePage>& InPages)
{
	if (UAvalanchePlaylist* Playlist = GetValidPlaylist())
	{
		using namespace UE::AvaPlaylistEditor::Utils;
		FImportTemplateMap ImportedTemplateIds;
		return ImportInstancedPages(Playlist, PageListReference, InPages, {}, ImportedTemplateIds);
	}
	return {};
}

TArray<int32> SAvaInstancedPageList::FilterPlayingPages(FFilterPageFunctionRef InFilterPageFunction) const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		return InFilterPageFunction(Playlist, Playlist->GetPlayingPageIds());
	}
	return {};
}

TArray<int32> SAvaInstancedPageList::FilterSelectedOrPlayingPages(FFilterPageFunctionRef InFilterPageFunction, const bool bInAllowFallback) const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		if (!SelectedPageIds.IsEmpty())
		{
			const TArray<int32> SelectedPages = InFilterPageFunction(Playlist, SelectedPageIds);
			if (!bInAllowFallback || !SelectedPages.IsEmpty())
			{
				return SelectedPages;
			}
		}
		return InFilterPageFunction(Playlist, Playlist->GetPlayingPageIds());
	}
	return {};
}

TArray<int32> SAvaInstancedPageList::FilterPageSetForProgram(FFilterPageFunctionRef InFilterPageFunction, const EAvaRundownPageSet InPageSet) const
{
	switch (InPageSet)
	{
	case EAvaRundownPageSet::SelectedOrPlayingStrict:
		return FilterSelectedOrPlayingPages(InFilterPageFunction, /*bAllowFallback*/ false);
	case EAvaRundownPageSet::SelectedOrPlaying:
		return FilterSelectedOrPlayingPages(InFilterPageFunction, /*bAllowFallback*/ true);
	case EAvaRundownPageSet::Selected:
		return FilterSelectedPages(InFilterPageFunction);
	case EAvaRundownPageSet::Playing:
		return FilterPlayingPages(InFilterPageFunction);
	default:
		return FilterSelectedOrPlayingPages(InFilterPageFunction, /*bAllowFallback*/ true);
	}
}

TArray<int32> SAvaInstancedPageList::GetPagesToTakeIn() const
{
	auto KeepPagesToPlay = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanPlayPage(PageId, false))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};
	return FilterSelectedPages(KeepPagesToPlay);
}

TArray<int32> SAvaInstancedPageList::GetPagesToTakeOut(bool bInForce) const
{
	const EAvaPlaylistPageStopOptions StopOptions = bInForce ? EAvaPlaylistPageStopOptions::ForceNoTransition : EAvaPlaylistPageStopOptions::Default;
	auto KeepPagesToStop = [StopOptions](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanStopPage(PageId, StopOptions, false))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};

	const UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::Get();
	const EAvaRundownPageSet PageSet = RundownEditorSettings ? RundownEditorSettings->TakeOutActionPageSet : EAvaRundownPageSet::SelectedOrPlaying;
	return FilterPageSetForProgram(KeepPagesToStop, PageSet);
}

TArray<int32> SAvaInstancedPageList::GetPagesToContinue() const
{
	auto KeepPagesToContinue = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanContinuePage(PageId, false))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};

	const UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::Get();
	const EAvaRundownPageSet PageSet = RundownEditorSettings ? RundownEditorSettings->ContinueActionPageSet : EAvaRundownPageSet::SelectedOrPlaying;
	return FilterPageSetForProgram(KeepPagesToContinue, PageSet);
}

TArray<int32> SAvaInstancedPageList::GetPagesToUpdate() const
{
	auto KeepPagesToUpdate = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->IsPagePlaying(PageId))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};

	const UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::Get();
	const EAvaRundownPageSet PageSet = RundownEditorSettings ? RundownEditorSettings->UpdateValuesActionPageSet : EAvaRundownPageSet::SelectedOrPlaying;
	return FilterPageSetForProgram(KeepPagesToUpdate, PageSet);
}

int32 SAvaInstancedPageList::GetPageIdToTakeNext() const
{
	return FAvaPlaylistPlaybackUtils::GetPageIdToPlayNext(GetPlaylist(), GetPageListReference(), /*bInPreview*/ false, NAME_None);
}

#undef LOCTEXT_NAMESPACE
