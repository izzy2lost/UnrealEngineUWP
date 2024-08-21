// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "AssetDataLabelWidget.generated.h"

class ITypedElementDataStorageInterface;
class UScriptStruct;

UCLASS()
class UAssetDataLabelWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UAssetDataLabelWidgetFactory() override = default;

	TEDSASSETDATA_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

// Label widget for assets in TEDS
USTRUCT()
struct FAssetDataLabelWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSASSETDATA_API FAssetDataLabelWidgetConstructor();
	~FAssetDataLabelWidgetConstructor() override = default;

	TEDSASSETDATA_API virtual TSharedPtr<SWidget> CreateWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		RowHandle TargetRow,
		RowHandle WidgetRow, 
		const TypedElementDataStorage::FMetaDataView& Arguments) override;

protected:
	
	static FText ConstructToolTip(ITypedElementDataStorageInterface* DataStorage, UE::Editor::DataStorage::RowHandle DataRow);
};