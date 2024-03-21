// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActionableMessageViewportWidget.h"

#include "ActionableMessageSubsystem.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "ActionableMessageViewportWidget"

void SActionableMessageEntry::Construct(const FArguments& InArgs)
{
	ActionableMessage = InArgs._ActionableMessage;
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		.ToolTipText_Lambda([this]() { return ActionableMessage.IsValid() ? ActionableMessage->Tooltip : FText::GetEmpty(); })
		+ SHorizontalBox::Slot()
		.Padding(FMargin(4, 0, 8, 0))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.FillWidth(1)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return ActionableMessage.IsValid() ? ActionableMessage->Message : FText::GetEmpty(); })
			.ColorAndOpacity(FStyleColors::Foreground)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Lambda([this]()
			{
				if (!ActionableMessage.IsValid() || ActionableMessage->ActionMessage.IsEmpty())
				{
					return EVisibility::Collapsed;
				}

				return EVisibility::Visible;
			})
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ContentPadding(FMargin(-4.f, 2.f))
			.OnClicked_Lambda([this]()
			{
				if (ActionableMessage.IsValid() && (ActionableMessage->ActionCallback != nullptr))
				{
					ActionableMessage->ActionCallback();
				}

				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("ActionableMessage.Update"))
					.DesiredSizeOverride(FVector2D(12, 12))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return ActionableMessage.IsValid() ? ActionableMessage->ActionMessage : FText::GetEmpty(); })
					.ColorAndOpacity(FStyleColors::White)
				]
			]
		]
	];
}

void SActionableMessageEntry::SetActionableMessage(TSharedPtr<FActionableMessage> InActionableMessage)
{
	ActionableMessage = InActionableMessage;
}

void SActionableMessageViewportWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("HoverHintOnly"))
			.IsEnabled(true)
			.OnClicked_Lambda([this]()
			{
				bExpanded = !bExpanded;
				return FReply::Handled();
			})
			.ContentPadding(FMargin(0.f, 4.f, 0.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(4.f, 0, 4.f, 0))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.FillWidth(1)
				[
					SNew(SImage)
					.Image_Lambda([this]() { return bExpanded ? FAppStyle::GetBrush("ContentBrowser.SortDown") : FAppStyle::GetBrush("Symbols.RightArrow"); })
					.Visibility_Lambda([this]() { return ActionableMessages.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0, 2.f, 0))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.FillWidth(1)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(FString::FromInt(ActionableMessages.Num())); })
					.Visibility_Lambda([this]() { return (ActionableMessages.Num() > 1) ? EVisibility::Visible : EVisibility::Collapsed; })
					.ColorAndOpacity(FStyleColors::Foreground)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0, 4.f, 0))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("ActionableMessage.Warning"))
					.DesiredSizeOverride(FVector2D(16, 16))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ActionableMessageList, SListView<TSharedPtr<FActionableMessage>>)
			.ListViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("ActionableMessage.ListView"))
			.ItemHeight(48)
			.ListItemsSource(&ActionableMessages)
			.OnGenerateRow(this, &SActionableMessageViewportWidget::OnGenerateRow)
			.Visibility_Lambda([this]()
			{
				return bExpanded ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
	];
}

EVisibility SActionableMessageViewportWidget::GetVisibility()
{
	if (UWorld* World = GEditor->GetEditorWorldContext().World())
	{
		const UActionableMessageSubsystem* ActionableMessageSubsystem = World->GetSubsystem<UActionableMessageSubsystem>();
		
		if (ActionableMessageSubsystem == nullptr)
		{
			return EVisibility::Collapsed;
		}

		const uint32 SubsystemStateID = ActionableMessageSubsystem->GetStateID();

		if (CachedStateID == SubsystemStateID)
		{
			return ActionableMessages.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		}
		
		const TMap<FName, TSharedPtr<FActionableMessage>>& ActionableMessageSource = ActionableMessageSubsystem->GetActionableMessages();

		ActionableMessages.Empty();

		Algo::Transform(ActionableMessageSource, ActionableMessages, [](const TPair<FName, TSharedPtr<FActionableMessage>>& ActionableMessagePair)
		{
			return ActionableMessagePair.Value;
		});

		CachedStateID = SubsystemStateID;
		
		return ActionableMessages.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

TSharedRef<ITableRow> SActionableMessageViewportWidget::OnGenerateRow(TSharedPtr<FActionableMessage> InActionableMessage, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(InActionableMessage.IsValid());

	return SNew(STableRow<TSharedPtr<FActionableMessage>>, OwnerTable)
	.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("ActionableMessage.ListViewRow"))
	.Padding(FMargin(8.f, 0.f, 8.f, 4.f))
	[
		SNew(SActionableMessageEntry)
		.ActionableMessage(InActionableMessage)
	];
}

#undef LOCTEXT_NAMESPACE
