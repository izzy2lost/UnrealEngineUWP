// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetProcessors.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "TedsAssetDataColumns.h"

void UTypedElementAssetFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) 
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementLabelColumn, FTypedElementClassTypeInfoColumn, FAssetPathColumn_Experimental,
			FAssetTag, FDiskSizeColumn, FVersePathColumn>(),
		FName("Editor_PlaceholderAssetTable"));
}
