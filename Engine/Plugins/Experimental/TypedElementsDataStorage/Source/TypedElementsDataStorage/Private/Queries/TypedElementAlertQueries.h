// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Columns/TypedElementAlertColumns.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementAlertQueries.generated.h"

namespace TypedElementDataStorage
{
	struct IQueryContext;
}

/**
 * Calls to manage alerts, in particular child alerts.
 */
UCLASS()
class UTypedElementAlertQueriesFactory final : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementAlertQueriesFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterSubQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterParentUpdatesQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterChildAlertUpdatesQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOnAddQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOnRemoveQueries(ITypedElementDataStorageInterface& DataStorage);

	static void AddChildAlertsToHierarchy(
		TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Parent, int32 ParentQueryIndex);

	static void IncrementParents(
		TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementAlertColumnType AlertType,
		int32 ChildAlertQueryIndex);
	
	static void ResetChildAlertCounters(FTypedElementChildAlertColumn& ChildAlert);

	static bool MoveToNextParent(
		TypedElementDataStorage::RowHandle& Parent, TypedElementDataStorage::IQueryContext& Context, int32 SubQueryIndex);

	static const FName AlertConditionName;
	TypedElementDataStorage::QueryHandle ChildAlertColumnReadWriteQuery;
	TypedElementDataStorage::QueryHandle ParentReadOnlyQuery;
};

