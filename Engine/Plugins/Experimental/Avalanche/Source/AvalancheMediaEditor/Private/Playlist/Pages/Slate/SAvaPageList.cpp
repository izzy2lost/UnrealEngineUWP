// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageList.h"

#include "AvaAssetTags.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor/EditorWidgets/Public/SAssetSearchBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAvaMediaEditorModule.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvaPlaylistEditorUtils.h"
#include "Playlist/AvaPlaylistPlaybackUtils.h"
#include "Playlist/AvaRundownEditorSettings.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "Playlist/Factories/Filters/IAvaPlaylistFilterSuggestionFactory.h"
#include "Playlist/Pages/AvaPlaylistPageContext.h"
#include "Playlist/Pages/AvaPlaylistPageContextMenu.h"
#include "Playlist/Pages/PageViews/AvaPageViewImpl.h"
#include "Playlist/Pages/Slate/SAvaPageViewRow.h"
#include "ScopedTransaction.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SAvaPageList"

namespace UE::AvalanchePlaylist::Private
{
	static const FString PageClipboardPrefix = TEXT("AvalanchePages");
	static const FString PageEntriesName = TEXT("Pages");

	void ExtractAssetSearchFilterTerms(const FText& SearchText, FString* OutFilterKey, FString* OutFilterValue, int32* OutSuggestionInsertionIndex)
	{
		const FString SearchString = SearchText.ToString();

		if (OutFilterKey)
		{
			OutFilterKey->Reset();
		}
		if (OutFilterValue)
		{
			OutFilterValue->Reset();
		}
		if (OutSuggestionInsertionIndex)
		{
			*OutSuggestionInsertionIndex = SearchString.Len();
		}

		// Build the search filter terms so that we can inspect the tokens
		FTextFilterExpressionEvaluator LocalFilter(ETextFilterExpressionEvaluatorMode::Complex);
		LocalFilter.SetFilterText(SearchText);
	
		// Inspect the tokens to see what the last part of the search term was
		// If it was a key->value pair then we'll use that to control what kinds of results we show
		// For anything else we just use the text from the last token as our filter term to allow incremental auto-complete
		const TArray<FExpressionToken>& FilterTokens = LocalFilter.GetFilterExpressionTokens();
		if (FilterTokens.Num() > 0)
		{
			const FExpressionToken& LastToken = FilterTokens.Last();
			// If the last token is a text token, then consider it as a value and walk back to see if we also have a key
			if (LastToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
			{
				if (OutFilterValue)
				{
					*OutFilterValue = LastToken.Context.GetString();
				}
				if (OutSuggestionInsertionIndex)
				{
					*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, LastToken.Context.GetCharacterIndex());
				}
				if (FilterTokens.IsValidIndex(FilterTokens.Num() - 2))
				{
					const FExpressionToken& ComparisonToken = FilterTokens[FilterTokens.Num() - 2];
					if (ComparisonToken.Node.Cast<TextFilterExpressionParser::FEqual>())
					{
						if (FilterTokens.IsValidIndex(FilterTokens.Num() - 3))
						{
							const FExpressionToken& KeyToken = FilterTokens[FilterTokens.Num() - 3];
							if (KeyToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
							{
								if (OutFilterKey)
								{
									*OutFilterKey = KeyToken.Context.GetString();
								}
								if (OutSuggestionInsertionIndex)
								{
									*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, KeyToken.Context.GetCharacterIndex());
								}
							}
						}
					}
				}
			}
			// If the last token is a comparison operator, then walk back and see if we have a key
			else if (LastToken.Node.Cast<TextFilterExpressionParser::FEqual>())
			{
				if (FilterTokens.IsValidIndex(FilterTokens.Num() - 2))
				{
					const FExpressionToken& KeyToken = FilterTokens[FilterTokens.Num() - 2];
					if (KeyToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
					{
						if (OutFilterKey)
						{
							*OutFilterKey = KeyToken.Context.GetString();
						}
						if (OutSuggestionInsertionIndex)
						{
							*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, KeyToken.Context.GetCharacterIndex());
						}
					}
				}
			}
		}
	}
}

void SAvaPageList::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{

}

