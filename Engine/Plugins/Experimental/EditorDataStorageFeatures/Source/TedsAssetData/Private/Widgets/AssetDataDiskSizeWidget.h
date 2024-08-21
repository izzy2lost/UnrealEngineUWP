// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "AssetDataDiskSizeWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class UDiskSizeWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UDiskSizeWidgetFactory() override = default;

	TEDSASSETDATA_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

// Widget to show disk size in bytes
USTRUCT()
struct FDiskSizeWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSASSETDATA_API FDiskSizeWidgetConstructor();
	~FDiskSizeWidgetConstructor() override = default;

	TEDSASSETDATA_API virtual TSharedPtr<SWidget> CreateWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		RowHandle TargetRow,
		RowHandle WidgetRow, 
		const TypedElementDataStorage::FMetaDataView& Arguments) override;
};