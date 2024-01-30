// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Playlist/Pages/Slate/SAvaPageViewRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

void SAvaPageName::Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	PageViewWeak = InPageView;
	PageViewRowWeak = InRow;

	InPageView->GetOnRename().AddSP(this, &SAvaPageName::OnRenameAction);
	
	ChildSlot
	[
		SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
		.Text(InPageView.Get(), &IAvaPageView::GetPageDescription)
		.OnTextCommitted(this, &SAvaPageName::OnTextCommitted)
		.OnVerifyTextChanged(this, &SAvaPageName::OnVerifyTextChanged)
		.OnEnterEditingMode(this, &SAvaPageName::OnEnterEditingMode)
		.OnExitEditingMode(this, &SAvaPageName::OnExitEditingMode)
		.IsSelected(InRow.Get(), &SAvaPageViewRow::IsSelectedExclusively)
		.IsReadOnly(this, &SAvaPageName::IsReadOnly)
	];
}

SAvaPageName::~SAvaPageName()
{
	if (PageViewWeak.IsValid())
	{
		PageViewWeak.Pin()->GetOnRename().RemoveAll(this);
	}
}

void SAvaPageName::OnRenameAction(EAvaPageActionState InRenameAction)
{
	if (InRenameAction == EAvaPageActionState::Requested)
	{
		check(InlineTextBlock.IsValid());
		bIsReadOnly = false;	// Only allow entering edit mode from rename action.
		InlineTextBlock->EnterEditingMode();
	}
}

void SAvaPageName::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo)
{
	if (const FAvaPageViewPtr PageView = PageViewWeak.Pin())
	{
		switch (InCommitInfo)
		{
		case ETextCommit::OnEnter:
			//falls through
				
		case ETextCommit::OnUserMovedFocus:
			RenamePage(InText, PageView);
			PageView->GetOnRename().Broadcast(EAvaPageActionState::Completed);
			break;

		case ETextCommit::Default:
			//falls through
				
		case ETextCommit::OnCleared:
			//falls through
			
		default:
			PageView->GetOnRename().Broadcast(EAvaPageActionState::Cancelled);
			break;
		}
	}
}

bool SAvaPageName::OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return true;
}

void SAvaPageName::OnEnterEditingMode()
{
	bInEditingMode = true;
}

void SAvaPageName::OnExitEditingMode()
{
	bInEditingMode = false;
	bIsReadOnly = true;
}

void SAvaPageName::RenamePage(const FText& InText, const FAvaPageViewPtr& InPageView)
{
	check(InPageView.IsValid());

	const bool bRenameSuccessful = InPageView->RenameFriendlyName(InText);
	if (bRenameSuccessful)
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
	}
}

bool SAvaPageName::IsReadOnly() const
{
	return bIsReadOnly;
}