void SAvaPageList::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor, const FAvaPageListReference& InPageListReference, EAvaPlaylistSearchListType InPageListType)
{
	PlaylistEditorWeak = InPlaylistEditor;
	check(InPlaylistEditor.IsValid());

	PageContextMenu = MakeShared<FAvaPlaylistPageContextMenu>();

	PageListReference = InPageListReference;
	PageListType = InPageListType;
	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	UAvalanchePlaylist* const Playlist = InPlaylistEditor->GetPlaylist();
	check(Playlist);

	InPlaylistEditor->GetOnPageEvent().AddSP(this, &SAvaPageList::OnPageEvent);

	CreateColumns();

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SAssignNew(SearchBar, SHorizontalBox)
				// Search Box
				+ SHorizontalBox::Slot()
				.Padding(4.f, 2.f)
				[
					SAssignNew(AssetSearchBoxPtr, SAssetSearchBox)
					.HintText(LOCTEXT("FilterSearch", "Search..."))
					.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search..."))
					.ShowSearchHistory(true)
					.OnTextChanged(this, &SAvaPageList::OnSearchTextChanged)
					.OnTextCommitted(this, &SAvaPageList::OnSearchTextCommitted)
					.OnAssetSearchBoxSuggestionFilter(this, &SAvaPageList::OnSearchBoxSuggestionFilter)
					.OnAssetSearchBoxSuggestionChosen(this, &SAvaPageList::OnAssetSearchBoxSuggestionChosen)
					.DelayChangeNotificationsWhileTyping(true)
					.Visibility(EVisibility::Visible)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("SAvaPageListSearchBox")))
				]
			]
			// Pages Collection
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.f)
				[
					SAssignNew(PageListView, SListView<FAvaPageViewPtr>)
					.HeaderRow(HeaderRow)
					.ListItemsSource(&PageViews)
					.HandleSpacebarSelection(true)
					.SelectionMode(ESelectionMode::Multi)
					.ClearSelectionOnClick(true)
					.OnGenerateRow(this, &SAvaPageList::GeneratePageTableRow)
					.OnSelectionChanged(this, &SAvaPageList::OnPageSelected)
					.OnContextMenuOpening(this, &SAvaPageList::OnContextMenuOpening)
				]
			]
		];

	Refresh();
}

SAvaPageList::~SAvaPageList()
{
	if (PlaylistEditorWeak.IsValid())
	{
		TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

		if (PlaylistEditor.IsValid())
		{
			PlaylistEditor->GetOnPageEvent().RemoveAll(this);
		}
	}
}

void SAvaPageList::OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent)
{
	using namespace  UE::AvalanchePlaylist;

	switch (InPageEvent)
	{
		case EPageEvent::SelectionRequest:
			OnPageSelectionRequested(InSelectedPageIds);
			break;

		case EPageEvent::RenameRequest:
			OnPageItemActionRequested(InSelectedPageIds, &IAvaPageView::GetOnRename);
			break;

		case EPageEvent::RenumberRequest:
			OnPageItemActionRequested(InSelectedPageIds, &IAvaPageView::GetOnRenumber);
			break;
	}
}

void SAvaPageList::OnPageItemActionRequested(const TArray<int32>& InPageIds, IAvaPageView::FOnPageAction& (IAvaPageView::* InFunc)())
{
	//Only allow one page id to be renamed at a time
	const TSet<int32> PageIdSet(InPageIds);
	for (const FAvaPageViewPtr& PageView : PageViews)
	{
		if (PageIdSet.Contains(PageView->GetPageId()))
		{
			(*PageView.*InFunc)().Broadcast(EAvaPageActionState::Requested);
		}
	}
}

void SAvaPageList::OnPageSelectionRequested(const TArray<int32>& InPageIds)
{
	TArray<FAvaPageViewPtr> DesiredSelectedItems;
	DesiredSelectedItems.Empty(InPageIds.Num());
	for (int32 PageId : InPageIds)
	{
		DesiredSelectedItems.Add(GetPageViewPtr(PageId));
	}
	PageListView->ClearSelection();
	PageListView->SetItemSelection(DesiredSelectedItems, true);
}

TSharedRef<ITableRow> SAvaPageList::GeneratePageTableRow(FAvaPageViewPtr InPageView, const TSharedRef<STableViewBase>& InOwnerTable)
{
	check(InPageView.IsValid());
	return SNew(SAvaPageViewRow, InPageView, SharedThis(this));
}

void SAvaPageList::OnPageSelected(FAvaPageViewPtr InPageView, ESelectInfo::Type InSelectInfo)
{
	if (PageListView.IsValid())
	{
		TArray<FAvaPageViewPtr> SelectedItems;
		PageListView->GetSelectedItems(SelectedItems);
		SelectedPageIds.Empty(SelectedItems.Num());

		for (const FAvaPageViewPtr& PageView : SelectedItems)
		{
			if (PageView.IsValid())
			{
				const int32 SelectedPage = PageView->GetPageId();
				SelectedPageIds.Add(SelectedPage);
			}
		}

		TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = GetPlaylistEditor();

		if (PlaylistEditor.IsValid())
		{
			PlaylistEditor->GetOnPageEvent().Broadcast(SelectedPageIds, UE::AvalanchePlaylist::EPageEvent::SelectionChanged);
		}
	}
}

TSharedPtr<IAvaPageViewColumn> SAvaPageList::FindColumn(FName InColumnName) const
{
	if (const TSharedPtr<IAvaPageViewColumn>* const FoundColumn = Columns.Find(InColumnName))
	{
		return *FoundColumn;
	}
	return nullptr;
}

FAvaPageViewPtr SAvaPageList::GetPageViewPtr(int32 InPageId) const
{
	for (const FAvaPageViewPtr& PageView : PageViews)
	{
		if (PageView->GetPageId() == InPageId)
		{
			return PageView;
		}
	}
	return nullptr;
}

