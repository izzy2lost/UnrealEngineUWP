// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurvePresetGroupItem.h"
#include "EaseCurveTool/AvaEaseCurveStyle.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurvePresetDragDropOp.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreview.h"
#include "Internationalization/Text.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurvePresetGroupItem"

void SAvaEaseCurvePresetGroupItem::Construct(const FArguments& InArgs, const TSharedPtr<STableViewBase>& InOwnerTableView)
{
	Preset = InArgs._Preset;
	bIsEditMode = InArgs._IsEditMode;
	OnClick = InArgs._OnClick;
	OnDelete = InArgs._OnDelete;
	OnRename = InArgs._OnRename;
	OnBeginMove = InArgs._OnBeginMove;
	OnEndMove = InArgs._OnEndMove;

	ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(this, &SAvaEaseCurvePresetGroupItem::GetBorderVisibility)
				.BorderImage(this, &SAvaEaseCurvePresetGroupItem::GetBackgroundImage)
			]
			+ SOverlay::Slot()
			.Padding(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(3.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(160)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]()
							{
								return IsEditMode() ? 1 : 0;
							})
						+ SWidgetSwitcher::Slot()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), TEXT("Menu.Label"))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.Text(FText::FromString(Preset->Name))
							.ToolTipText(FText::FromString(Preset->Name))
						]
						+ SWidgetSwitcher::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton.NoPad"))
								.VAlign(VAlign_Center)
								.ToolTipText(LOCTEXT("EditModeDeleteTooltip", "Delete this category and the json file associated with it on disk"))
								.Visibility(this, &SAvaEaseCurvePresetGroupItem::GetEditModeVisibility)
								.OnClicked(this, &SAvaEaseCurvePresetGroupItem::HandleDeleteClick)
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f))
									.Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))
								]
							]
							+ SHorizontalBox::Slot()
							.Padding(2.f, 0.f, 0.f, 0.f)
							[
								SAssignNew(RenameTextBox, SEditableTextBox)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Text(FText::FromString(Preset->Name))
								.ToolTipText(FText::FromString(Preset->Name))
								.OnTextCommitted(this, &SAvaEaseCurvePresetGroupItem::HandleRenameTextCommitted)
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FStyleColors::White25)
					.Padding(2.f)
					[
						SNew(SAvaEaseCurvePreview)
						.PreviewSize(20.f)
						.CurveThickness(1.5f)
						.Tangents(Preset->Tangents)
						.CustomToolTip(true)
						.UnderCurveColor(FStyleColors::SelectInactive.GetSpecifiedColor())
						.DisplayRate(InArgs._DisplayRate)
					]
				]
			]
		];

	STableRow<TSharedPtr<FAvaEaseCurvePreset>>::ConstructInternal(
		STableRow::FArguments()
		.Style(&FAppStyle::GetWidgetStyle<FTableRowStyle>(TEXT("ComboBox.Row")))
		.Padding(5.f)
		.ShowSelection(true)
		, InOwnerTableView.ToSharedRef());
}

void SAvaEaseCurvePresetGroupItem::SetPreset(const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
{
	Preset = InPreset;
}

void SAvaEaseCurvePresetGroupItem::HandlePresetClick() const
{
	if (OnClick.IsBound())
	{
		OnClick.Execute(Preset);
	}
}

bool SAvaEaseCurvePresetGroupItem::IsEditMode() const
{
	return bIsEditMode.Get(false);
}

EVisibility SAvaEaseCurvePresetGroupItem::GetEditModeVisibility() const
{
	return IsEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAvaEaseCurvePresetGroupItem::GetBorderVisibility() const
{
	return bIsDragging && IsEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SAvaEaseCurvePresetGroupItem::GetBackgroundImage() const
{
	return bIsDragging ? FAvaEaseCurveStyle::Get().GetBrush(TEXT("EditMode.Background.Over")) : nullptr;
}

void SAvaEaseCurvePresetGroupItem::HandleRenameTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType) const
{
	const FString NewPresetName = InNewText.ToString();
	
	if (!NewPresetName.IsEmpty() && !NewPresetName.Equals(Preset->Name)
		&& OnRename.IsBound() && OnRename.Execute(Preset, NewPresetName))
	{
		Preset->Name = NewPresetName;
	}
	else
	{
		RenameTextBox->SetText(FText::FromString(Preset->Name));
	}
}

FReply SAvaEaseCurvePresetGroupItem::HandleDeleteClick() const
{
	if (OnDelete.IsBound())
	{
		OnDelete.Execute(Preset);
	}

	return FReply::Handled();
}

FReply SAvaEaseCurvePresetGroupItem::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsEditMode())
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	if (OnClick.IsBound())
	{
		OnClick.Execute(Preset);
	}

	return FReply::Handled();
}

FReply SAvaEaseCurvePresetGroupItem::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	TriggerEndMove();

	return STableRow<TSharedPtr<FAvaEaseCurvePreset>>::OnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply SAvaEaseCurvePresetGroupItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsEditMode())
	{
		return FReply::Unhandled();
	}

	TriggerBeginMove();

	const TSharedRef<FAvaEaseCurvePresetDragDropOperation> Operation = MakeShared<FAvaEaseCurvePresetDragDropOperation>(SharedThis(this), Preset);
	return FReply::Handled().BeginDragDrop(Operation);
}

FReply SAvaEaseCurvePresetGroupItem::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	bIsDragging = false;

	return STableRow<TSharedPtr<FAvaEaseCurvePreset>>::OnDrop(InGeometry, InDragDropEvent);
}

void SAvaEaseCurvePresetGroupItem::TriggerBeginMove()
{
	if (OnBeginMove.IsBound())
	{
		OnBeginMove.Execute(Preset, Preset->Category);
	}

	bIsDragging = true;
}

void SAvaEaseCurvePresetGroupItem::TriggerEndMove()
{
	if (OnEndMove.IsBound())
	{
		OnEndMove.Execute(Preset, Preset->Category);
	}

	bIsDragging = false;
}

#undef LOCTEXT_NAMESPACE
