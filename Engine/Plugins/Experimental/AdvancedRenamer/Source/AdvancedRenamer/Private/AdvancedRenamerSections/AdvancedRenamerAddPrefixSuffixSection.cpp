// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerAddPrefixSuffixSection.h"

#include "AdvancedRenamerStyle.h"
#include "IAdvancedRenamer.h"
#include "Styling/StyleColors.h"
#include "Utils/AdvancedRenamerSlateUtils.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "AdvancedRenamerAddPrefixSuffixSection"

FAdvancedRenamerAddPrefixSuffixSection::FAdvancedRenamerAddPrefixSuffixSection()
{
	FAdvancedRenamerAddPrefixSuffixSection::ResetToDefault();
}

void FAdvancedRenamerAddPrefixSuffixSection::Init(TSharedRef<IAdvancedRenamer> InRenamer)
{
	FAdvancedRenamerSectionBase::Init(InRenamer);
	Section.SectionName = TEXT("AddPrefixSuffixNumber");
	Section.OnBeforeOperationExecutionStart().BindSP(this, &FAdvancedRenamerAddPrefixSuffixSection::ResetCurrentSuffixNumber);
	Section.OnOperationExecuted().BindSP(this, &FAdvancedRenamerAddPrefixSuffixSection::ApplyAddPrefixSuffixNumberOperation);
	InRenamer->AddSection(Section);
}

TSharedRef<SWidget> FAdvancedRenamerAddPrefixSuffixSection::GetWidget()
{
	using namespace AdvancedRenamerSlateUtils::Default;

	return SNew(SBorder)
		.BorderImage(FAdvancedRenamerStyle::Get().GetBrush("AdvancedRenamer.Style.BackgroundBorder"))
		.Content()
		[
			SNew(SVerticalBox)

			//Add Prefix
			+ SVerticalBox::Slot()
			.Padding(SectionContentFirstEntryPadding)
			.AutoHeight()
			[
				CreateAddPrefix()
			]

			// Add Suffix
			+ SVerticalBox::Slot()
			.Padding(SectionContentMiddleEntriesPadding)
			.AutoHeight()
			[
				CreateAddSuffix()
			]
			
			// Add Number
			+ SVerticalBox::Slot()
			.Padding(SectionContentMiddleEntriesPadding)
			.AutoHeight()
			[
				CreateAddNumber()
			]
		];
}

void FAdvancedRenamerAddPrefixSuffixSection::ResetToDefault()
{
	PrefixText = FText::GetEmpty();
	SuffixText = FText::GetEmpty();
	bAddSuffixNumbers = false;
	SuffixNumberStartValue = 1;
	CurrentSuffixNumber = 1;
	SuffixNumberStepValue = 1;
}

TSharedRef<SWidget> FAdvancedRenamerAddPrefixSuffixSection::CreateAddPrefix()
{
	using namespace AdvancedRenamerSlateUtils::Default;
	
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(NameWidgetPadding)
		[
			SNew(SBox)
			.WidthOverride(70.f)
			[
				SNew(STextBlock)
				.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
				.Text(LOCTEXT("AR_AddPrefix", "Add Prefix"))
			]
		]
		
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1.f)
		.Padding(ValueWidgetPadding)
		[
			SAssignNew(PrefixTextBox, SEditableTextBox)
			.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.HintText(LOCTEXT("AR_PrefixHint", "New Prefix"))
			.Text(this, &FAdvancedRenamerAddPrefixSuffixSection::GetPrefixText)
			.OnTextChanged(this, &FAdvancedRenamerAddPrefixSuffixSection::OnPrefixChanged)
		];
}