void SAvaPageList::SelectPage(int32 InPageId, bool bInScrollIntoView)
{
	if (PageListView.IsValid())
	{
		for (const FAvaPageViewPtr& PageView : PageViews)
		{
			if (PageView->GetPageId() == InPageId)
			{
				PageListView->SetItemSelection(PageView, true, ESelectInfo::Direct);

				if (bInScrollIntoView)
				{
					PageListView->RequestScrollIntoView(PageView);
				}

				return;
			}
		}
	}
}

void SAvaPageList::SelectPages(const TArray<int32>& InPageIds, bool bInScrollIntoView)
{
	if (PageListView.IsValid())
	{
		for (int32 PageId : InPageIds)
		{
			SelectPage(PageId, bInScrollIntoView);
		}
	}
}

void SAvaPageList::DeselectPage(int32 InPageId)
{
	if (PageListView.IsValid())
	{
		for (const FAvaPageViewPtr& PageView : PageViews)
		{
			if (PageView->GetPageId() == InPageId)
			{
				PageListView->SetItemSelection(PageView, false, ESelectInfo::Direct);
				return;
			}
		}
	}
}

void SAvaPageList::DeselectPages()
{
	if (PageListView.IsValid())
	{
		PageListView->ClearSelection();
	}
}

TArray<int32> SAvaPageList::GetPlayingPageIds() const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		return Playlist->GetPlayingPageIds();
	}
	return TArray<int32>();
}

TSharedPtr<FAvaPlaylistEditor> SAvaPageList::GetPlaylistEditor() const
{
	return PlaylistEditorWeak.Pin();
}

UAvalanchePlaylist* SAvaPageList::GetPlaylist() const
{
	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = GetPlaylistEditor();
	return PlaylistEditor.IsValid() ? PlaylistEditor->GetPlaylist() : nullptr;
}

UAvalanchePlaylist* SAvaPageList::GetValidPlaylist() const
{
	UAvalanchePlaylist* Playlist = GetPlaylist();
	return IsValid(Playlist) ? Playlist : nullptr;
}

void SAvaPageList::BindCommands()
{
	//Generic Commands
	{
		const FGenericCommands& GenericCommands = FGenericCommands::Get();

		CommandList->MapAction(GenericCommands.Rename,
			FExecuteAction::CreateSP(this, &SAvaPageList::RenameSelectedPage),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanRenameSelectedPage));

		CommandList->MapAction(GenericCommands.Delete,
			FExecuteAction::CreateSP(this, &SAvaPageList::RemoveSelectedPages),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanRemoveSelectedPages));

		CommandList->MapAction(GenericCommands.Cut,
			FExecuteAction::CreateSP(this, &SAvaPageList::CutSelectedPages),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanCutSelectedPages));

		CommandList->MapAction(GenericCommands.Copy,
			FExecuteAction::CreateSP(this, &SAvaPageList::CopySelectedPages),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanCopySelectedPages));

		CommandList->MapAction(GenericCommands.Paste,
			FExecuteAction::CreateSP(this, &SAvaPageList::Paste),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanPaste));

		CommandList->MapAction(GenericCommands.Duplicate,
			FExecuteAction::CreateSP(this, &SAvaPageList::DuplicateSelectedPages),
			FCanExecuteAction::CreateSP(this, &SAvaPageList::CanDuplicateSelectedPages));
	}
}

int32 SAvaPageList::GetFirstSelectedPageId() const
{
	return SelectedPageIds.Num() > 0 ? SelectedPageIds[0] : FAvalanchePage::InvalidPageId;
}

TSharedRef<SWidget> SAvaPageList::GetPageListContextMenu()
{
	return PageContextMenu->GeneratePageContextMenuWidget(PlaylistEditorWeak, PageListReference, CommandList);
}

bool SAvaPageList::CanAddPage() const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
	{
		return Playlist->CanAddPage();
	}
	return false;
}

bool SAvaPageList::CanAddTemplate() const
{
	return CanAddPage();
}

bool SAvaPageList::CanCreateInstance() const
{
	return CanAddPage();
}

bool SAvaPageList::CanCopySelectedPages() const
{
	return !SelectedPageIds.IsEmpty() && IsValid(GetPlaylist());
}

void SAvaPageList::CopySelectedPages()
{
	if (!CanCopySelectedPages())
	{
		return;
	}

	UAvalanchePlaylist* Playlist = GetPlaylist();
	if (!IsValid(Playlist))
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("Can't copy from invalid rundown."));
		return;
	}

	FString SerializedString = UE::AvaPlaylistEditor::Utils::SerializePagesToJson(Playlist, SelectedPageIds);
	
	// Add Prefix to quickly identify whether current clipboard is from Avalanche Pages or not
	SerializedString = *FString::Printf(TEXT("%s%s"), *UE::AvalanchePlaylist::Private::PageClipboardPrefix, *SerializedString);

	FPlatformApplicationMisc::ClipboardCopy(*SerializedString);
}

bool SAvaPageList::CanCutSelectedPages() const
{
	return CanCopySelectedPages() && CanRemoveSelectedPages();
}

