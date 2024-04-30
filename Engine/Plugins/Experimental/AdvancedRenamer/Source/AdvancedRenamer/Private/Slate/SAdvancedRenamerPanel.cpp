// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SAdvancedRenamerPanel.h"
#include "AdvancedRenamerStyle.h"
#include "EngineAnalytics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAdvancedRenamer.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SAdvancedRenamerPanel"

namespace UE::AdvancedRenamer::Private
{
	const FVector2D WindowSize = {600.f, 500.f};
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	const FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	constexpr float LeftBlockWidth = 304.f;
	const FVector2D TitleSize = {150.f, 20.f};
	constexpr float TitleOffsetY = 25.f;
	const FVector2D CheckboxOffset = {0.f, 2.f};
	const FVector2D CheckboxSize = {18.f, 18.f};
	const FVector2D RadioOffset = {1.f, 2.f};
	constexpr float LabelStart = 25.f;
	constexpr float LabelOffsetY = 3.f;
	const FVector2D LabelSize = {125.f, 18.f};
	constexpr float EntryStart = 145.f;
	const FVector2D EntrySize = {155.f, 21.f};
	constexpr float LineHeight = 26.f;
	const FVector2D SeparatorSize = {32.f, 21.f};
	const FVector2D SpinSize1 = {32.f, 21.f};
	const FVector2D SpinSize2 = {39.f, 21.f};
	const FVector2D SpinSize3 = {46.f, 21.f};
	constexpr float RightBlockOffsetX = 314.f;
	const FVector2D RightBlockSize = {281.f, 490.f};
	const FVector2D ListViewSize = {277.f, 431.f};
	constexpr float ListLineHeight = 15.f;
	constexpr float ApplyButtonHeight = 25.f;
	static FName OriginalNameColumnName = "OriginalName";
	static FName NewNameColumnName = "NewName";
}

class SAdvancedRenamerPreviewListRow : public SMultiColumnTableRow<TSharedPtr<FAdvancedRenamerPreview>>
{
public:
	SLATE_BEGIN_ARGS(SAdvancedRenamerPreviewListRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SAdvancedRenamerPanel> InRenamePanel,
		const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FAdvancedRenamerPreview> InRowItem)
	{
		PanelWeak = InRenamePanel;
		ItemWeak = InRowItem;

		SMultiColumnTableRow<TSharedPtr<FAdvancedRenamerPreview>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
		SetBorderImage(TAttribute<const FSlateBrush*>(this, &SAdvancedRenamerPreviewListRow::GetBorder));
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace UE::AdvancedRenamer::Private;

		if (InColumnName != OriginalNameColumnName
			&& InColumnName != NewNameColumnName)
		{
			return SNullWidget::NullWidget;
		}

		TSharedPtr<SAdvancedRenamerPanel> Panel = PanelWeak.Pin();

		if (!Panel.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		TSharedPtr<FAdvancedRenamerPreview> Item = ItemWeak.Pin();

		if (!Item.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
			.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
			.Font(RegularFont)
			.Margin(FMargin(5.f, 0.f, 5.f, 0.f));

		if (InColumnName == OriginalNameColumnName)
		{
			TextBlock->SetText(FText::FromString(Item->OriginalName));
			Panel->MinDesiredOriginalNameWidth = FMath::Max(Panel->MinDesiredOriginalNameWidth, TextBlock->ComputeDesiredSize(1.f).X);
		}
		else if (InColumnName == NewNameColumnName)
		{
			TextBlock->SetText(FText::FromString(Item->NewName));
			Panel->MinDesiredNewNameWidth = FMath::Max(Panel->MinDesiredOriginalNameWidth, TextBlock->ComputeDesiredSize(1.f).X);
		}

		return TextBlock;
	}

protected:
	TWeakPtr<SAdvancedRenamerPanel> PanelWeak;
	TWeakPtr<FAdvancedRenamerPreview> ItemWeak;
};

void SAdvancedRenamerPanel::Construct(const FArguments& InArgs, const TSharedRef<IAdvancedRenamer>& InRenamer)
{
	Renamer = InRenamer;

	bRemovePrefixSeparator = false;
	bRemovePrefixNumChars = false;
	bRemoveSuffixSeparator = false;
	bRemoveSuffixNumChars = false;

	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SAdvancedRenamerPanel::RemoveSelectedObjects),
		FCanExecuteAction()
	);

