// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActionableMessageSubsystem.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class LANDSCAPEEDITOR_API SActionableMessageEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActionableMessageEntry) {}
	SLATE_ARGUMENT(TSharedPtr<FActionableMessage>, ActionableMessage)
	SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetActionableMessage(TSharedPtr<FActionableMessage> InActionableMessage);

private:
	TSharedPtr<FActionableMessage> ActionableMessage;
	FOnClicked OnClicked;
};

class LANDSCAPEEDITOR_API SActionableMessageViewportWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActionableMessageViewportWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	EVisibility GetVisibility();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FActionableMessage> InActionableMessage, const TSharedRef<STableViewBase>& OwnerTable);

private:
	TSharedPtr<SActionableMessageEntry> MainActionableMessage;
	TSharedPtr<SListView<TSharedPtr<FActionableMessage>>> ActionableMessageList;
	TArray<TSharedPtr<FActionableMessage>> ActionableMessages;
	uint32 CachedStateID = 0;
	bool bMainMessageFilled = false;
	bool bExpanded = false;
};
