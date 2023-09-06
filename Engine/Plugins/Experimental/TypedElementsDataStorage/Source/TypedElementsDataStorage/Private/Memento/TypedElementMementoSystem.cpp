// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementMementoSystem.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Memento/TypedElementMementoInterface.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"

void UTypedElementMementoSystem::Initialize(UTypedElementDatabase& DataStorage)
{
	RegisterTables(DataStorage);
	RegisterQueries(DataStorage);
}

void UTypedElementMementoSystem::Deinitialize()
{
}

TypedElementRowHandle UTypedElementMementoSystem::CreateMemento(ITypedElementDataStorageInterface* DataStorage)
{
	return DataStorage->AddRow(MementoRowBaseTable);
}

void UTypedElementMementoSystem::RegisterTables(UTypedElementDatabase& DataStorage)
{
	// Register tables that will be used by reinstancing
	MementoRowBaseTable = DataStorage.RegisterTable(
		{FTypedElementMementoTag::StaticStruct()},
		TEXT("MementoRowBaseTable"));
	
}

void UTypedElementMementoSystem::RegisterQueries(UTypedElementDatabase& DataStorage) const
{
	using DSI = ITypedElementDataStorageInterface;
	
	TArray<const UTypedElementMementoTranslatorBase*> MementoTranslators;

	// Discover all MementoTranslators
	{
		const bool bIncludeDerived = true;
		EObjectFlags ExcludeFlags = EObjectFlags::RF_NoFlags;
		ForEachObjectOfClass(UTypedElementMementoTranslatorBase::StaticClass(), [&MementoTranslators](UObject* Object)
		{
			const UTypedElementMementoTranslatorBase* TranslatorCandidate = Cast<UTypedElementMementoTranslatorBase>(Object);
			// Exclude abstract classes
			if (TranslatorCandidate->GetClass()->GetClassFlags() & EClassFlags::CLASS_Abstract)
			{
				return;
			}
			MementoTranslators.Add(TranslatorCandidate);
		},
		bIncludeDerived,
		ExcludeFlags);
	}

	using namespace TypedElementQueryBuilder;

	// Setup observer queries to execute memento translators on row deletions that match the translator's
	// capabilities.
	for (int32 Index = 0, End = MementoTranslators.Num(); Index < End; ++Index)
	{
		const UTypedElementMementoTranslatorBase* MementoTranslator = MementoTranslators[Index];
		const UScriptStruct* MementoizedColumn = MementoTranslator->GetColumnType();
		const UScriptStruct* Memento = MementoTranslator->GetMementoType();
		
		const FName TranslationProcessorName = FName(FString::Printf(TEXT("MementoTranslator (Column->Memento) %s -> %s"), *MementoizedColumn->GetName(), *Memento->GetName()));
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TranslationProcessorName,
				FObserver::OnRemove<FTypedElementMementoOnDelete>(),
				[MementoTranslator, Memento, MementoizedColumn](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoOnDelete& MementoRow)
				{
					void* StagedMementoColumn = Context.AddColumnUninitialized(MementoRow.Memento, Memento);
					// StagedMementoColumn will be uninitialized memory, ensure constructor called
					Memento->InitializeStruct(StagedMementoColumn);
		
					const void* SourceColumn = Context.GetColumn(MementoizedColumn);
					MementoTranslator->TranslateColumnToMemento(SourceColumn, StagedMementoColumn);
				})
				.ReadOnly(MementoizedColumn)
				.Compile());
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}

	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TEXT("Add Populated To Memento"),
				FObserver::OnRemove<FTypedElementMementoOnDelete>(),
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Memento, const FTypedElementMementoOnDelete& MementoRow)
				{
					Context.AddColumns(MementoRow.Memento, TConstArrayView<const UScriptStruct*>({FTypedElementMementoPopulated::StaticStruct()}));
				})
		.Compile());
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}

	// Setup processors to execute memento translators on reinstantiate columns onto a target
	// The primary query runs over all mementos with a reinstance target and a column holding memento data
	// The subquery runs on rows targeted by the reinstance column that also have a mementoizable column
	// Note: Currently it is not possible for mementos to create a new column from a subquery
	for (int32 Index = 0, End = MementoTranslators.Num(); Index < End; ++Index)
	{
		const UTypedElementMementoTranslatorBase* MementoTranslator = MementoTranslators[Index];
		const UScriptStruct* MementoizedColumnType = MementoTranslator->GetColumnType();
		const UScriptStruct* MementoType = MementoTranslator->GetMementoType();

		namespace TEDS = TypedElementDataStorage;

		const FName TranslationProcessorName = FName(FString::Printf(TEXT("MementoTranslator (Memento->Column) %s -> %s"), *MementoType->GetName(), *MementoizedColumnType->GetName()));
		const TypedElementQueryHandle QueryHandleForExistence = DataStorage.RegisterQuery(
			Select(
				TranslationProcessorName,
				FProcessor(DSI::EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
				[MementoTranslator](TEDS::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoReinstanceTarget& ReinstanceTarget)
				{
					// Add the column, note the column may already exist.
					void* StagedColumn = Context.AddColumnUninitialized(ReinstanceTarget.Target, MementoTranslator->GetColumnType());

					// StagedColumn will be uninitialized memory, ensure constructor called
					MementoTranslator->GetColumnType()->InitializeStruct(StagedColumn);

					const void* Memento = Context.GetColumn(MementoTranslator->GetMementoType());
					MementoTranslator->TranslateMementoToColumn(Memento, StagedColumn);
				})
				.ReadOnly(MementoType)
				.Where()
					.All<FTypedElementMementoTag>()
				.Compile());
		check(QueryHandleForExistence != TypedElementInvalidQueryHandle);
	}

	/**
	 * A processor which deletes mementos that were used for rehydrating another row
	 */
	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
			TEXT("Delete mementos used for reinstancing"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row)
				{
					Context.RemoveRow(Row);
				})
				.ReadOnly<FTypedElementMementoReinstanceTarget>()
				.Where().All<FTypedElementMementoTag>()
				.Compile()
			);
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}
}



