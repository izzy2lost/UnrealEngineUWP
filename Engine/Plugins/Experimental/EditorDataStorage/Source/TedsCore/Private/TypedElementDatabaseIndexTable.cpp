// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseIndexTable.h"

namespace UE::Editor::DataStorage
{
	TypedElementDataStorage::RowHandle FIndexTable::FindIndexedRow(
		EGlobalLockScope LockScope, TypedElementDataStorage::IndexHash Index) const
	{
		using namespace TypedElementDataStorage;
	
		FScopedSharedLock Lock(LockScope);
	
		const RowHandle* Result = IndexLookupMap.Find(Index);
		return Result ? *Result : InvalidRowHandle;
	}

	void FIndexTable::BatchIndexRows(EGlobalLockScope LockScope,
		TConstArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexRowPairs)
	{
		using namespace TypedElementDataStorage;

		FScopedExclusiveLock Lock(LockScope);
	
		IndexLookupMap.Reserve(IndexLookupMap.Num() + IndexRowPairs.Num());
		ReverseIndexLookupMap.Reserve(ReverseIndexLookupMap.Num() + IndexRowPairs.Num());

		for (const TPair<IndexHash, RowHandle>& IndexAndRow : IndexRowPairs)
		{
			IndexRowUnguarded(IndexAndRow.Key, IndexAndRow.Value);
		}
	}

	void FIndexTable::IndexRow(EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
	{

		FScopedExclusiveLock Lock(LockScope);
		IndexRowUnguarded(Index, Row);
	}

	void FIndexTable::ReindexRow(EGlobalLockScope LockScope,
		TypedElementDataStorage::IndexHash OriginalIndex, TypedElementDataStorage::IndexHash NewIndex, 
		TypedElementDataStorage::RowHandle Row)
	{
		FScopedExclusiveLock Lock(LockScope);
	
		RemoveIndexUnguarded(OriginalIndex);
		IndexRowUnguarded(NewIndex, Row);
	}

	void FIndexTable::RemoveIndex(EGlobalLockScope LockScope, TypedElementDataStorage::IndexHash Index)
	{
		FScopedExclusiveLock Lock(LockScope);
		RemoveIndexUnguarded(Index);
	}

	void FIndexTable::RemoveRow(EGlobalLockScope LockScope, TypedElementDataStorage::RowHandle Row)
	{
		using namespace TypedElementDataStorage;

		FScopedExclusiveLock Lock(LockScope);
	
		if (TMultiMap<RowHandle, IndexHash>::TKeyIterator It = ReverseIndexLookupMap.CreateKeyIterator(Row); It)
		{
			do
			{
				IndexLookupMap.Remove(It.Value());
				++It;
			} while (It);
			ReverseIndexLookupMap.Remove(Row);
		}
	}

	void FIndexTable::IndexRowUnguarded(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
	{
		IndexLookupMap.Add(Index, Row);
		ReverseIndexLookupMap.Add(Row, Index);
	}


	void FIndexTable::RemoveIndexUnguarded(TypedElementDataStorage::IndexHash Index)
	{
		using namespace TypedElementDataStorage;

		if (const RowHandle* Row = IndexLookupMap.Find(Index))
		{
			IndexLookupMap.Remove(Index);
			ReverseIndexLookupMap.Remove(*Row, Index);
		}
	}
} // namespace UE::Editor::DataStorage
