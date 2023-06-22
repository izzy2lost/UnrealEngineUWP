// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UClass;
class UObject;
class UScriptStruct;

namespace TypedElementDataStorage
{
	struct FQueryDescription; 
	struct ISubqueryContext;

	using SubqueryCallback = TFunction<void(const FQueryDescription&, ISubqueryContext&)>;
	using SubqueryCallbackRef = TFunctionRef<void(const FQueryDescription&, ISubqueryContext&)>;

	/**
	 * Base interface for any contexts provided to query callbacks.
	 */
	struct ICommonQueryContext
	{
		virtual ~ICommonQueryContext() = default;

		/** Return the address of a immutable column matching the requested type or a nullptr if not found. */
		virtual const void* GetColumn(const UScriptStruct* ColumnType) const = 0;
		/** Return the address of a mutable column matching the requested type or a nullptr if not found. */
		virtual void* GetMutableColumn(const UScriptStruct* ColumnType) = 0;
		/**
		 * Get a list of columns or nullptrs if the column type wasn't found. Mutable addresses are returned and it's up to
		 * the caller to not change immutable addresses.
		 */
		virtual void GetColumns(TArrayView<char*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes,
			TConstArrayView<EQueryAccessType> AccessTypes) = 0;
		/**
		 * Get a list of columns or nullptrs if the column type wasn't found. Mutable addresses are returned and it's up to
		 * the caller to not change immutable addresses. This version doesn't verify that the enough space is provided and
		 * it's up to the caller to guarantee the target addresses have enough space.
		 */
		virtual void GetColumnsUnguarded(int32 TypeCount, char** RetrievedAddresses, const TWeakObjectPtr<const UScriptStruct>* ColumnTypes,
			const EQueryAccessType* AccessTypes) = 0;

		/** Returns the number rows in the batch. */
		virtual uint32 GetRowCount() const = 0;
		/**
		 * Returns an immutable view that contains the row handles for all returned results. The returned size will be the same  as the
		 * value returned by GetRowCount().
		 */
		virtual TConstArrayView<RowHandle> GetRowHandles() const = 0;

		// Utility functions

		template<typename Column>
		const Column* GetColumn() const;
		template<typename Column>
		Column* GetMutableColumn();
	};

	/**
	 * Interface to be provided to query callbacks that are directly called through RunQuery from outside a query callback.
	 */
	struct IDirectQueryContext : public ICommonQueryContext
	{
		virtual ~IDirectQueryContext() = default;
	};

	/**
	 * Interface to be provided to query callbacks that are directly called through from a query callback.
	 */
	struct ISubqueryContext : public ICommonQueryContext
	{
		virtual ~ISubqueryContext() = default;
	};

	/**
	 * Interface to be provided to query callbacks running with the Data Storage.
	 * Note that at the time of writing only subclasses of Subsystem are supported as dependencies.
	 */
	struct IQueryContext : public ICommonQueryContext
	{
		virtual ~IQueryContext() = default;

		/** Returns an immutable instance of the requested dependency or a nullptr if not found. */
		virtual const UObject* GetDependency(const UClass* DependencyClass) = 0;
		/** Returns a mutable instance of the requested dependency or a nullptr if not found. */
		virtual UObject* GetMutableDependency(const UClass* DependencyClass) = 0;
		/**
		 * Returns a list of dependencies or nullptrs if a dependency wasn't found. Mutable versions are return and it's up to the
		 * caller to not change immutable dependencies.
		 */
		virtual void GetDependencies(TArrayView<UObject*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UClass>> DependencyTypes,
			TConstArrayView<EQueryAccessType> AccessTypes) = 0;

		/**
		 * Removes the row with the provided row handle. The removal will not be immediately done but delayed until the end of the tick
		 * group.
		 */
		virtual void RemoveRow(RowHandle Row) = 0;
		/**
		 * Removes rows with the provided row handles. The removal will not be immediately done but delayed until the end of the tick
		 * group.
		 */
		virtual void RemoveRows(TConstArrayView<RowHandle> Rows) = 0;

