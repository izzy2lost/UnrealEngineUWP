// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorCueListRow.h"

#include "DMXEditorStyle.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorCueListRow"

namespace UE::DMX::Private
{ 
	void SDMXControlConsoleEditorCueListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXControlConsoleEditorCueListItem>& InItem)
	{
		Item = InItem;

		OnEditCueItemColorDelegate = InArgs._OnEditCueItemColor;
		OnRenameCueItemDelegate = InArgs._OnRenameCueItem;
		OnMoveCueItemDelegate = InArgs._OnMoveCueItem;
		OnDeleteCueItemDelegate = InArgs._OnDeleteCueItem;

		SMultiColumnTableRow<TSharedPtr<FDMXControlConsoleEditorCueListItem>>::Construct(
			FSuperRowType::FArguments()
			.IsEnabled(InArgs._IsEnabled)
			.Style(&FDMXEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("FixturePatchList.Row")),
			InOwnerTable);
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueListRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		if (ColumnName == FDMXControlConsoleEditorCueListColumnIDs::Color)
		{
			return GenerateCueColorRow();
		}
		else if (ColumnName == FDMXControlConsoleEditorCueListColumnIDs::Name)
		{
			return GenerateCueNameRow();
		}
		else if (ColumnName == FDMXControlConsoleEditorCueListColumnIDs::Options)
		{
			return GenerateCueOptionsRow();
		}

		return SNullWidget::NullWidget;
	}

	FReply SDMXControlConsoleEditorCueListRow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CueLabelEditableTextBlock.IsValid())
		{
			CueLabelEditableTextBlock->EnterEditingMode();
		}

		return FReply::Unhandled();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueListRow::GenerateCueColorRow()
	{
		return
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.Padding(5.f, 2.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(SImage)
				.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.WhiteRoundedPropertyBorder"))
				.ColorAndOpacity(Item.Get(), &FDMXControlConsoleEditorCueListItem::GetCueColor)
				.OnMouseButtonDown(this, &SDMXControlConsoleEditorCueListRow::OnCueColorMouseButtonClick)
			];
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueListRow::GenerateCueNameRow()
	{
		return
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(4.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SAssignNew(CueLabelEditableTextBlock, SInlineEditableTextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(Item.Get(), &FDMXControlConsoleEditorCueListItem::GetCueNameText)
				.ColorAndOpacity(FLinearColor::White)
				.OnTextCommitted(this, &SDMXControlConsoleEditorCueListRow::OnCueNameTextCommitted)
			];
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueListRow::GenerateCueOptionsRow()
	{
		return
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(4.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(22.0f)
					.HeightOverride(22.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &SDMXControlConsoleEditorCueListRow::OnMoveItemClicked, EListItemMoveDirection::Previous)
						.ContentPadding(0.0f)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.ChevronUp"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(22.0f)
					.HeightOverride(22.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &SDMXControlConsoleEditorCueListRow::OnMoveItemClicked, EListItemMoveDirection::Next)
						.ContentPadding(0.0f)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(22.0f)
					.HeightOverride(22.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &SDMXControlConsoleEditorCueListRow::OnDeleteItemClicked)
						.ContentPadding(0.0f)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.X"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]
			];
	}

	FReply SDMXControlConsoleEditorCueListRow::OnCueColorMouseButtonClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Item.IsValid())
		{
			return FReply::Unhandled();
		}

		const FLinearColor InitialColor = Item->GetCueColor().GetSpecifiedColor();

		FColorPickerArgs PickerArgs;
		{
			PickerArgs.bOnlyRefreshOnOk = true;
			PickerArgs.bUseAlpha = true;
			PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SDMXControlConsoleEditorCueListRow::OnSetCueColorFromColorPicker);
			PickerArgs.InitialColor = InitialColor;
			PickerArgs.ParentWidget = AsShared();
			FWidgetPath ParentWidgetPath;
			if (FSlateApplication::Get().FindPathToWidget(AsShared(), ParentWidgetPath))
			{
				PickerArgs.bOpenAsMenu = FSlateApplication::Get().FindMenuInWidgetPath(ParentWidgetPath).IsValid();
			}
		}

		OpenColorPicker(PickerArgs);

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorCueListRow::OnSetCueColorFromColorPicker(FLinearColor NewColor)
	{
		if (Item.IsValid())
		{
			Item->SetCueColor(NewColor);
			OnEditCueItemColorDelegate.ExecuteIfBound(Item);
		}
	}
	void SDMXControlConsoleEditorCueListRow::OnCueNameTextCommitted(const FText& NewName, ETextCommit::Type InCommit)
	{
		if (Item.IsValid())
		{
			Item->SetCueName(NewName.ToString());
			OnRenameCueItemDelegate.ExecuteIfBound(Item);
		}
	}

	FReply SDMXControlConsoleEditorCueListRow::OnMoveItemClicked(EListItemMoveDirection MoveDirection)
	{
		if (Item.IsValid())
		{
			OnMoveCueItemDelegate.ExecuteIfBound(Item, MoveDirection);
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorCueListRow::OnDeleteItemClicked()
	{
		if (Item.IsValid())
		{
			OnDeleteCueItemDelegate.ExecuteIfBound(Item);
		}

		return FReply::Handled();
	}
}

#undef LOCTEXT_NAMESPACE
