// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UObjectLabelWidget.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "TedsTableViewerUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "FUObjectLabelWidgetConstructor"

void UUObjectLabelWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory<FUObjectLabelWidgetConstructor>(
		TEXT("General.RowLabel"),
		TypedElementDataStorage::FColumn<FTypedElementLabelColumn>() && TypedElementDataStorage::FColumn<FTypedElementUObjectColumn>());
}

FUObjectLabelWidgetConstructor::FUObjectLabelWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FUObjectLabelWidgetConstructor::CreateWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, RowHandle TargetRow, RowHandle WidgetRow,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	if(DataStorage->IsRowAvailable(TargetRow))
	{
		UE::EditorDataStorage::FAttributeBinder Binder(TargetRow, DataStorage);

		// Once TEDS UI has widget combining functionality, the binder can be used to create the type info widget and label widget and combine them
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.AutoWidth()
			[
				SNew(SImage)
					.Image(UE::Editor::DataStorage::TableViewerUtils::GetIconForRow(DataStorage, TargetRow))
					.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+SHorizontalBox::Slot()
				.AutoWidth()
			[
				SNew(SSpacer)
					.Size(FVector2D(5.0f, 0.0f))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1.0f)
			[
				SNew(STextBlock)
					.Text(Binder.BindText(&FTypedElementLabelColumn::Label))
					.ToolTipText(Binder.BindText(&FTypedElementLabelColumn::Label))
			];
	}
	else
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("MissingRowReferenceColumn", "Unable to retrieve row reference."));
	}
	
	
}

#undef LOCTEXT_NAMESPACE // "FUObjectLabelWidgetConstructor"
