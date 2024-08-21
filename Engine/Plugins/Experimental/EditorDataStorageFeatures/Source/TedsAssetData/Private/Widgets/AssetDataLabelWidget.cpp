// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetDataLabelWidget.h"

#include "TedsAssetDataColumns.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FAssetDataLabelWidgetConstructor"

void UAssetDataLabelWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
                                                              ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory<FAssetDataLabelWidgetConstructor>(
		TEXT("General.RowLabel"),
		TypedElementDataStorage::FColumn<FItemNameColumn_Experimental>() && TypedElementDataStorage::FColumn<FAssetTag>());
}

FAssetDataLabelWidgetConstructor::FAssetDataLabelWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}


TSharedPtr<SWidget> FAssetDataLabelWidgetConstructor::CreateWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	if (DataStorage->IsRowAvailable(TargetRow))
	{
		UE::Editor::DataStorage::FAttributeBinder Binder(TargetRow, DataStorage);
		
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.AutoWidth()
			[
				// TODO: Asset color in TEDS
				SNew(SImage)
					.Image(FAppStyle::GetBrush(FName(TEXT("ContentBrowser.ColumnViewAssetIcon"))))
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
				.Text(Binder.BindText(&FItemNameColumn_Experimental::Name))
				.ToolTipText_Lambda([DataStorage, DataRow = TargetRow]()
					{
						return FAssetDataLabelWidgetConstructor::ConstructToolTip(DataStorage, DataRow);
					})
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

FText FAssetDataLabelWidgetConstructor::ConstructToolTip(
	ITypedElementDataStorageInterface* DataStorage, UE::Editor::DataStorage::RowHandle DataRow)
{
	TStringBuilder<1024> ToolTip;
	if (FItemNameColumn_Experimental* ItemName = DataStorage->GetColumn<FItemNameColumn_Experimental>(DataRow))
	{
		ToolTip.Append(ItemName->Name.ToString());
		ToolTip.AppendChar(TEXT('\n'));
	}
	if (FAssetPathColumn_Experimental* AssetPath = DataStorage->GetColumn<FAssetPathColumn_Experimental>(DataRow))
	{
		ToolTip.AppendChar(TEXT('\n'));
		ToolTip.Append(LOCTEXT("AssetPath", "Asset path: ").ToString());
		ToolTip.Append(AssetPath->Path.ToString());
	}
	if (FVersePathColumn* VersePath = DataStorage->GetColumn<FVersePathColumn>(DataRow))
	{
		ToolTip.AppendChar(TEXT('\n'));
		ToolTip.Append(LOCTEXT("VersePath", "Verse path: ").ToString());
		ToolTip.Append(VersePath->VersePath.AsStringView());
	}
	if (FVirtualPathColumn_Experimental* VirtualPath = DataStorage->GetColumn<FVirtualPathColumn_Experimental>(DataRow))
	{
		ToolTip.AppendChar(TEXT('\n'));
		ToolTip.Append(LOCTEXT("VirtualPath", "Virtual path: ").ToString());
		ToolTip.Append(VirtualPath->VirtualPath.ToString());
	}
	return FText::FromString(ToolTip.ToString());
}

#undef LOCTEXT_NAMESPACE