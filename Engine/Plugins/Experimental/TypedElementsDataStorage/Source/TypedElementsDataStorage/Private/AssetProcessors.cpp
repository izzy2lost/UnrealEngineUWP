// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetProcessors.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "TypedElementAssetColumns.h"

void UTypedElementAssetFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) 
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementLabelColumn, FTypedElementClassTypeInfoColumn,
			FAssetTag, FDiskSizeColumn, FVersePathColumn>(),
		FName("Editor_PlaceholderAssetTable"));
}
