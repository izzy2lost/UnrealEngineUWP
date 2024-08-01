// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TransformHeadsUpWidget.generated.h"

/**
 * The heads up transform display provides at a glance view in a scene outliner row of abnormal transform characteristics, including:
 *		1. Non-uniform scale
 *		2. Negative scaling on X, Y, or Z axis
 *		3. Unnormalized rotation
 */
UCLASS()
class UTransformHeadsUpWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTransformHeadsUpWidgetFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct FTransformHeadsUpWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTransformHeadsUpWidgetConstructor();
	~FTransformHeadsUpWidgetConstructor() override = default;

	TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row,
		const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT(meta = (DisplayName = "Heads up display for transforms widget"))
struct FTransformHeadsUpWidgetTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};
