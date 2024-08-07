// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Elements/Common/TypedElementQueryDescription.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Math/NumericLimits.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Function.h"
#include "UObject/Interface.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDataStorageInterface.generated.h"

class UClass;
class USubsystem;
class UScriptStruct;
class UTypedElementDataStorageFactory;

using TypedElementTableHandle = TypedElementDataStorage::TableHandle;
static constexpr auto TypedElementInvalidTableHandle = TypedElementDataStorage::InvalidTableHandle;
using TypedElementRowHandle = TypedElementDataStorage::RowHandle;
static constexpr auto TypedElementInvalidRowHandle = TypedElementDataStorage::InvalidRowHandle;
using TypedElementQueryHandle = TypedElementDataStorage::QueryHandle;
static constexpr auto TypedElementInvalidQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

using FTypedElementOnDataStorageCreation = FSimpleMulticastDelegate;
using FTypedElementOnDataStorageDestruction = FSimpleMulticastDelegate;
using FTypedElementOnDataStorageUpdate = FSimpleMulticastDelegate;

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Convenience structure that can be used to pass a list of columns to functions that don't
 * have an dedicate templated version that takes a column list directly, for instance when
 * multiple column lists are used. Note that the returned array view is only available while
 * this object is constructed, so care must be taken with functions that return a const array view.
 */
template<TypedElementDataStorage::TColumnType... Columns>
struct TTypedElementColumnTypeList
{
	const UScriptStruct* ColumnTypes[sizeof...(Columns)] = { Columns::StaticStruct()... };
	
	operator TConstArrayView<const UScriptStruct*>() const { return ColumnTypes; }
};

class ITypedElementDataStorageInterface
{
	GENERATED_BODY()

public:
	/**
	 * @section Factories
	 *
	 * @description
	 * Factories are an automated way to register tables, queries and other information with TEDS.
	 */

	/** Finds a factory instance registered with TEDS */
	virtual const UTypedElementDataStorageFactory* FindFactory(const UClass* FactoryType) const = 0;

	/** Convenience function for FindFactory */
	template<typename FactoryT>
	const FactoryT* FindFactory() const;
	
	/**
	 * @section Table management
	 * 
	 * @description
	 * Tables are automatically created by taking an existing table and adding/removing columns. For
	 * performance its however better to create a table before adding objects to the table. This
	 * doesn't prevent those objects from having columns added/removed at a later time.
	 * To make debugging and profiling easier it's also recommended to give tables a name.
	 */

