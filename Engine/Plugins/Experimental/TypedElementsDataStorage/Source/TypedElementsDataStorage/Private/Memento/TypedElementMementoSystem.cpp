// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementMementoSystem.h"

#include "Elements/Common/TypedElementDataStorageLog.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"

UTypedElementMementoSystem::UTypedElementMementoSystem(ITypedElementDataStorageInterface& InDataStorage)
	: DataStorage(InDataStorage)
{
	// Register tables that will be used by reinstancing
	MementoRowBaseTable = DataStorage.RegisterTable<FTypedElementMementoTag>(TEXT("MementoRowBaseTable"));

	// Discover all MementoTranslators
	{
		constexpr bool bIncludeDerived = true;
		constexpr EObjectFlags ExcludeFlags = EObjectFlags::RF_NoFlags;
		ForEachObjectOfClass(UTypedElementMementoTranslatorBase::StaticClass(),
			[this](UObject* Object)
			{
				const UTypedElementMementoTranslatorBase* TranslatorCandidate = Cast<UTypedElementMementoTranslatorBase>(Object);
				// Exclude abstract classes
				if (TranslatorCandidate->GetClass()->GetClassFlags() & EClassFlags::CLASS_Abstract)
				{
					return;
				}
				MementoTranslators.Add(TranslatorCandidate);
			},
			bIncludeDerived, ExcludeFlags);
	}
}

TypedElementRowHandle UTypedElementMementoSystem::CreateMemento(TypedElementDataStorage::RowHandle SourceRow)
{
	using namespace TypedElementDataStorage;

	RowHandle MementoRow = DataStorage.AddRow(MementoRowBaseTable);
	
	for (const UTypedElementMementoTranslatorBase* Translator : MementoTranslators)
	{
		if (void* SourceColumn = DataStorage.GetColumnData(SourceRow, Translator->GetColumnType()))
		{
			const UScriptStruct* MementoType = Translator->GetMementoType();
			DataStorage.AddColumnData(MementoRow, MementoType,
				[Translator, SourceColumn](void* MementoColumn, const UScriptStruct& ColumnType)
				{
					ColumnType.InitializeStruct(MementoColumn);
					Translator->TranslateColumnToMemento(SourceColumn, MementoColumn);
				},
				[](const UScriptStruct& ColumnType, void* Destination, void* Source)
				{
					ColumnType.CopyScriptStruct(Destination, Source);
				});

			UE_LOG(LogTypedElementDataStorage, VeryVerbose, 
				TEXT("Column->Memento: %llu -> %llu"), SourceRow, MementoRow);
		}
	}

	return MementoRow;
}

void UTypedElementMementoSystem::RestoreMemento(TypedElementDataStorage::RowHandle MementoRow, TypedElementDataStorage::RowHandle TargetRow)
{
	for (const UTypedElementMementoTranslatorBase* Translator : MementoTranslators)
	{
		if (void* MementoColumn = DataStorage.GetColumnData(MementoRow, Translator->GetMementoType()))
		{
			const UScriptStruct* TargetType = Translator->GetColumnType();
			DataStorage.AddColumnData(TargetRow, TargetType,
				[Translator, MementoColumn](void* TargetColumn, const UScriptStruct& ColumnType)
				{
					ColumnType.InitializeStruct(TargetColumn);
					Translator->TranslateMementoToColumn(MementoColumn, TargetColumn);
				},
				[](const UScriptStruct& ColumnType, void* Destination, void* Source)
				{
					ColumnType.CopyScriptStruct(Destination, Source);
				});

			UE_LOG(LogTypedElementDataStorage, VeryVerbose, 
				TEXT("Memento->Column: %llu -> %llu"), MementoRow, TargetRow);
		}
	}
}

void UTypedElementMementoSystem::DestroyMemento(TypedElementDataStorage::RowHandle MementoRow)
{
	checkf(DataStorage.IsRowAvailable(MementoRow) && DataStorage.HasColumns<FTypedElementMementoTag>(MementoRow),
		TEXT("Deleting memento row that's not marked as such."));
	DataStorage.RemoveRow(MementoRow);
}
