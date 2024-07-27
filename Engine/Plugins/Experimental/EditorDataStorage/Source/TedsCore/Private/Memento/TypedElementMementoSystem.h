// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Elements/Common/TypedElementHandles.h"

class ITypedElementDataStorageInterface;
class UTypedElementDatabase;
class UTypedElementMementoTranslatorBase;
	
class UTypedElementMementoSystem
{
public:
	explicit UTypedElementMementoSystem(ITypedElementDataStorageInterface& InDataStorage);
	
	TypedElementDataStorage::RowHandle CreateMemento(TypedElementDataStorage::RowHandle SourceRow);
	void CreateMemento(TypedElementDataStorage::RowHandle ReservedMementoRow, TypedElementDataStorage::RowHandle SourceRow);
	void RestoreMemento(TypedElementDataStorage::RowHandle MementoRow,  TypedElementDataStorage::RowHandle TargetRow);
	void DestroyMemento(TypedElementDataStorage::RowHandle MementoRow);

private:
	void CreateMementoInternal(TypedElementDataStorage::RowHandle MementoRow, TypedElementDataStorage::RowHandle SourceRow);

	TArray<const UTypedElementMementoTranslatorBase*> MementoTranslators;
	TypedElementDataStorage::TableHandle MementoRowBaseTable;
	ITypedElementDataStorageInterface& DataStorage;
};