	/** Creates a new table for with the provided columns. Optionally a name can be given which is useful for retrieval later. */
	virtual TypedElementDataStorage::TableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;
	template<TypedElementDataStorage::TColumnType... Columns>
	TypedElementDataStorage::TableHandle RegisterTable(const FName Name);
	/** 
	 * Copies the column information from the provided table and creates a new table for with the provided columns. Optionally a 
	 * name can be given which is useful for retrieval later.
	 */
	virtual TypedElementDataStorage::TableHandle RegisterTable(TypedElementDataStorage::TableHandle SourceTable,
		TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;
	template<TypedElementDataStorage::TColumnType... Columns>
	TypedElementDataStorage::TableHandle RegisterTable(TypedElementDataStorage::TableHandle SourceTable, const FName Name);

	/** Returns a previously created table with the provided name or TypedElementInvalidTableHandle if not found. */
	virtual TypedElementDataStorage::TableHandle FindTable(const FName Name) = 0;
	
	/**
	 * @section Row management
	 */

	/** 
	 * Reserves a row to be assigned to a table at a later point. If the row is no longer needed before it's been assigned
	 * to a table, it should still be released with RemoveRow.
	 */
	virtual TypedElementDataStorage::RowHandle ReserveRow() = 0;
	/**
	 * Reserve multiple rows at once to be assigned to a table at a later point. If multiple rows are needed, the batch version will
	 * generally have better performance. If a row is no longer needed before it's been assigned to a table, it should still be released 
	 * with RemoveRow.
	 * The reservation callback will be called once per reserved row.
	 */
	virtual void BatchReserveRows(int32 Count, TFunctionRef<void(TypedElementDataStorage::RowHandle)> ReservationCallback) = 0;
	/**
	 * Reserve multiple rows at once to be assigned to a table at a later point. If multiple rows are needed, the batch version will
	 * generally have better performance. If a row is no longer needed before it's been assigned to a table, it should still be released
	 * with RemoveRow.
	 * The provided range will be have its values set to the reserved row handles.
	 */
	virtual void BatchReserveRows(TArrayView<TypedElementDataStorage::RowHandle> ReservedRows) = 0;

	/** Adds a new row to the provided table. */
	virtual TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table) = 0;
	/**
	 * Adds a new row to the provided table. Callers are expected to use the callback to
	 * initialize the row if needed.
	 */
	virtual TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) = 0;
	/** Adds a new row to the provided table using a previously reserved row. */
	virtual bool AddRow(TypedElementDataStorage::RowHandle ReservedRow, TypedElementDataStorage::TableHandle Table) = 0;
	/**
	 * Adds a new row to the provided table using a previously reserved row. Callers are expected to use the callback to
	 * initialize the row if needed.
	 */
	virtual bool AddRow(TypedElementDataStorage::RowHandle ReservedRow, TypedElementDataStorage::TableHandle Table,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) = 0;

	/**
	 * Add multiple rows at once. For each new row the OnCreated callback is called. Callers are expected to use the callback to
	 * initialize the row if needed.
	 */
	virtual bool BatchAddRow(TypedElementDataStorage::TableHandle Table, int32 Count, TypedElementDataStorage::RowCreationCallbackRef OnCreated) = 0;
	/**
	 * Add multiple rows at once. For each new row the OnCreated callback is called. Callers are expected to use the callback to
	 * initialize the row if needed. This version uses a set of previously reserved rows. Any row that can't be used will be 
	 * released.
	 */
	virtual bool BatchAddRow(TypedElementDataStorage::TableHandle Table, TConstArrayView<TypedElementDataStorage::RowHandle> ReservedHandles,
		TypedElementDataStorage::RowCreationCallbackRef OnCreated) = 0;

	/** Removes a previously reserved or added row. If the row handle is invalid or already removed, nothing happens */
	virtual void RemoveRow(TypedElementDataStorage::RowHandle Row) = 0;

	/** Checks whether or not a row is in use. This is true even if the row has only been reserved. */
	virtual bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const = 0;
	/** Checks whether or not a row has been reserved but not yet assigned to a table. */
	virtual bool IsRowAssigned(TypedElementDataStorage::RowHandle Row) const = 0;

	
	/**
	 * @section Column management
	 */

	/** Adds a column to a row or does nothing if already added. */
	virtual void AddColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	template<TypedElementDataStorage::TColumnType ColumnType>
	void AddColumn(TypedElementRowHandle Row);
	/**
	 * Adds a new data column and initializes it. The relocator will be used to copy or move the column out of
	 * its temporary location into the final table if the addition needs to be deferred.
	 */
	virtual void AddColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType,
		const TypedElementDataStorage::ColumnCreationCallbackRef& Initializer,
		TypedElementDataStorage::ColumnCopyOrMoveCallback Relocator) = 0;
	template<TypedElementDataStorage::TDataColumnType ColumnType>
	void AddColumn(TypedElementRowHandle Row, ColumnType&& Column);

	/**
	 * Adds a DynamicTag with the given value to a row
	 * A row can have multiple DynamicTags, but only one of each tag type.
	 * Example:
	 *   AddColumn(Row, FDynamicTag(TEXT("Color"), TEXT("Red));     // Valid
	 *   AddColumn(Row, FDynamicTag(TEXT("Direction"), TEXT("Up")); // Valid
	 *   AddColumn(Row, FDynamicTag(TEXT("Color"), TEXT("Blue"));   // Will do nothing since there already exists a Color dynamic tag
	 * Note: Current support for changing a dynamic tag from one value to another requires that the tag is removed before a new one
	 *       is added.  This will likely change in the future to transparently replace the tag to have consistent behaviour with other usages
	 *       of AddColumn
	 */
	virtual void AddColumn(TypedElementDataStorage::RowHandle Row, const UE::Editor::DataStorage::FDynamicTag& Tag, const FName& Value) = 0;

	template<typename T>
	void AddColumn(TypedElementDataStorage::RowHandle Row, const FName& Tag, const FName& Value) = delete;
	
	template<>
	void AddColumn<UE::Editor::DataStorage::FDynamicTag>(TypedElementDataStorage::RowHandle Row, const FName& Tag, const FName& Value);

	template<TypedElementDataStorage::TEnumType EnumT>
	void AddColumn(TypedElementDataStorage::RowHandle Row, EnumT Value);
	
	template<auto Value, TypedElementDataStorage::TEnumType EnumT = decltype(Value)>
	void AddColumn(TypedElementDataStorage::RowHandle Row);

	/**
	 * Adds multiple columns from a row. This is typically more efficient than adding columns one 
	 * at a time.
	 */
	virtual void AddColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) = 0;
	template<TypedElementDataStorage::TColumnType... Columns>
	void AddColumns(TypedElementRowHandle Row);

	/** Removes a column from a row or does nothing if already removed. */
	virtual void RemoveColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	template<TypedElementDataStorage::TColumnType Column>
	void RemoveColumn(TypedElementRowHandle Row);

	template<TypedElementDataStorage::TEnumType EnumT>
	void RemoveColumn(TypedElementDataStorage::RowHandle Row);

	/**
	 * Removes a dynamic tag from the given row
	 * If tag does not exist on row, operation will do nothing.
	 */
	virtual void RemoveColumn(TypedElementDataStorage::RowHandle Row, const UE::Editor::DataStorage::FDynamicTag& Tag) = 0;

	template<typename T>
	void RemoveColumn(TypedElementDataStorage::RowHandle Row, const FName& Tag) = delete;
	
	template<>
	void RemoveColumn<UE::Editor::DataStorage::FDynamicTag>(TypedElementDataStorage::RowHandle Row, const FName& Tag);

	/**
	 * Removes multiple columns from a row. This is typically more efficient than adding columns one
	 * at a time.
	 */
	virtual void RemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) = 0;
	template<TypedElementDataStorage::TColumnType... Columns>
	void RemoveColumns(TypedElementRowHandle Row);

	/** 
	 * Adds and removes the provided column types from the provided row. This is typically more efficient 
	 * than individually adding and removing columns as well as being faster than adding and removing
	 * columns separately.
	 */
	virtual void AddRemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) = 0;
	
	/** Adds and removes the provided column types from the provided list of rows. */
	virtual void BatchAddRemoveColumns(
		TConstArrayView<TypedElementRowHandle> Rows,
		TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) = 0;
	
	/** Retrieves a pointer to the column of the given row or a nullptr if not found or if the column type is a tag. */
	virtual void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual const void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) const = 0;
	/** Returns a pointer to the column of the given row or a nullptr if the type couldn't be found or the row doesn't exist. */
	template<TypedElementDataStorage::TDataColumnType ColumnType>
	ColumnType* GetColumn(TypedElementRowHandle Row);
	template<TypedElementDataStorage::TDataColumnType ColumnType>
	const ColumnType* GetColumn(TypedElementRowHandle Row) const;
	
	/** Determines if the provided row contains the collection of columns and tags. */
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const = 0;
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const = 0;
	template<TypedElementDataStorage::TColumnType... ColumnTypes>
	bool HasColumns(TypedElementRowHandle Row) const;

	/** Lists the columns on a row. This includes data and tag columns. */
	virtual void ListColumns(TypedElementDataStorage::RowHandle Row, TypedElementDataStorage::ColumnListCallbackRef Callback) const = 0;

	/** 
	 * Lists the column type and data on a row. This includes data and tag columns. Not all columns may have data so the data pointer in 
	 * the callback can be null.
	 */
	virtual void ListColumns(TypedElementDataStorage::RowHandle Row, TypedElementDataStorage::ColumnListWithDataCallbackRef Callback) = 0;

	/** Determines if the columns in the row match the query conditions. */
	virtual bool MatchesColumns(TypedElementDataStorage::RowHandle Row, const TypedElementDataStorage::FQueryConditions& Conditions) const = 0;
	
	/**
	 * @section Query
	 * @description
	 * Queries can be constructed using the Query Builder. Note that the Query Builder allows for the creation of queries that
	 * are more complex than the back-end may support. The back-end is allowed to simplify the query, in which case the query
	 * can be used directly in the processor to do additional filtering. This will however impact performance and it's 
	 * therefore recommended to try to simplify the query first before relying on extended query filtering in a processor.
	 */

	using EQueryTickPhase = TypedElementDataStorage::EQueryTickPhase;
	using EQueryTickGroups = TypedElementDataStorage::EQueryTickGroups;
	using EQueryCallbackType = TypedElementDataStorage::EQueryCallbackType;
	using EQueryAccessType = TypedElementDataStorage::EQueryAccessType;
	using EQueryDependencyFlags = TypedElementDataStorage::EQueryDependencyFlags;
	using FQueryResult = TypedElementDataStorage::FQueryResult;

	using IQueryContext = TypedElementDataStorage::IQueryContext;
	using IDirectQueryContext = TypedElementDataStorage::IDirectQueryContext;
	using ISubqueryContext = TypedElementDataStorage::ISubqueryContext;

	using FQueryDescription = TypedElementDataStorage::FQueryDescription;
	using QueryCallback = TypedElementDataStorage::QueryCallback;
	using QueryCallbackRef = TypedElementDataStorage::QueryCallbackRef;
	using DirectQueryCallback = TypedElementDataStorage::DirectQueryCallback;
	using DirectQueryCallbackRef = TypedElementDataStorage::DirectQueryCallbackRef;
	using SubqueryCallback = TypedElementDataStorage::SubqueryCallback;
	using SubqueryCallbackRef = TypedElementDataStorage::SubqueryCallbackRef;

	/** 
	 * Registers a query with the data storage. The description is processed into an internal format and may be changed. If no valid
	 * could be created an invalid query handle will be returned. It's recommended to use the Query Builder for a more convenient
	 * and safer construction of a query.
	 */
	virtual TypedElementQueryHandle RegisterQuery(FQueryDescription&& Query) = 0;
	/** Removes a previous registered. If the query handle is invalid or the query has already been deleted nothing will happen. */
	virtual void UnregisterQuery(TypedElementQueryHandle Query) = 0;
	/** Returns the description of a previously registered query. If the query no longer exists an empty description will be returned. */
	virtual const FQueryDescription& GetQueryDescription(TypedElementQueryHandle Query) const = 0;
	/**
	 * Tick groups for queries can be given any name and the Data Storage will figure out the order of execution based on found
	 * dependencies. However keeping processors within the same query group can help promote better performance through parallelization.
	 * Therefore a collection of common tick group names is provided to help create consistent tick group names.
	 */
	virtual FName GetQueryTickGroupName(EQueryTickGroups Group) const = 0;
	/** Directly runs a query. If the query handle is invalid or has been deleted nothing will happen. */
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query) = 0;
	/**
	 * Directly runs a query. The callback will be called for batches of matching rows. During a single call to RunQuery the callback
	 * may be called multiple times. If the query handle is invalid or has been deleted nothing happens and the callback won't be called
	 */
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query, DirectQueryCallbackRef Callback) = 0;
	/**
	 * Triggers all queries registered under the activation name to run for one update cycle. The activatable queries will be activated at
	 * start of the cycle and disabled at the end of the cycle and act like regular queries for that cycle. This includes not running
	 * if there are no columns to match against.
	 */
	virtual void ActivateQueries(FName ActivationName) = 0;
	
	/**
	 * @section Indexing
	 * @description
	 * In order for rows to reference each other it's often needed to find a row based on the content of one of its columns. This can be
	 * done by linearly searching through columns, though this comes at a performance cost. As an alternative the data storage allows
	 * one or more indexes to be created for a row. An index is a 64-bit value and typically uses a hash value of an identifying value.
	 */

	/** Retrieves the row for an indexed object. Returns an invalid row handle if the hash wasn't found. */
	virtual TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const = 0;
	/** 
	 * Registers a row under the index hash. The same row can be registered multiple, but an index hash can only be associated 
	 * with a single row.
	 */
	virtual void IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row) = 0;
	/**
	 * Register multiple rows under their index hash. The same row can be registered multiple times,
	 * but an index hash can only be associated with a single row.
	 */
	virtual void BatchIndexRows(
		TConstArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexRowPairs) = 0;
	/** Updates the index of a row to a new value. Effectively this is the same as removing an index and adding a new one. */
	virtual void ReindexRow(
		TypedElementDataStorage::IndexHash OriginalIndex, TypedElementDataStorage::IndexHash NewIndex, TypedElementDataStorage::RowHandle Row) = 0;
	/** Removes a previously registered index hash from the index lookup table or does nothing if the hash no longer exists. */
	virtual void RemoveIndex(TypedElementDataStorage::IndexHash Index) = 0;

	/**
	 * @section Miscellaneous
	 */
	
	/**
	 * Called periodically when the storage is available. This provides an opportunity to do any repeated processing
	 * for the data storage.
	 */
	virtual FTypedElementOnDataStorageUpdate& OnUpdate() = 0;
	/**
	 * Called periodically when the storage is available. This provides an opportunity clean up after processing and
	 * to get ready for the next batch up updates.
	 */
	virtual FTypedElementOnDataStorageUpdate& OnUpdateCompleted() = 0;

	/**
	 * Whether or not the data storage is available. The data storage is available most of the time, but can be
	 * unavailable for a brief time between being destroyed and a new one created.
	 */
	virtual bool IsAvailable() const = 0;

	/** Returns a pointer to the registered external system if found, otherwise null. */
	virtual void* GetExternalSystemAddress(UClass* Target) = 0;
	/** Returns a pointer to the registered external system if found, otherwise null. */
	template<typename SystemType>
	SystemType* GetExternalSystem();

	/** Check if a custom extension is supported. This can be used to check for in-development features, custom extensions, etc. */
	virtual bool SupportsExtension(FName Extension) const = 0;
	/** Provides a list of all extensions that are enabled. */
	virtual void ListExtensions(TFunctionRef<void(FName)> Callback) const = 0;
};

