// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/IItemsSource.h"

class FRCSignatureTreeItemBase;
class SRCSignatureRow;
class SScrollBox;
struct FRCSignatureActionType;
template<typename OptionType> class SComboBox;

class SRCSignatureActionBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRCSignatureActionBox) {}
		SLATE_ATTRIBUTE(bool, LiveMode)
		SLATE_ITEMS_SOURCE_ARGUMENT(TSharedPtr<FRCSignatureActionType>, ActionTypesSource)
		SLATE_EVENT(TDelegate<void()>, OnActionTypesComboBoxOpening)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FRCSignatureTreeItemBase>& InItem, const TSharedRef<SRCSignatureRow>& InRow);

private:
	void Refresh();

	bool CanAddAction() const;

	EVisibility GetAddActionVisibility() const;

	TSharedRef<SWidget> GenerateActionTypeWidget(TSharedPtr<FRCSignatureActionType> InActionType);

	void OnActionTypeSelected(TSharedPtr<FRCSignatureActionType> InActionType, ESelectInfo::Type InSelectType);

	TSharedPtr<SComboBox<TSharedPtr<FRCSignatureActionType>>> ActionTypesComboBox;

	TWeakPtr<FRCSignatureTreeItemBase> ItemWeak;

	TSharedPtr<SScrollBox> ActionListBox;

	TAttribute<bool> IsHovered;
	TAttribute<bool> IsSelected;
	TAttribute<bool> LiveMode;
};
