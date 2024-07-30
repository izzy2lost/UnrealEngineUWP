// Copyright Epic Games, Inc. All Rights Reserved.

#include "HierarchyTable/Widgets/OverrideWidgetConstructor.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "HierarchyTable/Columns/OverrideColumn.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "HierarchyTable.h"

FTypedElementWidgetConstructor_Override::FTypedElementWidgetConstructor_Override()
	: Super(StaticStruct())
{ 
}

TSharedPtr<SWidget> FTypedElementWidgetConstructor_Override::CreateWidget(
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center);
}

bool FTypedElementWidgetConstructor_Override::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementDataStorage::RowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	if (!Widget)
	{
		return true;
	}

	checkf(Widget->GetType() == SBox::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FTypedElementFloatWidgetConstructor doesn't match type %s, but was a %s."),
		*(SBox::StaticWidgetClass().GetWidgetType().ToString()),
		*(Widget->GetTypeAsString()));

	TSharedPtr<SBox> WidgetInstance = StaticCastSharedPtr<SBox>(Widget);

	// Row is not actually the row we want, its contained inside of a row reference, I don't know why things are done this way.
	const TypedElementDataStorage::RowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;
	FTypedElementOverrideColumn* OverrideColumn = DataStorage->GetColumn<FTypedElementOverrideColumn>(TargetRow);

	if (OverrideColumn == nullptr)
	{
		check(false);
		WidgetInstance->SetContent(SNullWidget::NullWidget);

		return true;
	}

	const bool bHasParent = OverrideColumn->OwnerEntry->HasParent();
	FHierarchyTableEntryData* EntryData = OverrideColumn->OwnerEntry; 

	TSharedRef<SWidget> NewWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsEnabled(bHasParent)
			.OnClicked_Lambda([EntryData]()
				{
					EntryData->ToggleOverridden();
					return FReply::Handled();
				})
			.ContentPadding(0.0f)
				[
					SNew(SImage)
					.Image_Lambda([EntryData]()
						{
							const bool bHasOverriddenChildren = EntryData->HasOverriddenChildren();

							if (EntryData->IsOverridden())
							{
								if (bHasOverriddenChildren)
								{
									return FAppStyle::GetBrush(TEXT("DetailsView.OverrideHereInside"));
								}
								else
								{
									return FAppStyle::GetBrush(TEXT("DetailsView.OverrideHere"));
								}
							}
							else
							{
								if (bHasOverriddenChildren)
								{
									return FAppStyle::GetBrush(TEXT("DetailsView.OverrideInside"));
								}
								else
								{
									return FAppStyle::GetBrush(TEXT("DetailsView.OverrideNone"));
								}
							}
						})
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
		];

	WidgetInstance->SetContent(NewWidget);

	return true;
}