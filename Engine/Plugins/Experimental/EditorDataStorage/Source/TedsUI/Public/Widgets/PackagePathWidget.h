// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "PackagePathWidget.generated.h"

class ITypedElementDataStorageInterface;
class SWidget;
class UScriptStruct;

UCLASS()
class TEDSUI_API UPackagePathWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UPackagePathWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct TEDSUI_API FPackagePathWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FPackagePathWidgetConstructor();
	~FPackagePathWidgetConstructor() override = default;

protected:
	explicit FPackagePathWidgetConstructor(const UScriptStruct* InTypeInfo);

	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT()
struct TEDSUI_API FLoadedPackagePathWidgetConstructor : public FPackagePathWidgetConstructor
{
	GENERATED_BODY()

public:
	FLoadedPackagePathWidgetConstructor();
	~FLoadedPackagePathWidgetConstructor() override = default;

protected:
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};
