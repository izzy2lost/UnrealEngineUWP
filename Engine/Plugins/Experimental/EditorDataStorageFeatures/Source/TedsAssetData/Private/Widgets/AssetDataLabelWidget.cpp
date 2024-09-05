// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetDataLabelWidget.h"

#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "TedsAssetDataColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FAssetDataLabelWidgetConstructor"

void UAssetDataLabelWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
                                                              ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FAssetDataLabelWidgetConstructor>(
		TEXT("General.RowLabel"),
		TColumn<FNameColumn>() && (TColumn<FAssetTag>() || TColumn<FAssetPathColumn_Experimental>()));
}

FAssetDataLabelWidgetConstructor::FAssetDataLabelWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

FAssetDataLabelWidgetConstructor::FAssetDataLabelWidgetConstructor(const UScriptStruct* TypeInfo)
	: FSimpleWidgetConstructor(TypeInfo)
{
}

TSharedPtr<SWidget> FAssetDataLabelWidgetConstructor::CreateWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	if (DataStorage->IsRowAvailable(TargetRow))
	{
		UE::Editor::DataStorage::FAttributeBinder Binder(TargetRow, DataStorage);

		bool bIsAsset = DataStorage->HasColumns<FAssetDataColumn_Experimental>(TargetRow);

		TAttribute<FSlateColor> ColorAndOpacityAttribute;

		// For assets, grab the color from the asset definition
		if(bIsAsset)
		{
			ColorAndOpacityAttribute = Binder.BindData(&FAssetDataColumn_Experimental::AssetData, [](const FAssetData& AssetData)
			{
				if(const UAssetDefinition* AssetDefinition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForAsset(AssetData))
				{
					return FSlateColor(AssetDefinition->GetAssetColor());
				}

				return FSlateColor::UseForeground();
			});
		}
		// For folders, use the color column directly
		else
		{
			ColorAndOpacityAttribute = Binder.BindData(&FSlateColorColumn::Color, FSlateColor::UseForeground());
		}

		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.AutoWidth()
			[
				SNew(SImage)
					.Image(bIsAsset ? FAppStyle::GetBrush(FName(TEXT("ContentBrowser.ColumnViewAssetIcon"))) : FAppStyle::GetBrush(FName(TEXT("ContentBrowser.ColumnViewFolderIcon"))))
					.ColorAndOpacity(ColorAndOpacityAttribute)
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
				.Text(Binder.BindText(&FNameColumn::Name))
				.ToolTipText_Lambda([DataStorage, DataRow = TargetRow]()
					{
						return ConstructToolTip(DataStorage, DataRow);
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
	if (FNameColumn* ItemName = DataStorage->GetColumn<FNameColumn>(DataRow))
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