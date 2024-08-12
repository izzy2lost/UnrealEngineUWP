// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Elements/Common/TypedElementHandles.h"

class ITypedElementDataStorageInterface;
class UTedsMementoTranslatorBase;

namespace UE::Editor::DataStorage
{
	class FMementoSystem
	{
	public:
		explicit FMementoSystem(ITypedElementDataStorageInterface& InDataStorage);

		TypedElementDataStorage::RowHandle CreateMemento(TypedElementDataStorage::RowHandle SourceRow);
		void CreateMemento(TypedElementDataStorage::RowHandle ReservedMementoRow, TypedElementDataStorage::RowHandle SourceRow);
		void RestoreMemento(TypedElementDataStorage::RowHandle MementoRow, TypedElementDataStorage::RowHandle TargetRow);
		void DestroyMemento(TypedElementDataStorage::RowHandle MementoRow);

	private:
		void CreateMementoInternal(TypedElementDataStorage::RowHandle MementoRow, TypedElementDataStorage::RowHandle SourceRow);

		TArray<const UTedsMementoTranslatorBase*> MementoTranslators;
		TypedElementDataStorage::TableHandle MementoRowBaseTable;
		ITypedElementDataStorageInterface& DataStorage;
	};
} // namespace UE::Editor::DataStorage