void SAvaPageList::CutSelectedPages()
{
	if (CanCutSelectedPages())
	{
		CopySelectedPages();
		RemoveSelectedPages();
	}
}

bool SAvaPageList::CanPaste() const
{
	if (!CanAddPage())
	{
		return false;
	}

	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	return PastedText.StartsWith(UE::AvalanchePlaylist::Private::PageClipboardPrefix);
}

void SAvaPageList::Paste()
{
	if (!CanPaste())
	{
		return;
	}

	UAvalanchePlaylist* Playlist = GetPlaylist();
	if (!IsValid(Playlist))
	{
		return;
	}	

	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	PastedText.RightChopInline(UE::AvalanchePlaylist::Private::PageClipboardPrefix.Len());

	const TArray<FAvalanchePage> Pages = UE::AvaPlaylistEditor::Utils::DeserializePagesFromJson(PastedText);
	
	if (Pages.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("AddPages", "Add Pages"));
		Playlist->Modify();

		const TArray<int32> PageIds = AddPastedPages(Pages);

		if (PageIds.IsEmpty())
		{
			Transaction.Cancel();
		}
		else
		{
			DeselectPages();
			SelectPages(PageIds);
		}
	}
}

bool SAvaPageList::CanDuplicateSelectedPages() const
{
	return CanCopySelectedPages() && CanAddPage();
}

void SAvaPageList::DuplicateSelectedPages()
{
	if (CanDuplicateSelectedPages())
	{
		CopySelectedPages();
		Paste();
	}
}

bool SAvaPageList::CanRemoveSelectedPages() const
{
	if (!SelectedPageIds.IsEmpty())
	{
		if (const UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			return Playlist->CanRemovePages(SelectedPageIds);
		}
	}
	return false;
}

void SAvaPageList::RemoveSelectedPages()
{
	if (CanRemoveSelectedPages())
	{
		if (UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			FScopedTransaction Transaction(LOCTEXT("RemoveSelectedPages", "Remove Selected Pages"));
			Playlist->Modify();

			int32 RemovedCount = 0;

			if (PageListReference.Type != EAvaPageListType::View)
			{
				RemovedCount = Playlist->RemovePages(SelectedPageIds);
			}
			else if (Playlist->IsValidSubList(PageListReference))
			{
				RemovedCount = Playlist->RemovePagesFromSubList(PageListReference.SubListIndex, SelectedPageIds);
			}

			if (RemovedCount == 0)
			{
				Transaction.Cancel();
			}
		}
	}
}

bool SAvaPageList::CanRenameSelectedPage() const
{
	if (SelectedPageIds.Num() == 1)
	{
		if (const UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			return Playlist->GetPage(SelectedPageIds[0]).IsValidPage();
		}
	}
	return false;
}

void SAvaPageList::RenameSelectedPage()
{
	if (CanRenameSelectedPage())
	{
		OnPageEvent(SelectedPageIds, UE::AvalanchePlaylist::EPageEvent::RenameRequest);
	}
}

bool SAvaPageList::CanRenumberSelectedPage() const
{
	if (SelectedPageIds.Num() == 1)
	{
		if (const UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			return Playlist->CanRenumberPageId(SelectedPageIds[0]);
		}
	}
	return false;
}

void SAvaPageList::RenumberSelectedPage()
{
	if (CanRenumberSelectedPage())
	{
		OnPageEvent(SelectedPageIds, UE::AvalanchePlaylist::EPageEvent::RenumberRequest);
	}
}

bool SAvaPageList::CanReimportSelectedPage() const
{
	return SelectedPageIds.Num() > 0 && GetPlaylistEditor().IsValid();
}

void SAvaPageList::ReimportSelectedPage() const
{
	if (CanReimportSelectedPage())
	{
		if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = GetPlaylistEditor())
		{
			UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();
			if (IsValid(Playlist))
			{
				// Enforce invalidation of the avalanche managed instance cache for the selected page(s).
				Playlist->InvalidateManagedInstanceCacheForPages(SelectedPageIds);
				Playlist->UpdateAvalancheAssetForPages(SelectedPageIds, true);
			}

			PlaylistEditor->GetOnPageEvent().Broadcast(SelectedPageIds, UE::AvalanchePlaylist::EPageEvent::ReimportRequest);
		}
	}
}

bool SAvaPageList::CanEditSelectedPageSource() const
{
	if (SelectedPageIds.Num() == 1)
	{
		if (UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			const FAvalanchePage& Page = Playlist->GetPage(SelectedPageIds[0]);
			return Page.IsValidPage();
		}
	}
	return false;
}

void SAvaPageList::EditSelectedPageSource()
{
	if (SelectedPageIds.Num() == 1)
	{
		if (UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
		{
			const FAvalanchePage& Page = Playlist->GetPage(SelectedPageIds[0]);
			if (Page.IsValidPage())
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Page.GetAvalancheAssetPath(Playlist));
			}
		}
	}
}

bool SAvaPageList::CanExportSelectedPagesToPlaylist()
{
	return SelectedPageIds.Num() > 0;
}