	using namespace UE::AdvancedRenamer::Private;

	TSharedRef<SCanvas> Canvas = SNew(SCanvas)
		+ SCanvas::Slot()
		.Position(FVector2D(0.f, 0.f))
		.Size(WindowSize)
		[
			SNew(SColorBlock)
			.Color(FStyleColors::Background.GetSpecifiedColor())
		];

	CreateLeftPane(Canvas);
	CreateRightPane(Canvas);

	ChildSlot
	[
		Canvas
	];

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.AdvancedRenamer.Opened"));
	}
}

void SAdvancedRenamerPanel::CreateLeftPane(const TSharedRef<SCanvas>& InCanvas)
{	
	using namespace UE::AdvancedRenamer::Private;

	InCanvas->AddSlot()
		.Position(FVector2D(5.f, 5.f))
		.Size(FVector2D(LeftBlockWidth, 50.f))
		[
			CreateBaseName()
		];

	InCanvas->AddSlot()
		.Position(FVector2D(5.f, 60.f))
		.Size(FVector2D(LeftBlockWidth, 102.f))
		[
			CreatePrefix()
		];

	InCanvas->AddSlot()
		.Position(FVector2D(5.f, 167.f))
		.Size(FVector2D(LeftBlockWidth, 154.f))
		[
			CreateSuffix()
		];

	InCanvas->AddSlot()
		.Position(FVector2D(5.f, 327.f))
		.Size(FVector2D(LeftBlockWidth, 168.f))
		[
			CreateSearchAndReplace()
		];
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateBaseName()
{
	using namespace UE::AdvancedRenamer::Private;

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("BaseNameTitle", "Name"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SetBaseName", "Set Base Name"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(BaseNameTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("BaseNameHint", "Base name"))
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnBaseNameChanged)
			]
		];
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreatePrefix()
{
	using namespace UE::AdvancedRenamer::Private;

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("PrefixTitle", "Prefix"))
			]
			//
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("AddPrefix", "Add Prefix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(PrefixTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("PrefixHint", "Prefix"))
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnPrefixChanged)
			]

			// Remove prefix
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(PrefixRemoveCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsPrefixRemoveChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PrefixRemove", "Remove Prefix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight))
			.Size(SeparatorSize)
			[
				SAssignNew(PrefixSeparatorTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsPrefixRemoveSeparatorEnabled)
				.Text(LOCTEXT("Underscore", "_"))
				.OnVerifyTextChanged(this, &SAdvancedRenamerPanel::OnPrefixSeparatorVerifyTextChanged)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnPrefixSeparatorChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Separator", "(separator)"))
			]

			// Remove first
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 2.f) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(PrefixRemoveCharactersCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsPrefixRemoveCharactersChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCharactersCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PrefixRemoveChars", "Remove First"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight * 2.f))
			.Size(SpinSize1)
			[

				SAssignNew(PrefixRemoveCharactersSpinBox, SSpinBox<uint8>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(9)
				.Value(1)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsPrefixRemoveNumCharsEnabled)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCharactersChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Characters", "character(s)"))
			]
		];
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateSuffix()
{
	using namespace UE::AdvancedRenamer::Private;

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("SuffixTitle", "Suffix"))
			]

			// Suffix name
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("AddSuffix", "Add Suffix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(SuffixTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("SuffixHint", "Suffix"))
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSuffixChanged)
			]

			// Remove Suffix
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemove", "Remove Suffix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight))
			.Size(SeparatorSize)
			[
				SAssignNew(SuffixSeparatorTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsSuffixRemoveSeparatorEnabled)
				.Text(LOCTEXT("Underscore", "_"))
				.OnVerifyTextChanged(this, &SAdvancedRenamerPanel::OnSuffixSeparatorVerifyTextChanged)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSuffixSeparatorChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Separator", "(separator)"))
			]

			// Remove first
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 2.f) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveCharactersCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveCharactersChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCharactersCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemoveChars", "Remove Last"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight * 2.f))
			.Size(SpinSize1)
			[
				SAssignNew(SuffixRemoveCharactersSpinBox, SSpinBox<uint8>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(9)
				.Value(1)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsSuffixRemoveNumCharsEnabled)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCharactersChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Characters", "character(s)"))
			]

			// Remove Number
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 3.f) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveNumberCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveNumberChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveNumberCheckBoxChanged)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsSuffixRemoveNumberCheckBoxEnabled)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 3.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemoveNumber", "Remove Number"))
			]

			// Number
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 4.f) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixNumberCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixNumberChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixNumber", "Add Number"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart , TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Start", "Start"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f, TitleOffsetY + LineHeight * 4.f))
			.Size(SpinSize3)
			[

				SAssignNew(SuffixNumberStartSpinBox, SSpinBox<int32>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(0)
				.MaxValue(999)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberStartChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f + SpinSize3.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Step", "Step"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f + SpinSize3.X + 36.f, TitleOffsetY + LineHeight * 4.f))
			.Size(SpinSize2)
			[

				SAssignNew(SuffixNumberStepSpinBox, SSpinBox<int32>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(99)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberStepChanged)
			]
		];
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateSearchAndReplace()
{
	using namespace UE::AdvancedRenamer::Private;

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("SearchReplaceTitle", "Search and Replace"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplacePlainTextCheckbox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplacePlainTextChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplacePlainTextCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PlainText", "Plain Text"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(92.f, TitleOffsetY) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplaceRegexCheckbox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplaceRegexChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceRegexCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(92.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Regex", "Regex"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(200.f, TitleOffsetY) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplaceIgnoreCaseCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplaceIgnoreCaseChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceIgnoreCaseCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(200.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("IgnoreCase", "Ignore Case"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY + LineHeight))
			.Size(FVector2D(LeftBlockWidth - 4.f, 54.f))
			[
				SAssignNew(SearchReplaceSearchTextBox, SMultiLineEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.AllowMultiLine(false)
				.AutoWrapText(true)
				.HintText(LOCTEXT("RegeSearchHint", "Search"))
				.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceSearchTextChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY + LineHeight + 59.f))
			.Size(FVector2D(LeftBlockWidth - 4.f, 54.f))
			[
				SAssignNew(SearchReplaceReplaceTextBox, SMultiLineEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.AllowMultiLine(false)
				.AutoWrapText(true)
				.HintText(LOCTEXT("RegeReplaceHint", "Replace"))
				.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceReplaceTextChanged)
			]
		];
}

void SAdvancedRenamerPanel::CreateRightPane(const TSharedRef<SCanvas>& InCanvas)
{
	using namespace UE::AdvancedRenamer::Private;

	InCanvas->AddSlot()
		.Position(FVector2D(RightBlockOffsetX, 5.f))
		.Size(this, &SAdvancedRenamerPanel::GetRightPaneSize)
		[
			CreateRenamePreview()
		];
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateRenamePreview()
{
	using namespace UE::AdvancedRenamer::Private;

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.Content()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(0.f, LabelOffsetY, 0.f, 0.f)
			.AutoHeight()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("ObjectRenamePreviewTitle", "Rename Preview"))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				SAssignNew(RenamePreviewListBox, SBox)
				.Content()
				[
					SAssignNew(RenamePreviewList, SListView<TSharedPtr<FAdvancedRenamerPreview>>)
					.ItemHeight(ListLineHeight)
					.ListItemsSource(&Renamer->GetPreviews())
					.OnGenerateRow(this, &SAdvancedRenamerPanel::OnGenerateRowForList)
					.HeaderRow(
						SAssignNew(RenamePreviewListHeaderRow, SHeaderRow)
						+ SHeaderRow::Column(OriginalNameColumnName)
						.DefaultLabel(LOCTEXT("From", "From"))
						.FillWidth(0.5f)
						+ SHeaderRow::Column(NewNameColumnName)
						.DefaultLabel(LOCTEXT("To", "To"))
						.FillWidth(0.5f)
					)
					.OnKeyDownHandler(this, &SAdvancedRenamerPanel::OnListViewKeyDown)
					.OnContextMenuOpening(this, &SAdvancedRenamerPanel::GenerateListViewContextMenu)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				SAssignNew(ApplyButton, SButton)
				.ButtonStyle(FAdvancedRenamerStyle::Get(), "AdvancedRenamer.Style.DarkButton")
				.ToolTipText(LOCTEXT("ApplyRenameToolTip", "Renames all actors."))
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.IsEnabled(this, &SAdvancedRenamerPanel::IsApplyButtonEnabled)
				.OnClicked(this, &SAdvancedRenamerPanel::OnApplyButtonClicked)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
					.Font(RegularFont)
					.Text(LOCTEXT("ApplyRename", "Apply"))
				]
			]
		];
}