		/**
		 * Adds new empty columns to a row of the provided type. The addition will not be immediately done but delayed until the end of the
		 * tick group.
		 */
		virtual void AddColumns(RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) = 0;
		/**
		 * Adds new empty columns to the listed rows of the provided type. The addition will not be immediately done but delayed until the end of the
		 * tick group.
		 */
		virtual void AddColumns(TConstArrayView<RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) = 0;
		/**
		 * Removes columns of the provided types from a row. The removal will not be immediately done but delayed until the end of the
		 * tick group.
		 */
		virtual void RemoveColumns(RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) = 0;
		/**
		 * Removes columns of the provided types from the listed rows. The removal will not be immediately done but delayed until the end of the
		 * tick group.
		 */
		virtual void RemoveColumns(TConstArrayView<RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) = 0;

		/**
		 * Runs a previously created query. This version takes an arbitrary query, but is limited to running queries that do not directly
		 * access data from rows such as count queries.
		 * The returned result is a snap shot and values may change between phases.
		 */
		virtual FQueryResult RunQuery(QueryHandle Query) = 0;
		/** 
		 * Runs a subquery registered with the current query. The subquery index is in the order of registration with the query. Subqueries
		 * are executed as part of their parent query and are not scheduled separately.
		 */
		virtual FQueryResult RunSubquery(int32 SubqueryIndex) = 0;
		/** 
		 * Runs the provided callback on a subquery registered with the current query. The subquery index is in the order of registration 
		 * with the query. Subqueries are executed as part of their parent query and are not scheduled separately.
		 */
		virtual FQueryResult RunSubquery(int32 SubqueryIndex, SubqueryCallbackRef Callback) = 0;
		/** 
		 * Runs the provided callback on a subquery registered with the current query for the exact provided row. The subquery index is in 
		 * the order of registration with the query. If the row handle is in a table that doesn't match the selected subquery the callback
		 * will not be called. Check the count in the returned results to determine if the callback was called or not. Subqueries
		 * are executed as part of their parent query and are not scheduled separately.
		 */
		virtual FQueryResult RunSubquery(int32 SubqueryIndex, RowHandle Row, SubqueryCallbackRef Callback) = 0;



		// Utility functions

		template<typename... Columns>
		void AddColumns(RowHandle Row);
		template<typename... Columns>
		void AddColumns(TConstArrayView<RowHandle> Rows);
		template<typename... Columns>
		void RemoveColumns(RowHandle Row);
		template<typename... Columns>
		void RemoveColumns(TConstArrayView<RowHandle> Rows);
	};
} // namespace TypedElementDataStorage




//
// Implementations
//

namespace TypedElementDataStorage
{
	template<typename Column>
	const Column* ICommonQueryContext::GetColumn() const
	{
		return reinterpret_cast<const Column*>(GetColumn(Column::StaticStruct()));
	}

	template<typename Column>
	Column* ICommonQueryContext::GetMutableColumn()
	{
		return reinterpret_cast<Column*>(GetMutableColumn(Column::StaticStruct()));
	}

	template<typename... Columns>
	void IQueryContext::AddColumns(RowHandle Row)
	{
		AddColumns(Row, { Columns::StaticStruct()... });
	}

	template<typename... Columns>
	void IQueryContext::AddColumns(TConstArrayView<RowHandle> Rows)
	{
		AddColumns(Rows, { Columns::StaticStruct()... });
	}

	template<typename... Columns>
	void IQueryContext::RemoveColumns(RowHandle Row)
	{
		RemoveColumns(Row, { Columns::StaticStruct()... });
	}

	template<typename... Columns>
	void IQueryContext::RemoveColumns(TConstArrayView<RowHandle> Rows)
	{
		RemoveColumns(Rows, { Columns::StaticStruct()... });
	}
} // namespace TypedElementDataStorage
