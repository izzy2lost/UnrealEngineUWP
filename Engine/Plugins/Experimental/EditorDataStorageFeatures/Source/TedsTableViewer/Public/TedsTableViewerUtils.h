// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/Class.h"

#include "TedsTableViewerUtils.generated.h"

namespace TypedElementDataStorage
{
	class FMetaDataView;
}
class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;
struct FTypedElementWidgetConstructor;

// Util library for functions shared by the Teds Table Viewer and the Teds Outliner
namespace UE::EditorDataStorage::TableViewerUtils
{
	TEDSTABLEVIEWER_API FName GetWidgetTableName();
	
	// Find the longest matching common column name given a list of columns
	TEDSTABLEVIEWER_API FName FindLongestMatchingName(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, int32 DefaultNameIndex);

	// Create a header widget constructor for the given columns
	TEDSTABLEVIEWER_API TSharedPtr<FTypedElementWidgetConstructor> CreateHeaderWidgetConstructor(ITypedElementDataStorageUiInterface& StorageUi, 
	const TypedElementDataStorage::FMetaDataView& InMetaData, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes,
	const TConstArrayView<FName> CellWidgetPurposes);

	// Create a copy of the provided column types array after discarding invalid entries
	TEDSTABLEVIEWER_API TArray<TWeakObjectPtr<const UScriptStruct>> CreateVerifiedColumnTypeArray(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes);
}

UCLASS()
class UTypedElementTableViewerFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementTableViewerFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;
};

