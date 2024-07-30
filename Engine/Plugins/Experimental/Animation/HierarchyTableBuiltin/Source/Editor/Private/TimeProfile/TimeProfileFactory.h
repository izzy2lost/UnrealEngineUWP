// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "TimeProfileFactory.generated.h"

UCLASS()
class UHierarchyTableTimeFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};