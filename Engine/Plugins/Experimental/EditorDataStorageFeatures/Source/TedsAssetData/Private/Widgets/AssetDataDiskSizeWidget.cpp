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
	DataStorageUi.RegisterWidgetFactory<FDiskSizeWidgetConstructor>(TEXT("General.Cell"), TypedElementDataStorage::FColumn<FDiskSizeColumn>());
}

FDiskSizeWidgetConstructor::FDiskSizeWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FDiskSizeWidgetConstructor::CreateWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, RowHandle TargetRow, RowHandle WidgetRow,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	UE::EditorDataStorage::FAttributeBinder Binder(TargetRow, DataStorage);
	
	return SNew(STextBlock)
			.Text(Binder.BindData(&FDiskSizeColumn::DiskSize, [](int64 DiskSize)
				{
					return FText::AsMemory(DiskSize);
				}));
			
}
