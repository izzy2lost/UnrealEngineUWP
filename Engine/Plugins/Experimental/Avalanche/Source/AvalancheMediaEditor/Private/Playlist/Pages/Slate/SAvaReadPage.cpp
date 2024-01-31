// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaReadPage.h"

#include "Playlist/AvaPlaylistEditor.h"
#include "SAvaInstancedPageList.h"
#include "SAvaPageList.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SAvaReadPage"

class SAvaReadPageEditableTextBox : public SEditableTextBox
{
public:
	bool IsKeyRelevant(const FKeyEvent& InKeyEvent) const
	{
		static const TSet<FKey> NumPadKeys { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo
			, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven
			, EKeys::NumPadEight, EKeys::NumPadNine };

		const bool bTextNeedsFocus = EditableText.IsValid() && !EditableText->HasKeyboardFocus();

		return bTextNeedsFocus && NumPadKeys.Contains(InKeyEvent.GetKey());
	}

	bool ProcessPlaylistKeyDown(const FKeyEvent& InKeyEvent)
	{
		// Check if Editable Text needs focus, and that the Key Event is a NumPad Key event
		if (IsKeyRelevant(InKeyEvent))
		{
			SetText(FText::GetEmpty());
			TSharedRef<SWidget> TextWidget = EditableText.ToSharedRef();
			FSlateApplication::Get().SetKeyboardFocus(TextWidget);
			TextWidget->OnKeyDown(TextWidget->GetTickSpaceGeometry(), InKeyEvent);
			return true;
		}
		return false;
	}
};

void SAvaReadPage::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;
	InPlaylistEditor->GetOnPageEvent().AddSP(this, &SAvaReadPage::OnPageEvent);

	const int32 PageId = InPlaylistEditor->GetFirstSelectedPageOnActiveSubListWidget();
	const FText PageIdText = PageId != FAvalanchePage::InvalidPageId
		? FText::AsNumber(PageId, &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions)
		: FText::GetEmpty();

	// Small overestimate of 5 digit width.
	// Only relevant when text is empty (as numbers themselves are formatted for min 5 digits)
	constexpr float ReadPageWidth = 40.f;

	ChildSlot
	.Padding(2.f, 0.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SAssignNew(ReadPageText, SAvaReadPageEditableTextBox)
			.Text(PageIdText)
			.OnVerifyTextChanged(this, &SAvaReadPage::OnVerifyTextChanged)
			.OnTextCommitted(this, &SAvaReadPage::OnTextCommitted)
			.MinDesiredWidth(ReadPageWidth)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Text(LOCTEXT("ReadPageButton", "Read Page"))
			.OnClicked(this, &SAvaReadPage::OnReadPageClicked)
		]
	];
}

SAvaReadPage::~SAvaReadPage()
{
	if (PlaylistEditorWeak.IsValid())
	{
		PlaylistEditorWeak.Pin()->GetOnPageEvent().RemoveAll(this);
	}
}

void SAvaReadPage::OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent)
{
	if (!InSelectedPageIds.IsEmpty())
	{
		ReadPageText->SetText(FText::AsNumber(InSelectedPageIds[0]
			, &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions));
	}
	else
	{
		ReadPageText->SetText(FText::GetEmpty());
	}
}

bool SAvaReadPage::IsKeyRelevant(const FKeyEvent& InKeyEvent) const
{
	if (ReadPageText.IsValid())
	{
		return ReadPageText->IsKeyRelevant(InKeyEvent);
	}
	return false;
}

bool SAvaReadPage::ProcessPlaylistKeyDown(const FKeyEvent& InKeyEvent)
{
	if (ReadPageText.IsValid())
	{
		return ReadPageText->ProcessPlaylistKeyDown(InKeyEvent);
	}
	return false;
}

bool SAvaReadPage::OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage) const
{
	const FString String = InText.ToString();

	if (String.IsEmpty())
	{
		//Allow for the Text to be Empty (i.e. no pages are selected)
		return true;
	}
	
	if (!String.IsNumeric())
	{
		OutErrorMessage = LOCTEXT("NonNumericError", "Text is not Numeric");
		return false;
	}

	UAvalanchePlaylist* const Playlist = PlaylistEditorWeak.IsValid()
		? PlaylistEditorWeak.Pin()->GetPlaylist()
		: nullptr;

	if (!Playlist)
	{
		OutErrorMessage = LOCTEXT("NoPlaylist", "No Rundown found for Item");
		return false;
	}
	
	const int32 Id = FCString::Atoi(*String);

	if (!Playlist->GetPage(Id).IsValidPage())
	{
		OutErrorMessage = LOCTEXT("NoPageFound", "No Page with given Page Id exists");
		return false;
	}
	
	return true;
}

void SAvaReadPage::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();
	check(PlaylistEditor.IsValid());

	TSharedPtr<SAvaInstancedPageList> PageList = PlaylistEditor->GetActiveListWidget();

	if (PageList.IsValid())
	{
		if (InText.IsNumeric())
		{
			const int32 Id = FCString::Atoi(*InText.ToString());

			FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
			const bool bAddtoSelection = KeyState.IsControlDown() || KeyState.IsCommandDown() || KeyState.IsAltDown();

			if (!bAddtoSelection)
			{
				PageList->DeselectPages();
			}

			PageList->SelectPage(Id);
		}
		else if (InText.IsEmpty())
		{
			PageList->DeselectPages();
		}
	}
}

FReply SAvaReadPage::OnReadPageClicked()
{
	OnTextCommitted(ReadPageText->GetText(), ETextCommit::Type::OnEnter);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
