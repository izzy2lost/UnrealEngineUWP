// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "GlobalLock.h"


namespace UE::Editor::DataStorage
{
	/**
	 * Storage for an index to row mapping.
	 * Access to the index table is thread safe and guarded by the global lock.
	 */

	class FIndexTable final
	{
	public:
		TypedElementDataStorage::RowHandle FindIndexedRow(
			EGlobalLockScope LockScope,
			TypedElementDataStorage::IndexHash Index) const;
		void IndexRow(
			EGlobalLockScope LockScope,
			TypedElementDataStorage::IndexHash Index, 
			TypedElementDataStorage::RowHandle Row);
		void BatchIndexRows(
			EGlobalLockScope LockScope,
			TConstArrayView<TPair<TypedElementDataStorage::IndexHash, 
			TypedElementDataStorage::RowHandle>> IndexRowPairs);
		void ReindexRow(
			EGlobalLockScope LockScope,
			TypedElementDataStorage::IndexHash OriginalIndex,
			TypedElementDataStorage::IndexHash NewIndex,
			TypedElementDataStorage::RowHandle Row);
		void RemoveIndex(
			EGlobalLockScope LockScope,
			TypedElementDataStorage::IndexHash Index);
		void RemoveRow(
			EGlobalLockScope LockScope,
			TypedElementDataStorage::RowHandle Row);

	private:
		TMap<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle> IndexLookupMap;
		TMultiMap<TypedElementDataStorage::RowHandle, TypedElementDataStorage::IndexHash> ReverseIndexLookupMap;
	
		void IndexRowUnguarded(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row);
		void RemoveIndexUnguarded(TypedElementDataStorage::IndexHash Index);
	};
} // namespace UE::Editor::DataStorage
