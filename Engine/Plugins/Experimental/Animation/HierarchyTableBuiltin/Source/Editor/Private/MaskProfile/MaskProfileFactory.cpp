// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaskProfile/MaskProfileFactory.h"
#include "MaskProfile/MaskProfileProxyColumn.h"
#include "MaskProfile/MaskProfileWidgetConstructor.h"

void UHierarchyTableMaskFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	DataStorageUi.RegisterWidgetFactory<FHierarchyTableMaskWidgetConstructor_Value>(FName(TEXT("General.Cell")), FColumn<FHierarchyTableMaskColumn_Value>());
}