// Copyright Epic Games, Inc. All Rights Reserved.

#include "HierarchyTable/Factories/OverrideFactory.h"
#include "HierarchyTable/Columns/OverrideColumn.h"
#include "HierarchyTable/Widgets/OverrideWidgetConstructor.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

void UHierarchyTableOverrideFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	DataStorageUi.RegisterWidgetFactory<FTypedElementWidgetConstructor_Override>(FName(TEXT("General.Cell")), FColumn<FTypedElementOverrideColumn>());
}