TSharedRef<SWidget> FAdvancedRenamerAddPrefixSuffixSection::CreateAddSuffix()
{
	using namespace AdvancedRenamerSlateUtils::Default;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(NameWidgetPadding)
		[
			SNew(SBox)
			.WidthOverride(70.f)
			[
				SNew(STextBlock)
				.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
				.Text(LOCTEXT("BR_AddSuffix", "Add Suffix"))
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1.f)
		.Padding(ValueWidgetPadding)
		[
			SAssignNew(SuffixTextBox, SEditableTextBox)
			.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.HintText(LOCTEXT("AR_SuffixHint", "New Suffix"))
			.Text(this, &FAdvancedRenamerAddPrefixSuffixSection::GetSuffixText)
			.OnTextChanged(this, &FAdvancedRenamerAddPrefixSuffixSection::OnSuffixChanged)
		];
}

TSharedRef<SWidget> FAdvancedRenamerAddPrefixSuffixSection::CreateAddNumber()
{
	using namespace AdvancedRenamerSlateUtils::Default;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(NameWidgetPadding)
		[
			SAssignNew(SuffixNumberCheckBox, SCheckBox)
			.IsChecked(this, &FAdvancedRenamerAddPrefixSuffixSection::IsSuffixNumberChecked)
			.OnCheckStateChanged(this, &FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberCheckBoxChanged)
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.Text(LOCTEXT("AR_SuffixNumber", "Add Number"))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.Text(LOCTEXT("AR_Start", "Start"))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(ValueWidgetPadding)
		[
			SAssignNew(SuffixNumberStartSpinBox, SSpinBox<int32>)
			.Style(&FAppStyle::Get(), "Menu.SpinBox")
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.MinValue(1)
			.MaxValue(99)
			.Value(this, &FAdvancedRenamerAddPrefixSuffixSection::GetSuffixNumberStart)
			.IsEnabled(this, &FAdvancedRenamerAddPrefixSuffixSection::IsSuffixNumberEnabled)
			.OnValueChanged(this, &FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberStartChanged)
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(AddNumberStepPadding)
		[
			SNew(STextBlock)
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.Text(LOCTEXT("AR_Step", "Step"))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(ValueWidgetPadding)
		[
			SAssignNew(SuffixNumberStepSpinBox, SSpinBox<int32>)
			.Style(&FAppStyle::Get(), "Menu.SpinBox")
			.Font(FAdvancedRenamerStyle::Get().GetFontStyle("AdvancedRenamer.Style.RegularFont"))
			.MinValue(1)
			.MaxValue(99)
			.Value(this, &FAdvancedRenamerAddPrefixSuffixSection::GetSuffixNumberStep)
			.IsEnabled(this, &FAdvancedRenamerAddPrefixSuffixSection::IsSuffixNumberEnabled)
			.OnValueChanged(this, &FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberStepChanged)
		];
}

FText FAdvancedRenamerAddPrefixSuffixSection::GetPrefixText() const
{
	return PrefixText;
}

FText FAdvancedRenamerAddPrefixSuffixSection::GetSuffixText() const
{
	return SuffixText;
}

int32 FAdvancedRenamerAddPrefixSuffixSection::GetSuffixNumberStart() const
{
	return SuffixNumberStartValue;
}

int32 FAdvancedRenamerAddPrefixSuffixSection::GetSuffixNumberStep() const
{
	return SuffixNumberStepValue;
}

ECheckBoxState FAdvancedRenamerAddPrefixSuffixSection::IsSuffixNumberChecked() const
{
	return bAddSuffixNumbers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool FAdvancedRenamerAddPrefixSuffixSection::IsSuffixNumberEnabled() const
{
	return bAddSuffixNumbers;
}

void FAdvancedRenamerAddPrefixSuffixSection::OnPrefixChanged(const FText& InNewText)
{
	PrefixText = InNewText;
	MarkRenamerDirty();
}

void FAdvancedRenamerAddPrefixSuffixSection::OnSuffixChanged(const FText& InNewText)
{
	SuffixText = InNewText;
	MarkRenamerDirty();
}

void FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberCheckBoxChanged(ECheckBoxState InNewState)
{
	bAddSuffixNumbers = InNewState == ECheckBoxState::Checked;
	MarkRenamerDirty();
}

void FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberStartChanged(int32 InNewValue)
{
	SuffixNumberStartValue = InNewValue;
	MarkRenamerDirty();
}

void FAdvancedRenamerAddPrefixSuffixSection::OnSuffixNumberStepChanged(int32 InNewValue)
{
	SuffixNumberStepValue = InNewValue;
	MarkRenamerDirty();
}

bool FAdvancedRenamerAddPrefixSuffixSection::CanApplyAddPrefixOperation()
{
	return !PrefixText.IsEmpty();
}

bool FAdvancedRenamerAddPrefixSuffixSection::CanApplyAddSuffixOperation()
{
	return !SuffixText.IsEmpty();
}

bool FAdvancedRenamerAddPrefixSuffixSection::CanApplyAddSuffixNumberOperation()
{
	return IsSuffixNumberEnabled();
}

void FAdvancedRenamerAddPrefixSuffixSection::ApplyAddPrefixOperation(FString& OutOriginalName)
{
	OutOriginalName = PrefixText.ToString() + OutOriginalName;
}

void FAdvancedRenamerAddPrefixSuffixSection::ApplyAddSuffixOperation(FString& OutOriginalName)
{
	OutOriginalName = OutOriginalName + SuffixText.ToString();
}

void FAdvancedRenamerAddPrefixSuffixSection::ApplyAddSuffixNumberOperation(FString& OutOriginalName)
{
	OutOriginalName = OutOriginalName + FString::FromInt(CurrentSuffixNumber);
	CurrentSuffixNumber += SuffixNumberStepValue;
}

void FAdvancedRenamerAddPrefixSuffixSection::ResetCurrentSuffixNumber()
{
	CurrentSuffixNumber = SuffixNumberStartValue;
}

void FAdvancedRenamerAddPrefixSuffixSection::ApplyAddPrefixSuffixNumberOperation(FString& OutOriginalName)
{
	if (CanApplyAddPrefixOperation())
	{
		ApplyAddPrefixOperation(OutOriginalName);
	}

	if (CanApplyAddSuffixOperation())
	{
		ApplyAddSuffixOperation(OutOriginalName);
	}

	if (CanApplyAddSuffixNumberOperation())
	{
		ApplyAddSuffixNumberOperation(OutOriginalName);
	}
}

#undef LOCTEXT_NAMESPACE
