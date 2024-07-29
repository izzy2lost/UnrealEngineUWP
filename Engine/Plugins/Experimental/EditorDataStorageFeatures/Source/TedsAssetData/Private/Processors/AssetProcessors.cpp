// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/AssetProcessors.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "TedsAssetDataColumns.h"

void UTedsAssetDataFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) 
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementLabelColumn, FTypedElementClassTypeInfoColumn, FAssetPathColumn_Experimental,
			FAssetTag, FDiskSizeColumn, FVersePathColumn>(),
		FName("Editor_PlaceholderAssetTable"));
}
