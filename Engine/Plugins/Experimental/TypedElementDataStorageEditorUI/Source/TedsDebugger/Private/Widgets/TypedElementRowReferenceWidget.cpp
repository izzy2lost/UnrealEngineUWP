// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementRowReferenceWidget.h"

#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RowReferenceWidget"

UTypedElementRowReferenceWidgetFactory::~UTypedElementRowReferenceWidgetFactory()
{
}

void UTypedElementRowReferenceWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	// TEDS UI TODO: We can re-use this widget for FTypedElementParentColumn
	DataStorageUi.RegisterWidgetFactory<FTypedElementRowReferenceWidgetConstructor>(FName(TEXT("SceneOutliner.Cell")),
	TypedElementDataStorage::FColumn<FTypedElementRowReferenceColumn>());
}

FTypedElementRowReferenceWidgetConstructor::FTypedElementRowReferenceWidgetConstructor()
	: Super(FTypedElementRowReferenceWidgetConstructor::StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementRowReferenceWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(STextBlock);
}

bool FTypedElementRowReferenceWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	checkf(Widget, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));

	// The actual row we want to view in the widget
	TypedElementRowHandle TargetRowReference = TypedElementInvalidRowHandle;

	// The target row for which this widget was created
	TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;

	// Check if the target row has a row reference column, if so we want to view the row in there.
	if(FTypedElementRowReferenceColumn* RowReferenceColumn = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(TargetRow))
	{
		TargetRowReference = RowReferenceColumn->Row;
	}
	
	if (const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRowReference))
	{
		checkf(Widget->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FTypedElementLabelWidgetConstructor doesn't match type %s, but was a %s."),
		*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
		*(Widget->GetTypeAsString()));

		STextBlock* WidgetInstance = static_cast<STextBlock*>(Widget.Get());

		FNumberFormattingOptions NumberFormattingOptions;
		NumberFormattingOptions.SetUseGrouping(false);

		// TEDS Debugger TODO: Simply displaying the row handle and label for now. Eventually we want a clickable link to open the row in the TEDS debugger
		const FText Text = FText::Format(LOCTEXT("RowReference", "Row Handle: {0}, Label: {1}"), FText::AsNumber(TargetRowReference, &NumberFormattingOptions), FText::FromString(LabelColumn->Label));
			
		WidgetInstance->SetText(Text);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE