// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseIndexTable.h"

TypedElementDataStorage::RowHandle FTypedElementDatabaseIndexTable::FindIndexedRow(
	UE::EditorDataStorage::EGlobalLockScope LockScope, TypedElementDataStorage::IndexHash Index) const
{
	using namespace TypedElementDataStorage;
	using namespace UE::EditorDataStorage;
	
	FScopedSharedLock Lock(LockScope);
	
	const RowHandle* Result = IndexLookupMap.Find(Index);
	return Result ? *Result : InvalidRowHandle;
}

void FTypedElementDatabaseIndexTable::BatchIndexRows(UE::EditorDataStorage::EGlobalLockScope LockScope,
	TConstArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexRowPairs)
{
	using namespace TypedElementDataStorage;
	using namespace UE::EditorDataStorage;

	FScopedExclusiveLock Lock(LockScope);
	
	IndexLookupMap.Reserve(IndexLookupMap.Num() + IndexRowPairs.Num());
	ReverseIndexLookupMap.Reserve(ReverseIndexLookupMap.Num() + IndexRowPairs.Num());

	for (const TPair<IndexHash, RowHandle>& IndexAndRow : IndexRowPairs)
	{
		IndexRowUnguarded(IndexAndRow.Key, IndexAndRow.Value);
	}
}

void FTypedElementDatabaseIndexTable::IndexRow(UE::EditorDataStorage::EGlobalLockScope LockScope,
	TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
{
	using namespace UE::EditorDataStorage;

	FScopedExclusiveLock Lock(LockScope);
	IndexRowUnguarded(Index, Row);
}

void FTypedElementDatabaseIndexTable::ReindexRow(UE::EditorDataStorage::EGlobalLockScope LockScope,
	TypedElementDataStorage::IndexHash OriginalIndex, TypedElementDataStorage::IndexHash NewIndex, 
	TypedElementDataStorage::RowHandle Row)
{
	using namespace UE::EditorDataStorage;

	FScopedExclusiveLock Lock(LockScope);
	
	RemoveIndexUnguarded(OriginalIndex);
	IndexRowUnguarded(NewIndex, Row);
}

void FTypedElementDatabaseIndexTable::RemoveIndex(UE::EditorDataStorage::EGlobalLockScope LockScope, TypedElementDataStorage::IndexHash Index)
{
	using namespace UE::EditorDataStorage;

	FScopedExclusiveLock Lock(LockScope);
	RemoveIndexUnguarded(Index);
}

void FTypedElementDatabaseIndexTable::RemoveRow(UE::EditorDataStorage::EGlobalLockScope LockScope, TypedElementDataStorage::RowHandle Row)
{
	using namespace TypedElementDataStorage;
	using namespace UE::EditorDataStorage;

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

void FTypedElementDatabaseIndexTable::IndexRowUnguarded(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
{
	IndexLookupMap.Add(Index, Row);
	ReverseIndexLookupMap.Add(Row, Index);
}


void FTypedElementDatabaseIndexTable::RemoveIndexUnguarded(TypedElementDataStorage::IndexHash Index)
{
	using namespace TypedElementDataStorage;

	if (const RowHandle* Row = IndexLookupMap.Find(Index))
	{
		IndexLookupMap.Remove(Index);
		ReverseIndexLookupMap.Remove(*Row, Index);
	}
}