void SAvaPageList::ExportSelectedPagesToPlaylist()
{
	const UAvalanchePlaylist* SourcePlaylist = GetPlaylist();
	if (!SourcePlaylist || SelectedPageIds.IsEmpty())
	{
		return;
	}

	using namespace UE::AvaPlaylistEditor::Utils;
	if (const TStrongObjectPtr<UAvalanchePlaylist> NewPlaylist = ExportPagesToPlaylist(SourcePlaylist, SelectedPageIds))
	{	
		const UPackage* const PlaylistPackage = SourcePlaylist->GetPackage();
		const FString DefaultPackagePath = FPackageName::GetLongPackagePath(PlaylistPackage->GetName());
		const FString DefaultAssetName = SourcePlaylist->GetName() + TEXT("_Exported");		
		const FString SaveObjectPath = GetSaveAssetAsPath(DefaultPackagePath, DefaultAssetName);

		if (!SaveObjectPath.IsEmpty())
		{
			const FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
			const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
			SaveDuplicatePlaylist(NewPlaylist.Get(), AssetName, PackagePath);
		}
	}
}

bool SAvaPageList::CanExportSelectedPagesToExternalFile(const TCHAR* InType)
{
	return SelectedPageIds.Num() > 0;
}

void SAvaPageList::ExportSelectedPagesToExternalFile(const TCHAR* InType)
{
	const UAvalanchePlaylist* SourcePlaylist = GetPlaylist();
	if (!SourcePlaylist || SelectedPageIds.IsEmpty())
	{
		return;
	}

	using namespace UE::AvaPlaylistEditor::Utils;
	if (const TStrongObjectPtr<UAvalanchePlaylist> NewPlaylist = ExportPagesToPlaylist(SourcePlaylist, SelectedPageIds))
	{
		if (FCString::Stricmp(InType, TEXT("json")) == 0)
		{
			const FString ExportFilename = GetExportFilepath(SourcePlaylist, TEXT("json file"), TEXT("json"));
			if (!ExportFilename.IsEmpty())
			{
				SavePlaylistToJson(NewPlaylist.Get(), *ExportFilename);
			}
		}
		else if (FCString::Stricmp(InType, TEXT("xml")) == 0)
		{
			const FString ExportFilename = GetExportFilepath(SourcePlaylist, TEXT("xml file"), TEXT("xml"));
			if (!ExportFilename.IsEmpty())
			{
				SavePlaylistToXml(NewPlaylist.Get(), *ExportFilename);
			}
		}
		else
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("Export Pages to external file doesn't support type \"%s\"."), InType);
		}
	}
}

bool SAvaPageList::CanPreviewPlaySelectedPage() const
{
	const TArray<int32> PageIds = GetPagesToPreviewIn();
	return !PageIds.IsEmpty();
}

void SAvaPageList::PreviewPlaySelectedPage(bool bInToMark) const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const TArray<int32> PageIds = GetPagesToPreviewIn();
		Playlist->PlayPages(PageIds, bInToMark ? EAvaPlayType::PreviewFromFrame : EAvaPlayType::PreviewFromStart);
	}
}

bool SAvaPageList::CanPreviewStopSelectedPage(bool bInForce) const
{
	const TArray<int32> PageIds = GetPagesToPreviewOut(bInForce);
	return !PageIds.IsEmpty();
}

void SAvaPageList::PreviewStopSelectedPage(bool bInForce) const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const EAvaPlaylistPageStopOptions StopOptions = bInForce ? EAvaPlaylistPageStopOptions::ForceNoTransition : EAvaPlaylistPageStopOptions::Default;
		const TArray<int32> PageIds = GetPagesToPreviewOut(bInForce);
		Playlist->StopPages(PageIds, StopOptions, true);
	}
}

bool SAvaPageList::CanPreviewContinueSelectedPage() const
{
	const TArray<int32> PageIds = GetPagesToPreviewContinue();
	return !PageIds.IsEmpty();
}

void SAvaPageList::PreviewContinueSelectedPage() const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const TArray<int32> PageIds = GetPagesToPreviewContinue();
		for (const int32 PageId : PageIds)
		{
			Playlist->ContinuePage(PageId, true);
		}
	}
}

bool SAvaPageList::CanPreviewPlayNextPage() const
{
	const int32 PageIdToPreviewNext = GetPageIdToPreviewNext();
	return IsPageIdValid(PageIdToPreviewNext);
}

void SAvaPageList::PreviewPlayNextPage()
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		const int32 PageIdToPreviewNext = GetPageIdToPreviewNext();
		if (IsPageIdValid(PageIdToPreviewNext))
		{
			Playlist->PlayPage(PageIdToPreviewNext, EAvaPlayType::PreviewFromStart);
			DeselectPages();
			SelectPages({PageIdToPreviewNext});
		}
	}
}

bool SAvaPageList::CanTakeToProgram() const
{
	const TArray<int32> PageIds = GetPagesToTakeToProgram();
	return !PageIds.IsEmpty();
}

