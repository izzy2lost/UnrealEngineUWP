// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <variant>
#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Logging/LogMacros.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDatabaseUI.generated.h"

class ITypedElementDataStorageInterface;
class ITypedElementDataStorageCompatibilityInterface;

TYPEDELEMENTSDATASTORAGE_API DECLARE_LOG_CATEGORY_EXTERN(LogTypedElementDatabaseUI, Log, All);

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabaseUi final
	: public UObject
	, public ITypedElementDataStorageUiInterface
{
	GENERATED_BODY()

public:
	~UTypedElementDatabaseUi() override = default;

	void Initialize(
		ITypedElementDataStorageInterface* StorageInterface, 
		ITypedElementDataStorageCompatibilityInterface* StorageCompatibilityInterface);
	void Deinitialize();

	void RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description) override;

	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor,
		TArray<TWeakObjectPtr<const UScriptStruct>> Columns) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor,
		TArray<TWeakObjectPtr<const UScriptStruct>> Columns) override;

	void CreateWidgetConstructors(FName Purpose,
		TypedElementDataStorage::FMetaDataView Arguments, const WidgetConstructorCallback& Callback) override;
	void CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		TypedElementDataStorage::FMetaDataView Arguments, const WidgetConstructorCallback& Callback) override;

	void ConstructWidgets(FName Purpose, TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetCreatedCallback& ConstructionCallback) override;
	TSharedPtr<SWidget> ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
		TypedElementDataStorage::FMetaDataView Arguments) override;

	void ListWidgetPurposes(const WidgetPurposeCallback& Callback) const override;

private:
	struct FWidgetFactory
	{
		using ConstructorType = std::variant<const UScriptStruct*, TUniquePtr<FTypedElementWidgetConstructor>>;

		TArray<TWeakObjectPtr<const UScriptStruct>> Columns;
		ConstructorType Constructor;

		FWidgetFactory() = default;
		explicit FWidgetFactory(const UScriptStruct* InConstructor);
		explicit FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor);
		FWidgetFactory(const UScriptStruct* InConstructor, TArray<TWeakObjectPtr<const UScriptStruct>>&& InColumns);
		FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor, TArray<TWeakObjectPtr<const UScriptStruct>>&& InColumns);
	};

	struct FPurposeInfo
	{	
		TArray<FWidgetFactory> Factories;
		FText Description;
		EPurposeType Type;
		bool bIsSorted{ false }; //< Whether or not the array of factories needs to be sorted. The factories themselves are already sorted.
	};

	void CreateStandardArchetypes();

	bool CreateSingleWidgetConstructor(
		const FWidgetFactory::ConstructorType& Constructor,
		TypedElementDataStorage::FMetaDataView Arguments,
		TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes,
		const WidgetConstructorCallback& Callback);

	void CreateWidgetInstanceFromDescription(
		const UScriptStruct* Target,
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetInstanceFromInstance(
		FTypedElementWidgetConstructor* SourceConstructor,
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetInstance(
		FTypedElementWidgetConstructor& Constructor,
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	static bool PrepareColumnsList(TArray<TWeakObjectPtr<const UScriptStruct>>& Columns);

	void CreateWidgetConstructors_LongestMatch(
		const TArray<FWidgetFactory>& WidgetFactories, 
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, 
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_ExactMatch(
		const TArray<FWidgetFactory>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_SingleMatch(
		const TArray<FWidgetFactory>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		TypedElementDataStorage::FMetaDataView Arguments,
		const WidgetConstructorCallback& Callback);

	TypedElementTableHandle WidgetTable{ TypedElementInvalidTableHandle };
	
	TMap<FName, FPurposeInfo> WidgetPurposes;
	
	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };
};