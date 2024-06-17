// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "GlobalLock.h"

/**
 * Storage for an index to row mapping.
 * Access to the index table is thread safe and guarded by the global lock.
 */

class FTypedElementDatabaseIndexTable final
{
public:
	TypedElementDataStorage::RowHandle FindIndexedRow(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash Index) const;
	void IndexRow(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash Index, 
		TypedElementDataStorage::RowHandle Row);
	void BatchIndexRows(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TConstArrayView<TPair<TypedElementDataStorage::IndexHash, 
		TypedElementDataStorage::RowHandle>> IndexRowPairs);
	void ReindexRow(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash OriginalIndex,
		TypedElementDataStorage::IndexHash NewIndex,
		TypedElementDataStorage::RowHandle Row);
	void RemoveIndex(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash Index);
	void RemoveRow(
		UE::EditorDataStorage::EGlobalLockScope LockScope,
		TypedElementDataStorage::RowHandle Row);

private:
	TMap<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle> IndexLookupMap;
	TMultiMap<TypedElementDataStorage::RowHandle, TypedElementDataStorage::IndexHash> ReverseIndexLookupMap;
	
	void IndexRowUnguarded(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row);
	void RemoveIndexUnguarded(TypedElementDataStorage::IndexHash Index);
};