bool SAdvancedRenamerPanel::CloseWindow()
{
	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));

	if (CurrentWindow.IsValid())
	{
		CurrentWindow->RequestDestroyWindow();
		return true;
	}

	return false;
}

void SAdvancedRenamerPanel::RefreshListView(const double InCurrentTime)
{
	const int32 CurrentCount = Renamer->Num();

	Renamer->UpdatePreviews();

	if (CurrentCount != Renamer->Num())
	{
		RenamePreviewList->RequestListRefresh();
	}

	MinDesiredOriginalNameWidth = 0.f;
	MinDesiredNewNameWidth = 0.f;
	RenamePreviewList->RebuildList();

	ListLastUpdateTime = InCurrentTime;
}

void SAdvancedRenamerPanel::UpdateRequiredListWidth()
{
	if (!RenamePreviewListBox.IsValid() || !RenamePreviewListHeaderRow.IsValid())
	{
		return;
	}

	if (MinDesiredNewNameWidth == 0.f)
	{
		return;
	}

	using namespace UE::AdvancedRenamer::Private;

	const float ActualWidth = FMath::Max(GetTickSpaceGeometry().GetLocalSize().X, MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f);

	RenamePreviewListHeaderRow->SetColumnWidth(
		OriginalNameColumnName, 
		(MinDesiredOriginalNameWidth + 10.f) / (MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f) * ActualWidth
	);

	RenamePreviewListHeaderRow->SetColumnWidth(
		NewNameColumnName, 
		(MinDesiredNewNameWidth + 10.f) / (MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f) * ActualWidth
	);
}