// Implementations

template <typename FactoryT>
const FactoryT* ITypedElementDataStorageInterface::FindFactory() const
{
	return static_cast<const FactoryT*>(FindFactory(FactoryT::StaticClass()));
}

template<TypedElementDataStorage::TColumnType... Columns>
TypedElementDataStorage::TableHandle ITypedElementDataStorageInterface::RegisterTable(const FName Name)
{
	return RegisterTable({ Columns::StaticStruct()... }, Name);
}

template<TypedElementDataStorage::TColumnType... Columns>
TypedElementDataStorage::TableHandle ITypedElementDataStorageInterface::RegisterTable(
	TypedElementDataStorage::TableHandle SourceTable, const FName Name)
{
	return RegisterTable(SourceTable, { Columns::StaticStruct()... }, Name);
}

template<TypedElementDataStorage::TColumnType Column>
void ITypedElementDataStorageInterface::AddColumn(TypedElementRowHandle Row)
{
	AddColumn(Row, Column::StaticStruct());
}

template<TypedElementDataStorage::TColumnType Column>
void ITypedElementDataStorageInterface::RemoveColumn(TypedElementRowHandle Row)
{
	RemoveColumn(Row, Column::StaticStruct());
}

template<TypedElementDataStorage::TColumnType... Columns>
void ITypedElementDataStorageInterface::AddColumns(TypedElementRowHandle Row)
{
	AddColumns(Row, { Columns::StaticStruct()...});
}

