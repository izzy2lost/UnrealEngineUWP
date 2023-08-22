// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementMementoSystem.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoInterface.h"
#include "Memento/TypedElementMementoTranslators.h"
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
		ObjectRemovedDelegateHandle = DataStorageCompatibility.GetOnObjectPreDestroy().AddUObject(this, &UTypedElementMementoSystemFactory::HandleObjectPreRemoveFromCompatibility);
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
				DataStorageCompatibility.GetOnObjectPreDestroy().Remove(ObjectRemovedDelegateHandle);
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
				ObjectRemovedDelegateHandle = DataStorageCompatibility.GetOnObjectPreDestroy().AddUObject(this, &UTypedElementMementoSystemFactory::HandleObjectPreRemoveFromCompatibility);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
	};
	
	Private::CVarMementoEnable->OnChangedDelegate().AddLambda(HandleCVarEnabledChanged);
	
	FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementMementoSystemFactory::HandleOnObjectsReinstanced);
}

/**
 * Object reinstancing happens in the following sequence:
 * - This HandleOnObjectsReinstanced callback is fired
 * - The old instance objects are deleted
 * - The new instance objects are added
 */
void UTypedElementMementoSystemFactory::HandleOnObjectsReinstanced(
	const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap)
{
	for (FCoreUObjectDelegates::FReplacementObjectMap::TConstIterator Iter = ObjectReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		const UObject* PreDeleteObject = Iter->Key;
		const UObject* NewInstanceObject = Iter->Value;
		
		const TypedElementRowHandle* MementoRowPtr = MementoizableObjects.Find(PreDeleteObject);
		if (MementoRowPtr != nullptr)
		{
			NewInstanceToMementoMap.Add(NewInstanceObject, *MementoRowPtr);
			UE_LOG(LogTypedElementMemento, VeryVerbose, TEXT("Instance Replacement: 0x%p -> %llu [%s]"), NewInstanceObject, *MementoRowPtr, *NewInstanceObject->GetClass()->GetName());
		}
	}
}

void UTypedElementMementoSystemFactory::HandleObjectAddedToCompatibility(ITypedElementDataStorageInterface* Storage, const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row)
{
	// Register row unconditionally for mementoization
	{
		TypedElementRowHandle Memento = Storage->AddRow(MementoRowBaseTable);
		Storage->AddOrGetColumn<FTypedElementMementoOnDelete>(Row, FTypedElementMementoOnDelete{ .Memento = Memento });
		UE_LOG(LogTypedElementMemento, VeryVerbose, TEXT("Object [%s] tagged for mementoization 0x%p -> %llu"), *TypeInfo.GetFName().ToString(), Object, Memento);
		MementoizableObjects.Add(Object, Memento);
	}

	// If this object is a reinstantiation of a deleted object, then setup the memento row to with the target
	// row to trigger reinstantiation
	TypedElementRowHandle Memento;
	if (NewInstanceToMementoMap.RemoveAndCopyValue(Object, Memento))
	{
		UE_LOG(LogTypedElementMemento, VeryVerbose, TEXT("New instance object [%s] mapped to memento 0x%p -> %llu"), *TypeInfo.GetFName().ToString(), Object, Memento);
		Storage->AddOrGetColumn<FTypedElementReinstanceTarget>(Memento, FTypedElementReinstanceTarget{.Target = Row});
	}
}

void UTypedElementMementoSystemFactory::HandleObjectPreRemoveFromCompatibility(ITypedElementDataStorageInterface* Storage,
	const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row)
{
	UE_LOG(LogTypedElementMemento, VeryVerbose, TEXT("Removing object 0x%p"),Object);

	// Remove row unconditionally from mementoization
	MementoizableObjects.Remove(Object);
}

void UTypedElementMementoSystemFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
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

	// Setup processors to execute memento translators on to reinstantiate rows
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
				[MementoTranslator](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementReinstanceTarget& ReinstanceTarget)
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

	// Setup a default policy of deleting populated mementos after a set number of frames
	// The first use-case of mementos are to support reinstancing which occurs over a single frame
	// Therefore it should be safe to remove populated mementos after a couple of frames since the data
	// is no longer needed.
	// Expansion of memento deletion policies will be necessary for other use cases
	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
				TEXT("Add Memento Populated Tag"),
				FObserver::OnRemove<FTypedElementMementoOnDelete>(), // When a memento is populated
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementMementoOnDelete& MementoRow)
				{
					Context.AddColumn<FTypedElementMementoPopulated>(MementoRow.Memento, FTypedElementMementoPopulated{});
				})
				.Compile());
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}

	/**
	 * A processor which deletes mementos that were populated more than N frames ago
	 */
	{
		const TypedElementQueryHandle QueryHandle = DataStorage.RegisterQuery(
			Select(
			TEXT("Delete populated mementos"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
				[](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row)
				{
					Context.RemoveRow(Row);
				})
				.Where().All<FTypedElementMementoTag, FTypedElementMementoPopulated>()
				.Compile()
			);
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}
}



