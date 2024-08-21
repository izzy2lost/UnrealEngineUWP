// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "TypeInfoWidget.generated.h"

struct FSlateBrush;
struct FTypedElementClassTypeInfoColumn;

UCLASS()
class UTypeInfoWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypeInfoWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct TEDSOUTLINER_API FTypeInfoWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypeInfoWidgetConstructor();
	~FTypeInfoWidgetConstructor() override = default;

protected:
	explicit FTypeInfoWidgetConstructor(const UScriptStruct* InTypeInfo);
	TSharedPtr<SWidget> CreateWidget(const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		UE::Editor::DataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;

protected:

	// Whether the widget created by this constructor should be icon or text
	bool bUseIcon;
};
