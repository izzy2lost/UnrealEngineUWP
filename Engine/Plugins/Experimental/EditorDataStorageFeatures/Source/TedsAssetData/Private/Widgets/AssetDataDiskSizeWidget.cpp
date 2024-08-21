// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetDataDiskSizeWidget.h"

#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Internationalization/Text.h"
#include "TedsAssetDataColumns.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/Text/STextBlock.h"

void UDiskSizeWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
                                                        ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FDiskSizeWidgetConstructor>(TEXT("General.Cell"), TColumn<FDiskSizeColumn>());
}

FDiskSizeWidgetConstructor::FDiskSizeWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FDiskSizeWidgetConstructor::CreateWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	UE::Editor::DataStorage::FAttributeBinder Binder(TargetRow, DataStorage);
	
	return SNew(STextBlock)
			.Text(Binder.BindData(&FDiskSizeColumn::DiskSize, [](int64 DiskSize)
				{
					return FText::AsMemory(DiskSize);
				}));
			
}