template <>
inline void ITypedElementDataStorageInterface::AddColumn<UE::Editor::DataStorage::FDynamicTag>(TypedElementDataStorage::RowHandle Row, const FName& Tag, const FName& Value)
{
	AddColumn(Row, UE::Editor::DataStorage::FDynamicTag(Tag), Value);
}

template <>
inline void ITypedElementDataStorageInterface::RemoveColumn<UE::Editor::DataStorage::FDynamicTag>(TypedElementDataStorage::RowHandle Row, const FName& Tag)
{
	using namespace UE::Editor::DataStorage;
	RemoveColumn(Row, FDynamicTag(Tag));
}

template<TypedElementDataStorage::TEnumType EnumT>
void ITypedElementDataStorageInterface::AddColumn(TypedElementDataStorage::RowHandle Row, EnumT Value)
{
	const UEnum* Enum = StaticEnum<EnumT>();
	const FName ValueAsFName = *Enum->GetNameStringByValue(static_cast<int64>(Value));
	if (ValueAsFName != NAME_None)
	{
		AddColumn(Row, UE::Editor::DataStorage::FDynamicTag(Enum->GetFName()), ValueAsFName);
	}
}

template<auto Value, TypedElementDataStorage::TEnumType EnumT>
void ITypedElementDataStorageInterface::AddColumn(TypedElementDataStorage::RowHandle Row)
{
	AddColumn<EnumT>(Row, Value);
}