void SAdvancedRenamerPanel::RemoveSelectedObjects()
{
	if (!RenamePreviewList.IsValid() || RenamePreviewList->GetNumItemsSelected() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FAdvancedRenamerPreview>> SelectedItems = RenamePreviewList->GetSelectedItems();

	if (SelectedItems.Num() == 0)
	{
		return;
	}

	bool bMadeChange = false;

	for (int32 SelectedIndex = 0; SelectedIndex < SelectedItems.Num(); ++SelectedIndex)
	{
		if (!SelectedItems[SelectedIndex].IsValid())
		{
			continue;
		}

		const int32 PreviewIndex = Renamer->FindHash(SelectedItems[SelectedIndex]->Hash);

		if (PreviewIndex == INDEX_NONE)
		{
			continue;
		}

		if (Renamer->RemoveIndex(PreviewIndex))
		{
			bMadeChange = true;
		}
	}

	SelectedItems.Empty();

	if (bMadeChange)
	{
		if (Renamer->Num() == 0)
		{
			if (CloseWindow())
			{
				return;
			}
		}

		RenamePreviewList->RequestListRefresh();
	}
}

void SAdvancedRenamerPanel::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (ListLastUpdateTime == 0)
	{
		ListLastUpdateTime = InCurrentTime;
	}
	else if (Renamer->IsDirty() && InCurrentTime >= (ListLastUpdateTime + MinUpdateFrequency))
	{
		RefreshListView(InCurrentTime);
	}
	else
	{
		UpdateRequiredListWidth();
	}
}

