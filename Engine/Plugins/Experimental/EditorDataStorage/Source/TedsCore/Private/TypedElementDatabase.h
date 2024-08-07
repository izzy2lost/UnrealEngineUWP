// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassArchetypeTypes.h"
#include "Misc/TVariant.h"
#include "Templates/SharedPointer.h"
#include "TypedElementDatabaseCommandBuffer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"

#include "TypedElementDatabase.generated.h"

struct FMassEntityManager;
struct FMassProcessingPhaseManager;
class UTypedElementDataStorageFactory;
class FOutputDevice;
class UWorld;

namespace UE::Editor::DataStorage
{
	class FEnvironment;
} // namespace UE::Editor::DataStorage

UCLASS()
class TEDSCORE_API UTypedElementDatabase
	: public UObject
	, public ITypedElementDataStorageInterface
{
	GENERATED_BODY()

public:
	template<typename FactoryType, typename DatabaseType>
	class TFactoryIterator
	{
	public:
		using ThisType = TFactoryIterator<FactoryType, DatabaseType>;
		using FactoryPtr = FactoryType*;
		using DatabasePtr = DatabaseType*;

		TFactoryIterator() = default;
		explicit TFactoryIterator(DatabasePtr InDatabase);

		FactoryPtr operator*() const;
		ThisType& operator++();
		operator bool() const;

	private:
		DatabasePtr Database = nullptr;
		int32 Index = 0;
	};

	using FactoryIterator = TFactoryIterator<UTypedElementDataStorageFactory, UTypedElementDatabase>;
	using FactoryConstIterator = TFactoryIterator<const UTypedElementDataStorageFactory, const UTypedElementDatabase>;

public:
	~UTypedElementDatabase() override = default;
	
	void Initialize();
	
	void SetFactories(TConstArrayView<UClass*> InFactories);
	void ResetFactories();

	/** An iterator which allows traversal of factory instances. Ordered lowest->highest of GetOrder() */
	FactoryIterator CreateFactoryIterator();
	/** An iterator which allows traversal of factory instances. Ordered lowest->highest of GetOrder() */
	FactoryConstIterator CreateFactoryIterator() const;

	/** Returns factory instance given the type of factory */
	virtual const UTypedElementDataStorageFactory* FindFactory(const UClass* FactoryType) const override;
	/** Helper for FindFactory(const UClass*) */
	template<typename FactoryTypeT>
	const FactoryTypeT* FindFactory() const;
	
	void Deinitialize();

	/** Triggered at the start of the underlying Mass' tick cycle. */
	void OnPreMassTick(float DeltaTime);
	/** Triggered just before underlying Mass processing completes it's tick cycle. */
	void OnPostMassTick(float DeltaTime);

	TSharedPtr<FMassEntityManager> GetActiveMutableEditorEntityManager();
	TSharedPtr<const FMassEntityManager> GetActiveEditorEntityManager() const;

	virtual TypedElementDataStorage::TableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) override;
	virtual TypedElementDataStorage::TableHandle RegisterTable(
		TypedElementDataStorage::TableHandle SourceTable, TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) override;
	virtual TypedElementDataStorage::TableHandle FindTable(const FName Name) override;

	virtual TypedElementDataStorage::RowHandle ReserveRow() override;
	virtual void BatchReserveRows(int32 Count, TFunctionRef<void(TypedElementDataStorage::RowHandle)> ReservationCallback) override;
	virtual void BatchReserveRows(TArrayView<TypedElementDataStorage::RowHandle> ReservedRows) override;
	virtual TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table, 
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) override;
	TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table) override;
	virtual bool AddRow(TypedElementDataStorage::RowHandle ReservedRow, TypedElementDataStorage::TableHandle Table) override;
	virtual bool AddRow(TypedElementDataStorage::RowHandle ReservedRow, TypedElementDataStorage::TableHandle Table,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) override;
	virtual bool BatchAddRow(TypedElementDataStorage::TableHandle Table, int32 Count,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) override;
	virtual bool BatchAddRow(TypedElementDataStorage::TableHandle Table, TConstArrayView<TypedElementDataStorage::RowHandle> ReservedHandles,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) override;
	virtual void RemoveRow(TypedElementDataStorage::RowHandle Row) override;
	virtual bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const override;
	virtual bool IsRowAssigned(TypedElementDataStorage::RowHandle Row) const override;

	virtual void AddColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	virtual void AddColumn(TypedElementRowHandle Row, const UE::Editor::DataStorage::FDynamicTag& Tag, const FName& InValue) override;
	virtual void AddColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType,
		const TypedElementDataStorage::ColumnCreationCallbackRef& Initializer,
		TypedElementDataStorage::ColumnCopyOrMoveCallback Relocator) override;
	virtual void RemoveColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	virtual void RemoveColumn(TypedElementRowHandle Row, const UE::Editor::DataStorage::FDynamicTag& Tag) override;
	virtual void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	virtual const void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) const override;
	virtual void AddColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	virtual void RemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	virtual void AddRemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	virtual void BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle> Rows,TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const override;
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const override;
	virtual void ListColumns(TypedElementDataStorage::RowHandle Row, TypedElementDataStorage::ColumnListCallbackRef Callback) const;
	virtual void ListColumns(TypedElementDataStorage::RowHandle Row, TypedElementDataStorage::ColumnListWithDataCallbackRef Callback);
	virtual bool MatchesColumns(TypedElementDataStorage::RowHandle Row, const TypedElementDataStorage::FQueryConditions& Conditions) const override;

	void RegisterTickGroup(FName GroupName, EQueryTickPhase Phase, FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread);
	void UnregisterTickGroup(FName GroupName, EQueryTickPhase Phase);

	TypedElementQueryHandle RegisterQuery(FQueryDescription&& Query) override;
	virtual void UnregisterQuery(TypedElementQueryHandle Query) override;
	virtual const FQueryDescription& GetQueryDescription(TypedElementQueryHandle Query) const override;
	virtual FName GetQueryTickGroupName(EQueryTickGroups Group) const override;
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query) override;
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query, DirectQueryCallbackRef Callback) override;
	virtual void ActivateQueries(FName ActivationName) override;

	virtual TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const override;
	virtual void IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row) override;
	virtual void BatchIndexRows(
		TConstArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexRowPairs) override;
	virtual void ReindexRow(
		TypedElementDataStorage::IndexHash OriginalIndex, 
		TypedElementDataStorage::IndexHash NewIndex, 
		TypedElementDataStorage::RowHandle Row) override;
	virtual void RemoveIndex(TypedElementDataStorage::IndexHash Index) override;

	virtual FTypedElementOnDataStorageUpdate& OnUpdate() override;
	virtual FTypedElementOnDataStorageUpdate& OnUpdateCompleted() override;
	virtual bool IsAvailable() const override;
	virtual void* GetExternalSystemAddress(UClass* Target) override;

	virtual bool SupportsExtension(FName Extension) const override;
	virtual void ListExtensions(TFunctionRef<void(FName)> Callback) const override;
	
	TSharedPtr<UE::Editor::DataStorage::FEnvironment> GetEnvironment();
	TSharedPtr<const UE::Editor::DataStorage::FEnvironment> GetEnvironment() const;

	FMassArchetypeHandle LookupArchetype(TypedElementDataStorage::TableHandle InTableHandle) const;

	void DebugPrintQueryCallbacks(FOutputDevice& Output);

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	void PreparePhase(EQueryTickPhase Phase, float DeltaTime);
	void FinalizePhase(EQueryTickPhase Phase, float DeltaTime);
	void Reset();

	int32 GetTableChunkSize(FName TableName) const;
	
	struct FFactoryTypePair
	{
		// Used to find the factory by type without needing to dereference each one
		TObjectPtr<UClass> Type;
		
		TObjectPtr<UTypedElementDataStorageFactory> Instance;
	};
	
	static const FName TickGroupName_Default;
	static const FName TickGroupName_PreUpdate;
	static const FName TickGroupName_Update;
	static const FName TickGroupName_PostUpdate;
	static const FName TickGroupName_SyncWidget;
	static const FName TickGroupName_SyncExternalToDataStorage;
	static const FName TickGroupName_SyncDataStorageToExternal;
	
	TArray<FMassArchetypeHandle> Tables;
	TMap<FName, TypedElementDataStorage::TableHandle> TableNameLookup;

	// Ordered array of factories by the return value of GetOrder()
	TArray<FFactoryTypePair> Factories;

	TSharedPtr<UE::Editor::DataStorage::FEnvironment> Environment;
	
	FTypedElementOnDataStorageUpdate OnUpdateDelegate;
	FTypedElementOnDataStorageUpdate OnUpdateCompletedDelegate;
	FDelegateHandle OnPreMassTickHandle;
	FDelegateHandle OnPostMassTickHandle;

	TSharedPtr<FMassEntityManager> ActiveEditorEntityManager;
	TSharedPtr<FMassProcessingPhaseManager> ActiveEditorPhaseManager;
};

template <typename FactoryType, typename DatabaseType>
UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::TFactoryIterator(DatabasePtr InDatabase): Database(InDatabase)
{}

template <typename FactoryType, typename DatabaseType>
typename UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::FactoryPtr UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::operator*() const
{
	return Database->Factories[Index].Instance;
}

template <typename FactoryType, typename DatabaseType>
typename UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::ThisType& UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::operator++()
{
	if (Database != nullptr && Index < Database->Factories.Num())
	{
		++Index;
	}
	return *this;
}

template <typename FactoryType, typename DatabaseType>
UTypedElementDatabase::TFactoryIterator<FactoryType, DatabaseType>::operator bool() const
{
	return Database != nullptr && Index < Database->Factories.Num();
}

template <typename FactoryTypeT>
const FactoryTypeT* UTypedElementDatabase::FindFactory() const
{
	return static_cast<const FactoryTypeT*>(FindFactory(FactoryTypeT::StaticClass()));
}
