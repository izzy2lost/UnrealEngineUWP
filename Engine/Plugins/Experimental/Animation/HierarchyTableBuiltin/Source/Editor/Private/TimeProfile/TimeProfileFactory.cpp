// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeProfile/TimeProfileFactory.h"
#include "TimeProfile/TimeProfileProxyColumn.h"
#include "TimeProfile/TimeProfileWidgetConstructor.h"

void UHierarchyTableTimeFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	DataStorageUi.RegisterWidgetFactory<FHierarchyTableTimeWidgetConstructor_StartTime>(FName(TEXT("General.Cell")), FColumn<FHierarchyTableTimeColumn_StartTime>());
	DataStorageUi.RegisterWidgetFactory<FHierarchyTableTimeWidgetConstructor_EndTime>(FName(TEXT("General.Cell")), FColumn<FHierarchyTableTimeColumn_EndTime>());
	DataStorageUi.RegisterWidgetFactory<FHierarchyTableTimeWidgetConstructor_TimeFactor>(FName(TEXT("General.Cell")), FColumn<FHierarchyTableTimeColumn_TimeFactor>());
	DataStorageUi.RegisterWidgetFactory<FHierarchyTableTimeWidgetConstructor_Preview>(FName(TEXT("General.Cell")), FColumn<FHierarchyTableTimeColumn_Preview>());
}