void SAdvancedRenamerPanel::OnBaseNameChanged(const FText& InNewText)
{
	Renamer->GetOptions().BaseName = InNewText.ToString();
	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnPrefixChanged(const FText& InNewText)
{
	Renamer->GetOptions().AddPrefix = InNewText.ToString();
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsPrefixRemoveChecked() const
{
	return bRemovePrefixSeparator ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnPrefixRemoveCheckBoxChanged(ECheckBoxState InNewState)
{
	bRemovePrefixSeparator = InNewState == ECheckBoxState::Checked;

	if (bRemovePrefixSeparator)
	{
		bRemovePrefixNumChars = false;
	}

	if (bRemovePrefixSeparator && PrefixSeparatorTextBox.IsValid())
	{
		Renamer->GetOptions().RemovePrefixSeparator = PrefixSeparatorTextBox->GetText().ToString();
		Renamer->GetOptions().RemovePrefixCharacterCount = 0;
	}
	else
	{
		Renamer->GetOptions().RemovePrefixSeparator = "";
	}

	Renamer->MarkDirty();
}

bool SAdvancedRenamerPanel::IsPrefixRemoveSeparatorEnabled() const
{
	return bRemovePrefixSeparator;
}

bool SAdvancedRenamerPanel::OnPrefixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const
{
	if (InText.ToString().Len() > 1)
	{
		OutErrorText = LOCTEXT("SeparatorError", "Separators can only be a single character.");
		return false;
	}

	return true;
}

void SAdvancedRenamerPanel::OnPrefixSeparatorChanged(const FText& InNewText)
{
	Renamer->GetOptions().RemovePrefixSeparator = InNewText.ToString();
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsPrefixRemoveCharactersChecked() const
{
	return bRemovePrefixNumChars ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnPrefixRemoveCharactersCheckBoxChanged(ECheckBoxState InNewState)
{
	bRemovePrefixNumChars = InNewState == ECheckBoxState::Checked;

	if (bRemovePrefixNumChars)
	{
		bRemovePrefixSeparator = false;
	}

	if (bRemovePrefixNumChars && PrefixRemoveCharactersSpinBox.IsValid())
	{
		Renamer->GetOptions().RemovePrefixCharacterCount = PrefixRemoveCharactersSpinBox->GetValue();
		Renamer->GetOptions().RemovePrefixSeparator = "";
	}
	else
	{
		Renamer->GetOptions().RemovePrefixCharacterCount = 0;
	}

	Renamer->MarkDirty();
}

bool SAdvancedRenamerPanel::IsPrefixRemoveNumCharsEnabled() const
{
	return bRemovePrefixNumChars;
}

void SAdvancedRenamerPanel::OnPrefixRemoveCharactersChanged(uint8 InNewValue)
{
	Renamer->GetOptions().RemovePrefixCharacterCount = InNewValue;
	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSuffixChanged(const FText& InNewText)
{
	Renamer->GetOptions().AddSuffix = InNewText.ToString();
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSuffixRemoveChecked() const
{
	return bRemoveSuffixSeparator ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnSuffixRemoveCheckBoxChanged(ECheckBoxState InNewState)
{
	bRemoveSuffixSeparator = InNewState == ECheckBoxState::Checked;

	if (bRemoveSuffixSeparator)
	{
		bRemoveSuffixNumChars = false;
	}

	if (bRemoveSuffixSeparator && SuffixSeparatorTextBox.IsValid())
	{
		Renamer->GetOptions().RemoveSuffixSeparator = SuffixSeparatorTextBox->GetText().ToString();
		Renamer->GetOptions().RemoveSuffixCharacterCount = 0;
	}
	else
	{
		Renamer->GetOptions().RemoveSuffixSeparator = "";
	}

	Renamer->MarkDirty();
}

bool SAdvancedRenamerPanel::IsSuffixRemoveSeparatorEnabled() const
{
	return bRemoveSuffixSeparator;
}

bool SAdvancedRenamerPanel::OnSuffixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const
{
	if (InText.ToString().Len() > 1)
	{
		OutErrorText = LOCTEXT("SeparatorError", "Separators can only be a single character.");
		return false;
	}

	return true;
}

void SAdvancedRenamerPanel::OnSuffixSeparatorChanged(const FText& InNewText)
{
	Renamer->GetOptions().RemoveSuffixSeparator = InNewText.ToString();
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSuffixRemoveCharactersChecked() const
{
	return bRemoveSuffixNumChars ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SAdvancedRenamerPanel::IsSuffixRemoveNumCharsEnabled() const
{
	return bRemoveSuffixNumChars;
}

void SAdvancedRenamerPanel::OnSuffixRemoveCharactersCheckBoxChanged(ECheckBoxState InNewState)
{
	bRemoveSuffixNumChars = InNewState == ECheckBoxState::Checked;

	if (bRemoveSuffixNumChars)
	{
		bRemoveSuffixSeparator = false;
	}

	if (bRemoveSuffixNumChars && SuffixRemoveCharactersSpinBox.IsValid())
	{
		Renamer->GetOptions().RemoveSuffixCharacterCount = SuffixRemoveCharactersSpinBox->GetValue();
		Renamer->GetOptions().RemoveSuffixSeparator = "";
	}
	else
	{
		Renamer->GetOptions().RemoveSuffixCharacterCount = 0;
	}

	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSuffixRemoveCharactersChanged(uint8 InNewValue)
{
	Renamer->GetOptions().RemoveSuffixCharacterCount = InNewValue;
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSuffixRemoveNumberChecked() const
{
	return Renamer->GetOptions().bRemoveSuffixNumber ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnSuffixRemoveNumberCheckBoxChanged(ECheckBoxState InNewState)
{
	Renamer->GetOptions().bRemoveSuffixNumber = InNewState == ECheckBoxState::Checked;

	if (!Renamer->GetOptions().bRemoveSuffixNumber)
	{
		Renamer->GetOptions().bAddSuffixNumber = false;
	}

	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSuffixNumberChecked() const
{
	return Renamer->GetOptions().bAddSuffixNumber ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SAdvancedRenamerPanel::IsSuffixRemoveNumberCheckBoxEnabled() const
{
	return !Renamer->GetOptions().bAddSuffixNumber;
}

void SAdvancedRenamerPanel::OnSuffixNumberCheckBoxChanged(ECheckBoxState InNewState)
{
	Renamer->GetOptions().bAddSuffixNumber = InNewState == ECheckBoxState::Checked;

	if (Renamer->GetOptions().bAddSuffixNumber)
	{
		Renamer->GetOptions().bRemoveSuffixNumber = true;
	}

	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSuffixNumberStartChanged(int32 InNewValue)
{
	Renamer->GetOptions().AddSuffixNumberStart = InNewValue;
	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSuffixNumberStepChanged(int32 InNewValue)
{
	Renamer->GetOptions().AddSuffixNumberStep = InNewValue;
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSearchReplacePlainTextChecked() const
{
	return Renamer->GetOptions().SearchAndReplaceType == EAdvancedRenamerSeachAndReplaceType::PlainText ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnSearchReplacePlainTextCheckBoxChanged(ECheckBoxState InNewState)
{
	if (InNewState != ECheckBoxState::Checked)
	{
		return;
	}

	Renamer->GetOptions().SearchAndReplaceType = EAdvancedRenamerSeachAndReplaceType::PlainText;
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSearchReplaceRegexChecked() const
{
	return Renamer->GetOptions().SearchAndReplaceType == EAdvancedRenamerSeachAndReplaceType::RegularExpression ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnSearchReplaceRegexCheckBoxChanged(ECheckBoxState InNewState)
{
	if (InNewState != ECheckBoxState::Checked)
	{
		return;
	}

	Renamer->GetOptions().SearchAndReplaceType = EAdvancedRenamerSeachAndReplaceType::RegularExpression;
	Renamer->MarkDirty();
}

ECheckBoxState SAdvancedRenamerPanel::IsSearchReplaceIgnoreCaseChecked() const
{
	return Renamer->GetOptions().SearchAndReplaceCase == ESearchCase::IgnoreCase ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedRenamerPanel::OnSearchReplaceIgnoreCaseCheckBoxChanged(ECheckBoxState InNewState)
{
	switch (InNewState)
	{
		case ECheckBoxState::Checked:
			Renamer->GetOptions().SearchAndReplaceCase = ESearchCase::IgnoreCase;
			break;

		case ECheckBoxState::Unchecked:
			Renamer->GetOptions().SearchAndReplaceCase = ESearchCase::CaseSensitive;
			break;

		default:
			return;
	}

	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSearchReplaceSearchTextChanged(const FText& InNewText)
{
	Renamer->GetOptions().SearchAndReplaceFromText = InNewText.ToString();

	if (Renamer->GetOptions().SearchAndReplaceType == EAdvancedRenamerSeachAndReplaceType::None)
	{
		Renamer->GetOptions().SearchAndReplaceType = EAdvancedRenamerSeachAndReplaceType::PlainText;
	}

	Renamer->MarkDirty();
}

void SAdvancedRenamerPanel::OnSearchReplaceReplaceTextChanged(const FText& InNewText)
{
	Renamer->GetOptions().SearchAndReplaceToText = InNewText.ToString();

	if (Renamer->GetOptions().SearchAndReplaceType == EAdvancedRenamerSeachAndReplaceType::None)
	{
		Renamer->GetOptions().SearchAndReplaceType = EAdvancedRenamerSeachAndReplaceType::PlainText;
	}

	Renamer->MarkDirty();
}

TSharedRef<ITableRow> SAdvancedRenamerPanel::OnGenerateRowForList(TSharedPtr<FAdvancedRenamerPreview> InItem, 
	const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SAdvancedRenamerPreviewListRow, SharedThis(this), InOwnerTable, InItem);
}

FReply SAdvancedRenamerPanel::OnListViewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedPtr<SWidget> SAdvancedRenamerPanel::GenerateListViewContextMenu()
{
	if (!RenamePreviewList.IsValid())
	{
		return nullptr;
	}

	if (RenamePreviewList->GetNumItemsSelected() == 0)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList.ToSharedRef());

	MenuBuilder.BeginSection("Actions", LOCTEXT("Actions", "Actions"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("RemoveObject", "Remove Object"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SAdvancedRenamerPanel::IsApplyButtonEnabled() const
{
	return !Renamer->IsDirty() && Renamer->HasRenames();
}

FReply SAdvancedRenamerPanel::OnApplyButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("AdvancedRenamerRename", "Advanced Renamer Rename"));

	if (Renamer->Execute())
	{
		CloseWindow();
	}

	return FReply::Handled();
}

FVector2D SAdvancedRenamerPanel::GetRightPaneSize() const
{
	using namespace UE::AdvancedRenamer::Private;

	const FVector2f GeoSize = GetTickSpaceGeometry().GetLocalSize();

	return FVector2D(
		FMath::Max(RightBlockSize.X, GeoSize.X - 319.0),
		RightBlockSize.Y
	);
}

FVector2D SAdvancedRenamerPanel::GetListViewsize() const
{
	using namespace UE::AdvancedRenamer::Private;

	const FVector2f GeoSize = GetTickSpaceGeometry().GetLocalSize();

	return FVector2D(
		FMath::Max(ListViewSize.X, GeoSize.X - 323.0),
		ListViewSize.Y
	);
}

FVector2D SAdvancedRenamerPanel::GetApplyButtonSize() const
{
	using namespace UE::AdvancedRenamer::Private;

	const FVector2f GeoSize = GetTickSpaceGeometry().GetLocalSize();

	return FVector2D(
		FMath::Max(ListViewSize.X, GeoSize.X - 323.0),
		ApplyButtonHeight
	);
}

#undef LOCTEXT_NAMESPACE
