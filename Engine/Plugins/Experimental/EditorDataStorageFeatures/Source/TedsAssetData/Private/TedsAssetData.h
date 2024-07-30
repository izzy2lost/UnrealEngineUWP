// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/StringFwd.h"
#include "Elements/Common/TypedElementHandles.h"

struct FAssetData;

class ITypedElementDataStorageInterface;

namespace UE::EditorDataStorage::AssetData::Private
{

/**
 * Manage the registration and life cycle of the row related representing the data from the asset registry into TEDS.
 */
class FTedsAssetData
{
public:
	FTedsAssetData(ITypedElementDataStorageInterface& InDatabase);

	~FTedsAssetData();

	void ProcessAllEvents();

private:
	void OnAssetsAdded(TConstArrayView<FAssetData> InAssetsAdded);
	void OnAssetsRemoved(TConstArrayView<FAssetData> InAssetsRemoved);
	void OnAssetsUpdated(TConstArrayView<FAssetData> InAssetsUpdated);
	void OnAssetsUpdatedOnDisk(TConstArrayView<FAssetData> InAssetsUpdated);

	void OnAssetRenamed(const FAssetData& InAsset, const FString& InOldObjectPath);

	void OnPathsAdded(TConstArrayView<FStringView> InPathsAdded);
	void OnPathsRemoved(TConstArrayView<FStringView> InPathsRemoved);

	ITypedElementDataStorageInterface& Database;
	TypedElementDataStorage::TableHandle PathsTable = TypedElementDataStorage::InvalidTableHandle;
	TypedElementDataStorage::TableHandle AssetsDataTable = TypedElementDataStorage::InvalidTableHandle;

	TypedElementDataStorage::QueryHandle UpdateAssetsInPathQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle UpdateAssetsInPath = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle ResolveMissingAssetInPathQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle UpdateParentToChildrenAssetPathQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle ResolveMissingParentPathQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle RemoveUpdatedPathTagQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle RemoveUpdatedAssetDataTagQuery = TypedElementDataStorage::InvalidQueryHandle;
};

}
