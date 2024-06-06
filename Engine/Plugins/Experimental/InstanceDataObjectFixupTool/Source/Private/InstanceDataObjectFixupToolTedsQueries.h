// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "InstanceDataObjectFixupToolTedsQueries.generated.h"

UCLASS()
class UInstanceDataObjectFixupToolTedsQueryFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UInstanceDataObjectFixupToolTedsQueryFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	static void ShowFixUpToolForPlaceholders(TypedElementDataStorage::RowHandle Row);
	static void ShowFixUpToolForLooseProperties(TypedElementDataStorage::RowHandle Row);
	static void ShowFixUpTool(TypedElementDataStorage::RowHandle Row, bool bRecurseIntoObject);
};