void SAvaPageList::TakeToProgram() const
{
	if (UAvalanchePlaylist* Playlist = GetPlaylist(); IsValid(Playlist))
	{
		const TArray<int32> PageIds = GetPagesToTakeToProgram();
		Playlist->PlayPages(PageIds, EAvaPlayType::PlayFromStart);
	}
}

namespace UE::AvaPageList::Private
{
	static bool IsAvaAsset(const FAssetData& InAssetData, bool bInLogInfo)
	{
		static const FName& AvalancheSceneAssetTag = UE::Ava::AssetTags::AvalancheScene;
		static const FString& AssetTagValueEnabled = UE::Ava::AssetTags::Values::Enabled;

		const UClass* AssetClass = InAssetData.GetClass(EResolveClass::Yes);
		if (!IsValid(AssetClass))
		{
			return false;
		}
		
		const EAvalancheAssetType AssetType = FAvaSoftAssetPath::GetAssetTypeFromClass(AssetClass, true);
		if (AssetType == EAvalancheAssetType::Unknown)
		{
			return false;
		}
		
		// If the asset is a level, we check if it has the avalanche scene tag.
		if (AssetType == EAvalancheAssetType::World)
		{
			const FAssetTagValueRef AvalancheSceneTag = InAssetData.TagsAndValues.FindTag(AvalancheSceneAssetTag);
			
			if (!AvalancheSceneTag.IsSet())
			{
				if (bInLogInfo)
				{
					UE_LOG(LogAvaMediaEditor, Display,
						TEXT("Level Asset \"%s\" is not an Motion Design Scene (Asset Tag not found)."),
						*InAssetData.GetSoftObjectPath().ToString());
				}
				return false;
			}
			
			if (!AvalancheSceneTag.Equals(AssetTagValueEnabled))
			{
				if (bInLogInfo)
				{
					UE_LOG(LogAvaMediaEditor, Display,
						TEXT("Level Asset \"%s\" is an Motion Design Scene but not enabled."),
						*InAssetData.GetSoftObjectPath().ToString());
				}
				return false;						
			}
		}
		return true;
	}
}

bool SAvaPageList::IsAssetDropSupported(const FAssetData& InAsset, const FSoftObjectPath& InDestinationPlaylistPath)
{
	using namespace UE::AvaPageList::Private;
	if (IsAvaAsset(InAsset, false))
	{
		return true;
	}
	if (InAsset.IsInstanceOf<UAvalanchePlaylist>(EResolveClass::Yes))
	{
		// Don't drop a playlist in itself.
		if (InAsset.GetSoftObjectPath() != InDestinationPlaylistPath)
		{
			return true;
		}
	}
	return false;
}

TArray<FSoftObjectPath> SAvaPageList::FilterAvaAssetPaths(const TArray<FAssetData>& InAssets)
{
	using namespace UE::AvaPageList::Private;
	TArray<FSoftObjectPath> AvaAssets;
	AvaAssets.Reserve(InAssets.Num());
	
	for (const FAssetData& AssetData : InAssets)
	{
		if (IsAvaAsset(AssetData, true))
		{
			AvaAssets.Add(AssetData.GetSoftObjectPath());
		}
	}
	return AvaAssets;
}

TArray<FSoftObjectPath> SAvaPageList::FilterPlaylistPaths(const TArray<FAssetData>& InAssets, const FSoftObjectPath& InDestinationPlaylistPath)
{
	TArray<FSoftObjectPath> Playlists;
	Playlists.Reserve(InAssets.Num());

	for (const FAssetData& AssetData : InAssets)
	{
		if (AssetData.IsInstanceOf<UAvalanchePlaylist>(EResolveClass::Yes))
		{
			FSoftObjectPath PlaylistPath = AssetData.GetSoftObjectPath();
			// Don't drop a playlist in itself.
			if (PlaylistPath != InDestinationPlaylistPath)
			{
				Playlists.Add(MoveTemp(PlaylistPath));
			}
		}
	}
	
	return Playlists;
}

FAvaPageInsertPosition SAvaPageList::MakeInsertPosition(EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	FAvaPageInsertPosition InsertPosition;
	InsertPosition.AdjacentId = InItem.IsValid() ? InItem->GetPageId() : FAvalanchePage::InvalidPageId;
	InsertPosition.bAddBelow = InDropZone != EItemDropZone::AboveItem;
	return InsertPosition;
}

