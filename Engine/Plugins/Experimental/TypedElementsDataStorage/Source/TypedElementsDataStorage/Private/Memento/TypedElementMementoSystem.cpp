// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementMementoSystem.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoInterface.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "PropertyBag.h"
#include "TypedElementMementoRowTypes.h"

DECLARE_LOG_CATEGORY_CLASS(LogTypedElementMemento, Log, All)

namespace Private
{

	// Number of frames to keep memento rows before deletion
	bool GMementosEnabled = false;
	FAutoConsoleVariableRef CVarMementoEnable(
		TEXT("teds.mementos.enable"),
		GMementosEnabled,
		TEXT("Enable memento system for newly added objects\n"));

	// Number of frames to keep memento rows before deletion
	int32 GMementoKeepFrames = 120;
	FAutoConsoleVariableRef CVarMementoKeepFrame(
		TEXT("teds.mementos.keepframes"),
		GMementoKeepFrames,
		TEXT("Number of frames to keep memento rows before deletion\n"));
}

void UTypedElementMementoSystemFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) const
{
	MementoRowBaseTable = DataStorage.RegisterTable(
		{FTypedElementMementoTag::StaticStruct()},
		TEXT("MementoRowBaseTable"));

	// Memento rows become populated when the FTypedElementMementoPopulated column is added
	TypedElementTableHandle MementoRowBasePopulatedTable = DataStorage.RegisterTable(
		MementoRowBaseTable,
		{FTypedElementMementoPopulated::StaticStruct()},
		TEXT("MementoRowBasePopulatedTable"));
}

void UTypedElementMementoSystemFactory::RegisterRegistrationFilters(
	ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility) const
{
	const_cast<UTypedElementMementoSystemFactory*>(this)->RegisterWithCompatibilityLayer(DataStorageCompatibility);
}

void UTypedElementMementoSystemFactory::RegisterWithCompatibilityLayer(ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility)
{
	if (Private::GMementosEnabled)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ObjectAddedDelegateHandle = DataStorageCompatibility.GetOnObjectAddedDelegate().AddUObject(this, &UTypedElementMementoSystemFactory::HandleObjectAddedToCompatibility);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	auto HandleCVarEnabledChanged = [this, &DataStorageCompatibility](IConsoleVariable* CVar)
	{
		// Toggled Off
		if (!Private::GMementosEnabled)
		{
			if (ObjectAddedDelegateHandle.IsValid())
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				DataStorageCompatibility.GetOnObjectAddedDelegate().Remove(ObjectAddedDelegateHandle);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				ObjectAddedDelegateHandle.Reset();
			}
		}
		// Toggled on
		else if (Private::GMementosEnabled)
		{
			if (!ObjectAddedDelegateHandle.IsValid())
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				ObjectAddedDelegateHandle = DataStorageCompatibility.GetOnObjectAddedDelegate().AddUObject(this, &UTypedElementMementoSystemFactory::HandleObjectAddedToCompatibility);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
	};
	
	Private::CVarMementoEnable->OnChangedDelegate().AddLambda(HandleCVarEnabledChanged);
}

void UTypedElementMementoSystemFactory::HandleObjectAddedToCompatibility(ITypedElementDataStorageInterface* Storage, const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row)
{
	// Register row for mementoization
	TypedElementRowHandle Memento = Storage->AddRow(MementoRowBaseTable);
	Storage->AddOrGetColumn<FTypedElementMementoOnDelete>(Row, FTypedElementMementoOnDelete{ .Memento = Memento });
}

void UTypedElementMementoSystemFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
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

	// Setup a default policy of deleting populated mementos after a set number of frames
	// The first use-case of mementos are to support reinstancing which occurs over a single frame
	// Therefore it should be safe to remove populated mementos after a couple of frames since the data
	// is no longer needed.
	// Expansion of memento deletion policies will be necessary for other use cases
	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TEXT("Add Memento Deletion Policy Data"),
				FObserver::OnRemove<FTypedElementMementoOnDelete>(), // When a memento is populated
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoOnDelete& MementoRow)
				{
					Context.AddColumn<FTypedElementMementoPopulated>(
						MementoRow.Memento,
						FTypedElementMementoPopulated
						{
							.FrameNumber = GFrameCounter
						});
				})
				.Compile());
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}

	using DSI = ITypedElementDataStorageInterface;

	/**
	 * A processor which deletes mementos that were populated more than N frames ago
	 */
	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
			TEXT("Delete populated mementos older than N frames"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoPopulated& PopulateFrameNumber)
				{
					if (PopulateFrameNumber.FrameNumber + Private::GMementoKeepFrames < GFrameCounter)
					{
						Context.RemoveRow(Row);
					}
				})
				.Where().All<FTypedElementMementoTag>()
				.Compile()
			);
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}
}



