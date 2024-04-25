// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCSignatureLabel.h"
#include "Styling/SlateTypes.h"
#include "UI/Signature/Items/RCSignatureTreeItemBase.h"
#include "UI/Signature/SRCSignatureRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"

#define LOCTEXT_NAMESPACE "SRCSignatureLabel"

void SRCSignatureLabel::Construct(const FArguments& InArgs
	, const TSharedRef<FRCSignatureTreeItemBase>& InItem
	, const TSharedRef<SRCSignatureRow>& InRow)
{
	ItemWeak = InItem;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight(25)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SExpanderArrow, InRow)
				.IndentAmount(12)
				.ShouldDrawWires(true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SRCSignatureLabel::GetItemEnabledState)
				.OnCheckStateChanged(this, &SRCSignatureLabel::SetItemEnabledState)
				.Padding(FMargin(4.0f, 0.0f))
				.ToolTipText(LOCTEXT("ItemEnableCheckBoxTooltip", "Determines whether the entry is enabled or not"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SInlineEditableTextBlock)
				.Text(this, &SRCSignatureLabel::GetSignatureDisplayName)
				.OnTextCommitted(this, &SRCSignatureLabel::OnSignatureDisplayNameCommitted)
				.IsSelected(InRow, &SRCSignatureRow::IsSelected)
			]
		]
	];
}

ECheckBoxState SRCSignatureLabel::GetItemEnabledState() const
{
	if (!CachedCheckBoxState.IsSet())
	{
		TSharedPtr<FRCSignatureTreeItemBase> Item = ItemWeak.Pin();
		TOptional<bool> ItemEnabled = Item.IsValid() ? Item->IsEnabled() : TOptional<bool>();

		if (ItemEnabled.IsSet())
		{
			CachedCheckBoxState = *ItemEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		else
		{
			CachedCheckBoxState = ECheckBoxState::Undetermined;
		}
	}
	return *CachedCheckBoxState;
}

void SRCSignatureLabel::SetItemEnabledState(ECheckBoxState InState)
{
	if (TSharedPtr<FRCSignatureTreeItemBase> Item = ItemWeak.Pin())
	{
		Item->SetEnabled(InState == ECheckBoxState::Checked);
		CachedCheckBoxState.Reset();
	}
}

FText SRCSignatureLabel::GetSignatureDisplayName() const
{
	if (!CachedDisplayName.IsSet())
	{
		TSharedPtr<FRCSignatureTreeItemBase> Item = ItemWeak.Pin();
		CachedDisplayName = Item.IsValid() ? Item->GetDisplayNameText() : FText::GetEmpty();
	}
	return *CachedDisplayName;
}

void SRCSignatureLabel::OnSignatureDisplayNameCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (TSharedPtr<FRCSignatureTreeItemBase> Item = ItemWeak.Pin())
	{
		Item->SetDisplayNameText(InText);
		CachedDisplayName.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