template<TypedElementDataStorage::TEnumType EnumT>
void ITypedElementDataStorageInterface::RemoveColumn(TypedElementDataStorage::RowHandle Row)
{
	const UEnum* Enum = StaticEnum<EnumT>();
	RemoveColumn(Row, UE::Editor::DataStorage::FDynamicTag(Enum->GetFName()));
}

template<TypedElementDataStorage::TColumnType... Columns>
void ITypedElementDataStorageInterface::RemoveColumns(TypedElementRowHandle Row)
{
	RemoveColumns(Row, { Columns::StaticStruct()...});
}

template<TypedElementDataStorage::TDataColumnType ColumnType>
void ITypedElementDataStorageInterface::AddColumn(TypedElementRowHandle Row, ColumnType&& Column)
{
	AddColumnData(Row, ColumnType::StaticStruct(),
		[&Column](void* ColumnData, const UScriptStruct&)
		{
			if constexpr (std::is_move_constructible_v<ColumnType>)
			{
				new(ColumnData) ColumnType(MoveTemp(Column));
			}
			else
			{
				new(ColumnData) ColumnType(Column);
			}
		},
		[](const UScriptStruct&, void* Destination, void* Source)
		{
			if constexpr (std::is_move_assignable_v<ColumnType>)
			{
				*reinterpret_cast<ColumnType*>(Destination) = MoveTemp(*reinterpret_cast<ColumnType*>(Source));
			}
			else
			{
				*reinterpret_cast<ColumnType*>(Destination) = *reinterpret_cast<ColumnType*>(Source);
			}
		});
}

template<TypedElementDataStorage::TDataColumnType ColumnType>
ColumnType* ITypedElementDataStorageInterface::GetColumn(TypedElementRowHandle Row)
{
	return reinterpret_cast<ColumnType*>(GetColumnData(Row, ColumnType::StaticStruct()));
}

template<TypedElementDataStorage::TDataColumnType ColumnType>
const ColumnType* ITypedElementDataStorageInterface::GetColumn(TypedElementRowHandle Row) const
{
	return reinterpret_cast<const ColumnType*>(GetColumnData(Row, ColumnType::StaticStruct()));
}

template<TypedElementDataStorage::TColumnType... ColumnType>
bool ITypedElementDataStorageInterface::HasColumns(TypedElementRowHandle Row) const
{
	return HasColumns(Row, TConstArrayView<const UScriptStruct*>({ ColumnType::StaticStruct()... }));
}

template<typename SystemType>
SystemType* ITypedElementDataStorageInterface::GetExternalSystem()
{
	return reinterpret_cast<SystemType*>(GetExternalSystemAddress(SystemType::StaticClass()));
}