bool SAvaPageList::CanHandleDragObjects(const FDragDropEvent& InDragDropEvent) const
{
	if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		const FSoftObjectPath DestinationPlaylistPath(GetPlaylist());
		for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
		{
			if (IsAssetDropSupported(AssetData, DestinationPlaylistPath))
			{
				return true;
			}
		}
		return false;
	}

	if (const TSharedPtr<FAvaPageViewRowDragDropOp> PageDragDropOp = InDragDropEvent.GetOperationAs<FAvaPageViewRowDragDropOp>())
	{
		return true;
	}

	if (const TSharedPtr<FExternalDragOperation> ExternalDragDropOp = InDragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (ExternalDragDropOp->HasFiles())
		{
			for (const FString& File : ExternalDragDropOp->GetFiles())
			{
				if (UE::AvaPlaylistEditor::Utils::CanLoadPlaylistFromFile(*File))
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool SAvaPageList::HandleDropEvent(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, const FAvaPageViewPtr& InItem)
{
	if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		const TArray<FSoftObjectPath> AvaAssets = FilterAvaAssetPaths(AssetDragDropOp->GetAssets());
		const TArray<FSoftObjectPath> Playlists = FilterPlaylistPaths(AssetDragDropOp->GetAssets(), FSoftObjectPath(GetPlaylist()));

		FScopedTransaction DropTransaction(LOCTEXT("DropTransaction", "Drop Motion Design Assets onto Page List"));

		bool bIsHandled = false;
		if (!Playlists.IsEmpty())
		{
			bIsHandled |= HandleDropPlaylists(Playlists, InDropZone, InItem);
		}
		if (!AvaAssets.IsEmpty())
		{
			bIsHandled |= HandleDropAvalancheAssets(AvaAssets, InDropZone, InItem);
		}
		return bIsHandled;
	}

	if (const TSharedPtr<FAvaPageViewRowDragDropOp> PageDragDropOp = InDragDropEvent.GetOperationAs<FAvaPageViewRowDragDropOp>())
	{
		if (const TSharedPtr<SAvaPageList> FromPageList = PageDragDropOp->GetPageList())
		{
			return HandleDropPageIds(FromPageList->GetPageListReference(), PageDragDropOp->GetDraggedIds(), InDropZone, InItem);
		}
		return false;
	}

	if (const TSharedPtr<FExternalDragOperation> ExternalDragDropOp = InDragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		FScopedTransaction DropTransaction(LOCTEXT("DropTransaction", "Drop External Asset onto Page List"));

		return ExternalDragDropOp->HasFiles() && HandleDropExternalFiles(ExternalDragDropOp->GetFiles(), InDropZone, InItem);
	}

	return false;
}

FReply SAvaPageList::OnDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

FReply SAvaPageList::OnDragOver(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent)
{
	if (CanHandleDragObjects(InDragDropEvent))
	{
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SAvaPageList::OnDrop(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent)
{
	if (HandleDropEvent(InDragDropEvent, EItemDropZone::OntoItem, nullptr))
	{
		return FReply::Handled();
	}
		
	return FReply::Unhandled();
}

FReply SAvaPageList::OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SAvaPageList::OnAssetSearchBoxSuggestionChosen(const FText& InSearchText, const FString& InSuggestion)
{
	int32 SuggestionInsertionIndex = 0;
	UE::AvalanchePlaylist::Private::ExtractAssetSearchFilterTerms(InSearchText, nullptr, nullptr, &SuggestionInsertionIndex);

	FString SearchString = InSearchText.ToString();
	SearchString.RemoveAt(SuggestionInsertionIndex, SearchString.Len() - SuggestionInsertionIndex, false);
	SearchString.Append(InSuggestion);
	return FText::FromString(SearchString);
}

void SAvaPageList::OnSearchTextChanged(const FText& FilterText)
{
	if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		PlaylistEditor->SetSearchText(FilterText, PageListType);
	}
}

void SAvaPageList::OnSearchTextCommitted(const FText& FilterText, ETextCommit::Type CommitType)
{
	if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		PlaylistEditor->SetSearchText(FilterText, PageListType);
	}
}

void SAvaPageList::OnSearchBoxSuggestionFilter(const FText& InSearchText, TArray<FAssetSearchBoxSuggestion>& OutPossibleSuggestions, FText& OutSuggestionHighlightText)
{
	// We don't bind the suggestion list, so this list should be empty as we populate it here based on the search term
	check(OutPossibleSuggestions.IsEmpty());

	FString FilterKey;
	FString FilterValue;
	UE::AvalanchePlaylist::Private::ExtractAssetSearchFilterTerms(InSearchText, &FilterKey, &FilterValue, nullptr);

	const IAvaMediaEditorModule& AvaMediaEditorModule = IAvaMediaEditorModule::Get();

	TSet<FString> FilterCache;
	const TSharedRef<FAvaPlaylistFilterSuggestionPayload> SimplePayload =
				MakeShared<FAvaPlaylistFilterSuggestionPayload>(FAvaPlaylistFilterSuggestionPayload{ OutPossibleSuggestions, FilterValue, FAvalanchePage::InvalidPageId, nullptr, FilterCache });
	for (const TSharedPtr<IAvaPlaylistFilterSuggestionFactory>& SimpleSuggestion : AvaMediaEditorModule.GetSimpleSuggestions(PageListType))
	{
		SimpleSuggestion->AddSuggestion(SimplePayload);
	}

	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		if (const UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist())
		{
			const TSharedRef<FAvaPlaylistFilterSuggestionPayload> ComplexPayload =
				MakeShared<FAvaPlaylistFilterSuggestionPayload>(FAvaPlaylistFilterSuggestionPayload{ OutPossibleSuggestions, FilterValue, FAvalanchePage::InvalidPageId, Playlist, FilterCache });

			for (const FAvalanchePage& Page : GetPagesByType(PageListType))
			{
				for (const TSharedPtr<IAvaPlaylistFilterSuggestionFactory>& ComplexSuggestion : AvaMediaEditorModule.GetComplexSuggestions(PageListType))
				{
					ComplexPayload->ItemPageId = Page.GetPageId();
					ComplexSuggestion->AddSuggestion(ComplexPayload);
				}
			}
		}
	}
	OutSuggestionHighlightText = FText::FromString(FilterValue);
}

TArray<FAvalanchePage> SAvaPageList::GetPagesByType(EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	TArray<FAvalanchePage> Pages = TArray<FAvalanchePage>();

	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		if (const UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist())
		{
			switch (InPlaylistSearchListType)
			{
			case EAvaPlaylistSearchListType::Template:
				Pages = Playlist->GetTemplatePages().Pages;
				break;

			case EAvaPlaylistSearchListType::Instanced:
				Pages = Playlist->GetInstancedPages().Pages;
				break;

			case EAvaPlaylistSearchListType::None:
			default:
				break;
			}
		}
	}

	return Pages;
}

