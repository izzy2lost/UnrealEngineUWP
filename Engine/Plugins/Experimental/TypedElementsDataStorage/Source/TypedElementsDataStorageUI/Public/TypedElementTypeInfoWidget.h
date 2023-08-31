// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "TypedElementTypeInfoWidget.generated.h"

struct FSlateBrush;
struct FTypedElementClassTypeInfoColumn;

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementTypeInfoWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementTypeInfoWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementTypeInfoWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementTypeInfoWidgetConstructor();
	~FTypedElementTypeInfoWidgetConstructor() override = default;

protected:
	explicit FTypedElementTypeInfoWidgetConstructor(const UScriptStruct* InTypeInfo);
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;

private:

	// Get the icon for a given row, checking the cache to see if we already have one
	static const FSlateBrush* GetIconForRow(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const FTypedElementClassTypeInfoColumn* TypeInfoColumn);
	
private:

	// Cache to avoid looking up the icon for a class every time
	static TMap<FName, const FSlateBrush*> CachedIconMap;

	// Whether the widget created by this constructor should be icon or text
	bool bUseIcon;
};
