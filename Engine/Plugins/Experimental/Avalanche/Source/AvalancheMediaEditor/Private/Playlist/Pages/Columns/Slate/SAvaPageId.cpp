// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageId.h"
#include "Playlist/AvalanchePlaylist.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaPageId"

void SAvaPageId::Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView)
{
	PageViewWeak = InPageView;

	InPageView->GetOnRenumber().AddSP(this, &SAvaPageId::OnRenumberAction);
	
	ChildSlot
	[
		SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
		.Text(InPageView.Get(), &IAvaPageView::GetPageIdText)
		.OnTextCommitted(this, &SAvaPageId::OnTextCommitted)
		.OnVerifyTextChanged(this, &SAvaPageId::OnVerifyTextChanged)
		.OnEnterEditingMode(this, &SAvaPageId::OnEnterEditingMode)
		.OnExitEditingMode(this, &SAvaPageId::OnExitEditingMode)
	];
}

SAvaPageId::~SAvaPageId()
{
	if (PageViewWeak.IsValid())
	{
		PageViewWeak.Pin()->GetOnRenumber().RemoveAll(this);
	}
}

void SAvaPageId::OnRenumberAction(EAvaPageActionState InRenumberAction)
{
	if (InRenumberAction == EAvaPageActionState::Requested)
	{
		check(InlineTextBlock.IsValid());
		InlineTextBlock->EnterEditingMode();
	}
}

void SAvaPageId::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo)
{
	const FAvaPageViewPtr PageView = PageViewWeak.Pin();

	if (!PageView)
	{
		return;
	}
	
	switch (InCommitInfo)
	{
	case ETextCommit::OnEnter:
		//falls through
				
	case ETextCommit::OnUserMovedFocus:
		RenumberPageId(InText, PageView);
		PageView->GetOnRenumber().Broadcast(EAvaPageActionState::Completed);
		break;

	case ETextCommit::Default:
		//falls through
				
	case ETextCommit::OnCleared:
		//falls through
			
	default:
		PageView->GetOnRenumber().Broadcast(EAvaPageActionState::Cancelled);
		break;
	}
}

bool SAvaPageId::OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage)
{
	const FString String = InText.ToString();
	
	if (!String.IsNumeric())
	{
		OutErrorMessage = LOCTEXT("NonNumericError", "Text is not Numeric");
		return false;
	}

	UAvalanchePlaylist* const Playlist = PageViewWeak.IsValid()
		? PageViewWeak.Pin()->GetPlaylist()
		: nullptr;

	if (!Playlist)
	{
		OutErrorMessage = LOCTEXT("NoPlaylist", "No Rundown found for Item");
		return false;
	}
	
	const int32 Id = FCString::Atoi(*String);

	if (Playlist->GetPage(Id).IsValidPage())
	{
		OutErrorMessage = LOCTEXT("ExistingPageWithId", "A Page already exists with input Id");
		return false;
	}
	
	return true;
}

void SAvaPageId::OnEnterEditingMode()
{
	bInEditingMode = true;
}

void SAvaPageId::OnExitEditingMode()
{
	bInEditingMode = false;
}

void SAvaPageId::RenumberPageId(const FText& InText, const FAvaPageViewPtr& InPageView)
{
	const FString String = InText.ToString();
	check(String.IsNumeric());

	UAvalanchePlaylist* const Playlist = InPageView->GetPlaylist();
	check(Playlist);
	
	const int32 Id = FCString::Atoi(*String);	

	FScopedTransaction Transaction(LOCTEXT("RenumberPageId", "Renumber Page Id"));
	Playlist->Modify();
	
	const bool bResult = Playlist->RenumberPageId(InPageView->GetPageId(), Id);

	if (!bResult)
	{
		Transaction.Cancel();
	}
}

#undef LOCTEXT_NAMESPACE