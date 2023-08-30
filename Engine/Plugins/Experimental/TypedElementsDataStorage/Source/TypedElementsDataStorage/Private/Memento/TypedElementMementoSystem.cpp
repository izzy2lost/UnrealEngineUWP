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
		
		const FName TranslationProcessorName = FName(FString::Printf(TEXT("MementoTranslator: %s -> %s"), *MementoizedColumn->GetName(), *Memento->GetName()));
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TranslationProcessorName,
				FObserver::OnRemove<FTypedElementMementoOnDelete>(),
				[MementoTranslator, Memento, MementoizedColumn](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle row, const FTypedElementMementoOnDelete& MementoRow)
				{
					void* MementoObject = Context.AddColumnUnitialized(MementoRow.Memento, Memento);
					Memento->InitializeStruct(MementoObject);
		
					const void* SourceColumn = Context.GetColumn(MementoizedColumn);
					MementoTranslator->TranslateColumnToMemento(SourceColumn, MementoObject);
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

		TypedElementQueryHandle Subquery = DataStorage.RegisterQuery(
			Select()
				.ReadWrite(MementoizedColumnType)
			.Compile());

		const FName TranslationProcessorName = FName(FString::Printf(TEXT("MementoTranslator: %s -> %s"), *MementoType->GetName(), *MementoizedColumnType->GetName()));
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TranslationProcessorName,
				FProcessor(DSI::EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
				[MementoTranslator](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoReinstanceTarget& ReinstanceTarget)
				{
					const UScriptStruct* MementoType = MementoTranslator->GetMementoType();
					
					const void* Memento = Context.GetColumn(MementoType);
					Context.RunSubquery(0, ReinstanceTarget.Target, 
						[MementoTranslator, Memento](const DSI::FQueryDescription&, DSI::ISubqueryContext& SubQueryContext)
						{
							const UScriptStruct* MementoizedColumnType = MementoTranslator->GetColumnType();
							void* MementoizedColumn = SubQueryContext.GetMutableColumn(MementoizedColumnType);

							MementoTranslator->TranslateMementoToColumn(Memento, MementoizedColumn);
						});
				})
				.ReadOnly(MementoType)
				.Where()
					.All<FTypedElementMementoTag>()
				.DependsOn().SubQuery(Subquery)
				.Compile());
		check(QueryHandle != TypedElementInvalidQueryHandle);
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



