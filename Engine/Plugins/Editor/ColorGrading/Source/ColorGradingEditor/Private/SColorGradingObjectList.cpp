// Copyright Epic Games, Inc. All Rights Reserved.

#include "SColorGradingObjectList.h"

#include "ClassIconFinder.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ColorGrading"

bool FColorGradingListItem::operator<(const FColorGradingListItem& Other) const
{
	FString ThisName = TEXT("");
	FString OtherName = TEXT("");

	if (Component.IsValid())
	{
		ThisName = Component->GetName();
	}
	else if (Actor.IsValid())
	{
		ThisName = Actor->GetActorLabel();
	}

	if (Other.Component.IsValid())
	{
		OtherName = Other.Component->GetName();
	}
	else if (Other.Actor.IsValid())
	{
		OtherName = Other.Actor->GetActorLabel();
	}

	return ThisName < OtherName;
}

namespace ColorGradingObjectListColumnNames
{
	const static FName ItemEnabled(TEXT("ItemEnabled"));
	const static FName ItemLabel(TEXT("ItemLabel"));
};

class SColorGradingListItemRow : public SMultiColumnTableRow<FColorGradingListItemRef>
{
public:
	SLATE_BEGIN_ARGS(SColorGradingListItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const FColorGradingListItemRef InListItem)
	{
		ListItem = InListItem;

		SMultiColumnTableRow<FColorGradingListItemRef>::Construct(
			SMultiColumnTableRow<FColorGradingListItemRef>::FArguments()
			.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
			, InOwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ColorGradingObjectListColumnNames::ItemEnabled)
		{
			return SNew(SCheckBox)
				.IsChecked(this, &SColorGradingListItemRow::GetCheckBoxState)
				.OnCheckStateChanged(this, &SColorGradingListItemRow::OnCheckBoxStateChanged);
		}
		else if (ColumnName == ColorGradingObjectListColumnNames::ItemLabel)
		{
			return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(6, 1))
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(16)
					.HeightOverride(16)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(this, &SColorGradingListItemRow::GetItemIcon)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(this, &SColorGradingListItemRow::GetItemLabel)
				];
		}

		return SNullWidget::NullWidget;
	}

private:
	FText GetItemLabel() const
	{
		FString ItemName = TEXT("");

		if (ListItem.IsValid())
		{
			if (ListItem->Component.IsValid())
			{
				ItemName = ListItem->Component->GetName();
			}
			else if (ListItem->Actor.IsValid())
			{
				ItemName = ListItem->Actor->GetActorLabel();
			}
		}

		return FText::FromString(*ItemName);
	}

	const FSlateBrush* GetItemIcon() const
	{
		if (ListItem.IsValid())
		{
			if (ListItem->Component.IsValid())
			{
				return FSlateIconFinder::FindIconBrushForClass(ListItem->Component->GetClass(), TEXT("SCS.Component"));
			}
			else if (ListItem->Actor.IsValid())
			{
				return FClassIconFinder::FindIconForActor(ListItem->Actor);
			}
		}

		return nullptr;
	}

	EVisibility GetCheckBoxVisibility() const
	{
		if (ListItem.IsValid())
		{
			bool bEnabledIsSet = ListItem->IsItemEnabled.IsSet();
			return bEnabledIsSet ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Collapsed;
	}

	ECheckBoxState GetCheckBoxState() const
	{
		if (ListItem.IsValid())
		{
			bool bIsEnabled = ListItem->IsItemEnabled.Get(false);
			return bIsEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		return ECheckBoxState::Unchecked;
	}

	void OnCheckBoxStateChanged(ECheckBoxState NewState)
	{
		if (ListItem.IsValid())
		{
			const bool bNewEnabledState = NewState == ECheckBoxState::Checked ? true : false;
			ListItem->OnItemEnabledChanged.ExecuteIfBound(ListItem, bNewEnabledState);
		}
	}

private:
	FColorGradingListItemRef ListItem;
};

void SColorGradingObjectList::Construct(const FArguments& InArgs)
{
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;

	RefreshList();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ListView, SListView<FColorGradingListItemRef>)
			.ListItemsSource(InArgs._ColorGradingItemsSource)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SColorGradingObjectList::GenerateListItemRow)
			.OnSelectionChanged(this, &SColorGradingObjectList::OnSelectionChanged)
			.HeaderRow
			(
				SNew(SHeaderRow)

				+ SHeaderRow::Column(ColorGradingObjectListColumnNames::ItemEnabled)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)
				.DefaultTooltip(LOCTEXT("ItemEnabledColumnTooltip", "Color Grading Enabled"))

				+ SHeaderRow::Column(ColorGradingObjectListColumnNames::ItemLabel)
				.DefaultLabel(LOCTEXT("ColorGradingPanelList_ItemLabelHeader", "Item Label"))
				.FillWidth(1.0f)
			)
		]
	];
}

void SColorGradingObjectList::RefreshList()
{
	if (ListView.IsValid())
	{
		ListView->RebuildList();
	}
}

TArray<FColorGradingListItemRef> SColorGradingObjectList::GetSelectedItems()
{
	return ListView->GetSelectedItems();
}

void SColorGradingObjectList::SetSelectedItems(const TArray<FColorGradingListItemRef>& InSelectedItems)
{
	ListView->ClearSelection();
	ListView->SetItemSelection(InSelectedItems, true);
}

TSharedRef<ITableRow> SColorGradingObjectList::GenerateListItemRow(FColorGradingListItemRef Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SColorGradingListItemRow, OwnerTable, Item);
}

void SColorGradingObjectList::OnSelectionChanged(FColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo)
{
	OnSelectionChangedDelegate.ExecuteIfBound(SharedThis<SColorGradingObjectList>(this), SelectedItem, SelectInfo);
}

#undef LOCTEXT_NAMESPACE