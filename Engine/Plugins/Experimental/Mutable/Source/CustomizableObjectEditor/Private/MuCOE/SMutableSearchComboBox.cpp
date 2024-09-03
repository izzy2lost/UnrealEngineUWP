// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableSearchComboBox.h"

#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Attribute.h"
#include "DetailLayoutBuilder.h"	// For font: TODO: move to argument


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


void SMutableSearchComboBox::Construct(const FArguments& InArgs)
{
	check(InArgs._ComboBoxStyle);

	ItemStyle = InArgs._ItemStyle;
	MenuRowPadding = InArgs._ComboBoxStyle->MenuRowPadding;

	bAllowAddNewOptions = InArgs._AllowAddNewOptions;

	// Work out which values we should use based on whether we were given an override, or should use the style's version
	OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
	if (InArgs._MenuButtonBrush)
	{
		OurComboButtonStyle.DownArrowImage = *InArgs._MenuButtonBrush;
	}
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;

	this->OnSelectionChanged = InArgs._OnSelectionChanged;
	this->OnGenerateWidget = InArgs._OnGenerateWidget;

	OptionsSource = InArgs._OptionsSource;
	RefreshOptions();

	TAttribute<EVisibility> SearchVisibility = InArgs._SearchVisibility;
	const EVisibility CurrentSearchVisibility = SearchVisibility.Get();

	TSharedRef<SWidget> ComboBoxMenuContent =
		SNew(SBox)
		.MaxDesiredHeight(InArgs._MaxListHeight)
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(this->SearchField, SEditableTextBox)
						.HintText(bAllowAddNewOptions ? LOCTEXT("SearchOrAdd", "Search or add...") : LOCTEXT("Search", "Search..."))
						.OnTextChanged(this, &SMutableSearchComboBox::OnSearchTextChanged)
						.OnTextCommitted(this, &SMutableSearchComboBox::OnSearchTextCommitted)
						.Visibility(SearchVisibility)
				]

				+ SVerticalBox::Slot()
				[
					SAssignNew(this->ComboListView, SComboListType)
						.ListItemsSource(&FilteredOptionsSource)
						.OnGenerateRow(this, &SMutableSearchComboBox::GenerateMenuItemRow)
						.OnSelectionChanged(this, &SMutableSearchComboBox::OnSelectionChanged_Internal)
						.OnKeyDownHandler(this, &SMutableSearchComboBox::OnKeyDownHandler)
						.SelectionMode(ESelectionMode::Single)
				]
		];

	// Set up content
	TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
	if (InArgs._Content.Widget == SNullWidget::NullWidget)
	{
		SAssignNew(ButtonContent, STextBlock);
	}


	SComboButton::Construct(SComboButton::FArguments()
		.ComboButtonStyle(&OurComboButtonStyle)
		.ButtonStyle(OurButtonStyle)
		.Method(InArgs._Method)
		.ButtonContent()
		[
			ButtonContent.ToSharedRef()
		]
		.MenuContent()
		[
			ComboBoxMenuContent
		]
		.ContentPadding(InArgs._ContentPadding)
		.ForegroundColor(InArgs._ForegroundColor)
		.OnMenuOpenChanged(this, &SMutableSearchComboBox::OnMenuOpenChanged)
		.IsFocusable(true)
	);

	if (CurrentSearchVisibility == EVisibility::Visible)
	{
		SetMenuContentWidgetToFocus(SearchField);
	}
	else
	{
		SetMenuContentWidgetToFocus(ComboListView);
	}
}