TArray<int32> SAvaPageList::FilterSelectedPages(FFilterPageFunctionRef InFilterPageFunction) const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		if (!SelectedPageIds.IsEmpty())
		{
			return InFilterPageFunction(Playlist, SelectedPageIds);
		}
	}
	return {};
}
	
TArray<int32> SAvaPageList::FilterPreviewingPages(FFilterPageFunctionRef InFilterPageFunction) const
{
	if (const UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		return InFilterPageFunction(Playlist, Playlist->GetPreviewingPageIds());
	}
	return {};
}
	
TArray<int32> SAvaPageList::FilterSelectedOrPreviewingPages(FFilterPageFunctionRef InFilterPageFunction, const bool bInAllowFallback) const
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
		return InFilterPageFunction(Playlist, Playlist->GetPreviewingPageIds());
	}
	return {};
}
	
TArray<int32> SAvaPageList::FilterPageSetForPreview(FFilterPageFunctionRef InFilterPageFunction, const EAvaRundownPageSet InPageSet) const
{
	switch (InPageSet)
	{
	case EAvaRundownPageSet::SelectedOrPlayingStrict:
		return FilterSelectedOrPreviewingPages(InFilterPageFunction, /*bAllowFallback*/ false);
	case EAvaRundownPageSet::SelectedOrPlaying:
		return FilterSelectedOrPreviewingPages(InFilterPageFunction, /*bAllowFallback*/ true);
	case EAvaRundownPageSet::Selected:
		return FilterSelectedPages(InFilterPageFunction);
	case EAvaRundownPageSet::Playing:
		return FilterPreviewingPages(InFilterPageFunction);
	default:
		return FilterSelectedOrPreviewingPages(InFilterPageFunction, /*bAllowFallback*/ true);
	}
}

TArray<int32> SAvaPageList::GetPagesToPreviewIn() const
{
	auto KeepPagesToPreviewIn = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanPlayPage(PageId, true))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};
	return FilterSelectedPages(KeepPagesToPreviewIn);
}

TArray<int32> SAvaPageList::GetPagesToPreviewOut(bool bInForce) const
{
	EAvaPlaylistPageStopOptions StopOptions = bInForce ? EAvaPlaylistPageStopOptions::ForceNoTransition : EAvaPlaylistPageStopOptions::Default;
	auto KeepPagesToPreviewStop = [StopOptions](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanStopPage(PageId, StopOptions, true))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};

	const UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::Get();
	const EAvaRundownPageSet PageSet = RundownEditorSettings ? RundownEditorSettings->PreviewOutActionPageSet : EAvaRundownPageSet::SelectedOrPlaying;
	return FilterPageSetForPreview(KeepPagesToPreviewStop, PageSet);
}

TArray<int32> SAvaPageList::GetPagesToPreviewContinue() const
{
	auto KeepPagesToPreviewContinue = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			if (InPlaylist->CanContinuePage(PageId, true))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};

	const UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::Get();
	const EAvaRundownPageSet PageSet = RundownEditorSettings ? RundownEditorSettings->PreviewContinueActionPageSet : EAvaRundownPageSet::SelectedOrPlaying;
	return FilterPageSetForPreview(KeepPagesToPreviewContinue, PageSet);
}

TArray<int32> SAvaPageList::GetPagesToTakeToProgram() const
{
	// Remark: taking to program will take all previewing pages that are not already playing
	// regardless of what is selected.
	return FAvaPlaylistPlaybackUtils::GetPagesToTakeToProgram(GetPlaylist(), {});
}

int32 SAvaPageList::GetPageIdToPreviewNext() const
{
	return FAvaPlaylistPlaybackUtils::GetPageIdToPlayNext(
		GetPlaylist(), GetPageListReference(), /*bInPreview*/ true, UAvalanchePlaylist::GetDefaultPreviewChannelName());
}

#undef LOCTEXT_NAMESPACE
