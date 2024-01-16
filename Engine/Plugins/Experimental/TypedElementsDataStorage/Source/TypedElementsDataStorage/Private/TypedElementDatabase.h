// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassArchetypeTypes.h"
#include "Misc/TVariant.h"
#include "Queries/TypedElementExtendedQueryStore.h"
#include "Templates/SharedPointer.h"
#include "TypedElementDatabaseCommandBuffer.h"
#include "TypedElementDatabaseEnvironment.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"

#include "TypedElementDatabase.generated.h"

struct FMassEntityManager;
struct FMassProcessingPhaseManager;
class FOutputDevice;
class UWorld;

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabase 
	: public UObject
	, public ITypedElementDataStorageInterface
{
	GENERATED_BODY()

public:
	~UTypedElementDatabase() override = default;
	
	void Initialize();
	void Deinitialize();

	/** Triggered at the start of the underlying Mass' tick cycle. */
	void OnPreMassTick(float DeltaTime);
	/** Triggered just before underlying Mass processing completes it's tick cycle. */
	void OnPostMassTick(float DeltaTime);

	TSharedPtr<FMassEntityManager> GetActiveMutableEditorEntityManager();
	TSharedPtr<const FMassEntityManager> GetActiveEditorEntityManager() const;

	TypedElementTableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) override;
	TypedElementTableHandle RegisterTable(TypedElementTableHandle SourceTable, TConstArrayView<const UScriptStruct*> ColumnList, 
		const FName Name) override;
	TypedElementTableHandle FindTable(const FName Name) override;

	TypedElementRowHandle ReserveRow() override;
	TypedElementRowHandle AddRow(TypedElementTableHandle Table) override;
	bool AddRow(TypedElementRowHandle ReservedRow, TypedElementTableHandle Table) override;
	bool BatchAddRow(TypedElementTableHandle Table, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated) override;
	bool BatchAddRow(TypedElementTableHandle Table, TConstArrayView<TypedElementRowHandle> ReservedHandles,
		TypedElementDataStorageCreationCallbackRef OnCreated) override;
	void RemoveRow(TypedElementRowHandle Row) override;
	bool IsRowAvailable(TypedElementRowHandle Row) const override;
	bool HasRowBeenAssigned(TypedElementRowHandle Row) const override;

	bool AddColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	bool AddColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) override;
	void RemoveColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	void RemoveColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) override;
	void* AddOrGetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) override;
	void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) override;
	const void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) const override;
	ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments) override;
	bool AddColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	void RemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	bool AddRemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	bool BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle> Rows,TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	ColumnDataResult GetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) override;
	bool HasColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const override;
	bool HasColumns(TypedElementRowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const override;

	void RegisterTickGroup(FName GroupName, EQueryTickPhase Phase, FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread);
	void UnregisterTickGroup(FName GroupName, EQueryTickPhase Phase);

	TypedElementQueryHandle RegisterQuery(FQueryDescription&& Query) override;
	void UnregisterQuery(TypedElementQueryHandle Query) override;
	const FQueryDescription& GetQueryDescription(TypedElementQueryHandle Query) const override;
	FName GetQueryTickGroupName(EQueryTickGroups Group) const override;
	FQueryResult RunQuery(TypedElementQueryHandle Query) override;
	FQueryResult RunQuery(TypedElementQueryHandle Query, DirectQueryCallbackRef Callback) override;

	TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const override;
	void IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row) override;
	void ReindexRow(
		TypedElementDataStorage::IndexHash OriginalIndex, 
		TypedElementDataStorage::IndexHash NewIndex, 
		TypedElementDataStorage::RowHandle Row) override;
	void RemoveIndex(TypedElementDataStorage::IndexHash Index) override;

	FTypedElementOnDataStorageUpdate& OnUpdate() override;
	bool IsAvailable() const override;
	void* GetExternalSystemAddress(UClass* Target) override;

	void DebugPrintQueryCallbacks(FOutputDevice& Output);

private:
	void PreparePhase(EQueryTickPhase Phase, float DeltaTime);
	void FinalizePhase(EQueryTickPhase Phase, float DeltaTime);
	void Reset();
	
	static const FName TickGroupName_SyncWidget;
	
	TArray<FMassArchetypeHandle> Tables;
	TMap<FName, TypedElementTableHandle> TableNameLookup;

	TUniquePtr<FTypedElementDatabaseEnvironment> Environment;
	FTypedElementExtendedQueryStore Queries;

	FTypedElementDatabaseCommandBuffer::CommandBuffer DeferredCommands;
	
	FTypedElementOnDataStorageUpdate OnUpdateDelegate;
	FDelegateHandle OnPreMassTickHandle;
	FDelegateHandle OnPostMassTickHandle;

	TSharedPtr<FMassEntityManager> ActiveEditorEntityManager;
	TSharedPtr<FMassProcessingPhaseManager> ActiveEditorPhaseManager;
};