void SMutableSearchComboBox::RefreshOptions()
{
	// Need to refresh filtered list whenever options change
	FilteredOptionsSource.Reset();

	if (SearchText.IsEmpty())
	{
		for (const TSharedPtr<FString>& Option : *OptionsSource)
		{
			if (Option)
			{
				FFilteredOption Data;
				Data.ActualOption = *Option;
				Data.DisplayOption = *Option;
				FilteredOptionsSource.Add(MakeShared<FFilteredOption>(Data));
			}
		}
	}
	else
	{
		TArray<FString> SearchTokens;
		SearchText.ToString().ParseIntoArrayWS(SearchTokens);

		for (const TSharedPtr<FString>& Option : *OptionsSource)
		{
			bool bAllTokensMatch = true;
			for (const FString& SearchToken : SearchTokens)
			{
				if (Option->Find(SearchToken, ESearchCase::Type::IgnoreCase) == INDEX_NONE)
				{
					bAllTokensMatch = false;
					break;
				}
			}

			if (bAllTokensMatch)
			{
				FFilteredOption Data;
				Data.ActualOption = *Option;
				Data.DisplayOption = *Option;
				FilteredOptionsSource.Add(MakeShared<FFilteredOption>(Data));
			}
		}

		bool bFullMatch = false;
		FString SearchString = SearchText.ToString();
		for (const TSharedPtr<FString>& Option : *OptionsSource)		
		{
			if (*Option == SearchString)
			{
				bFullMatch = true; 
				break;
			}
		}

		if ( bAllowAddNewOptions && !SearchText.IsEmpty() )
		{
			FFilteredOption Data;
			Data.ActualOption = SearchString;
			Data.DisplayOption = FString::Printf(TEXT("Add new (%s)"), *SearchString);
			FilteredOptionsSource.Add(MakeShared<FFilteredOption>(Data));
		}
	}

	if (ComboListView)
	{
		ComboListView->RequestListRefresh();
	}
}


TSharedRef<ITableRow> SMutableSearchComboBox::GenerateMenuItemRow(TSharedPtr<FFilteredOption> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	// TODO
	//if (OnGenerateWidget.IsBound())
	//{
	//	return SNew(SComboRow<TSharedPtr<FString>>, OwnerTable)
	//		.Style(ItemStyle)
	//		.Padding(MenuRowPadding)
	//		[
	//			OnGenerateWidget.Execute(InItem)
	//		];
	//}
	//else
	{
		// Just add the text
		return SNew(SComboRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew(STextBlock)
					.Text(FText::FromString(InItem ? InItem->DisplayOption : FString(TEXT("Invalid item."))))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
}


void SMutableSearchComboBox::OnMenuOpenChanged(bool bOpen)
{
	if (bOpen == false)
	{
		// Set focus back to ComboBox for users focusing the ListView that just closed
		FSlateApplication::Get().ForEachUser([this](FSlateUser& User)
			{
				TSharedRef<SWidget> ThisRef = this->AsShared();
				if (User.IsWidgetInFocusPath(this->ComboListView))
				{
					User.SetFocus(ThisRef);
				}
			});

	}
}


void SMutableSearchComboBox::OnSelectionChanged_Internal(TSharedPtr<FFilteredOption> ProposedSelection, ESelectInfo::Type SelectInfo)
{
	if (!ProposedSelection)
	{
		return;
	}

	// close combo as long as the selection wasn't from navigation
	if (SelectInfo != ESelectInfo::OnNavigation)
	{		
		OnSelectionChanged.ExecuteIfBound(ProposedSelection ? FText::FromString(ProposedSelection->ActualOption) : FText());
		this->SetIsOpen(false);
	}
}


void SMutableSearchComboBox::OnSearchTextChanged(const FText& ChangedText)
{
	SearchText = ChangedText;

	RefreshOptions();
}


void SMutableSearchComboBox::OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if ((InCommitType == ETextCommit::Type::OnEnter) && FilteredOptionsSource.Num() > 0)
	{
		ComboListView->SetSelection(FilteredOptionsSource[0], ESelectInfo::OnKeyPress);
	}
}


FReply SMutableSearchComboBox::OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		// Select the first selected item on hitting enter
		TArray<TSharedPtr<FFilteredOption>> SelectedItems = ComboListView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::OnKeyPress);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
