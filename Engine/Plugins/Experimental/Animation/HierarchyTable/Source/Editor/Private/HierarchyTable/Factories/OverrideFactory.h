// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "OverrideFactory.generated.h"

UCLASS()
class UHierarchyTableOverrideFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};