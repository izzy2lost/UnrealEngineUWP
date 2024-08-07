// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassEntityQuery.h"
#include "MassRequirements.h"
#include "TypedElementHandleStore.h"
#include "UObject/StrongObjectPtr.h"

struct FMassEntityManager;
struct FMassProcessingPhaseManager;
class UMassProcessor;
class FOutputDevice;

namespace UE::Editor::DataStorage
{
	class FEnvironment;

	struct FExtendedQuery
	{
		FMassEntityQuery NativeQuery; // Used if there's no processor bound.
		ITypedElementDataStorageInterface::FQueryDescription Description;
		TStrongObjectPtr<UMassProcessor> Processor;
	};

	/**
	 * Storage and utilities for Typed Element queries after they've been processed by the Data Storage implementation.
	 */
	class FExtendedQueryStore
	{
	private:
		using QueryStore = THandleStore<FExtendedQuery>;
	public:
		using Handle = QueryStore::Handle;
		using ListAliveEntriesConstCallback = QueryStore::ListAliveEntriesConstCallback;

		/**
		 * @section Registration
		 * @description A set of functions to manage the registration of queries.
		 */

		 /** Adds a new query to the store and initializes the query with the provided arguments. */
		Handle RegisterQuery(
			ITypedElementDataStorageInterface::FQueryDescription Query,
			FEnvironment& Environment,
			FMassEntityManager& EntityManager,
			FMassProcessingPhaseManager& PhaseManager);
		/** Removes the query at the given handle if still alive and otherwise does nothing. */
		void UnregisterQuery(Handle Query, FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager);

		/** Removes all data in the query store. */
		void Clear(FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager);

		/** Register the defaults for a tick group. These will be applied on top of any settings provided with a query registration. */
		void RegisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase,
			FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread);
		/** Removes a previously registered set of tick group defaults. */
		void UnregisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase);

		/**
		 * @section Retrieval
		 * @description Functions to retrieve data or information on queries.
		 */

		 /** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
		FExtendedQuery* Get(Handle Entry);
		/** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
		FExtendedQuery* GetMutable(Handle Entry);
		/** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
		const FExtendedQuery* Get(Handle Entry) const;

		/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
		FExtendedQuery& GetChecked(Handle Entry);
		/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
		FExtendedQuery& GetMutableChecked(Handle Entry);
		/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
		const FExtendedQuery& GetChecked(Handle Entry) const;

		/** Gets the original description used to create an extended query or an empty default if the provided query isn't alive. */
		const ITypedElementDataStorageInterface::FQueryDescription& GetQueryDescription(Handle Query) const;

		/** Checks to see if a query is still available or has been removed. */
		bool IsAlive(Handle Entry) const;

		/** Calls the provided callback for each query that's available. */
		void ListAliveEntries(const ListAliveEntriesConstCallback& Callback) const;

		/**
		 * @section activatable queries
		 * @description Functions to manipulate activatable queries
		 */

		 /** Update the active activatable queries. In practice this means decrementing any active queries that automatically decrement. */
		void UpdateActivatableQueries();
		/** Triggers a query to run for a single update cycle. */
		void ActivateQueries(FName ActivationName);

		/**
		 * @section Execution
		 * @description Various functions to run queries.
		 */

		TypedElementDataStorage::FQueryResult RunQuery(FMassEntityManager& EntityManager, Handle Query);
		TypedElementDataStorage::FQueryResult RunQuery(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			Handle Query,
			TypedElementDataStorage::DirectQueryCallbackRef Callback);
		TypedElementDataStorage::FQueryResult RunQuery(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			FMassExecutionContext& ParentContext,
			Handle Query,
			TypedElementDataStorage::SubqueryCallbackRef Callback);
		TypedElementDataStorage::FQueryResult RunQuery(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			FMassExecutionContext& ParentContext,
			Handle Query,
			TypedElementRowHandle Row,
			TypedElementDataStorage::SubqueryCallbackRef Callback);
		void RunPhasePreambleQueries(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			ITypedElementDataStorageInterface::EQueryTickPhase Phase,
			float DeltaTime);
		void RunPhasePostambleQueries(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			ITypedElementDataStorageInterface::EQueryTickPhase Phase,
			float DeltaTime);

		void DebugPrintQueryCallbacks(FOutputDevice& Output) const;

	private:
		using QueryTickPhaseType = std::underlying_type_t<ITypedElementDataStorageInterface::EQueryTickPhase>;
		static constexpr QueryTickPhaseType MaxTickPhase = static_cast<QueryTickPhaseType>(ITypedElementDataStorageInterface::EQueryTickPhase::Max);

		struct FTickGroupId
		{
			FName Name;
			ITypedElementDataStorageInterface::EQueryTickPhase Phase;

			friend inline uint32 GetTypeHash(const FTickGroupId& Id) { return HashCombine(GetTypeHash(Id.Name), GetTypeHash(Id.Phase)); }
			friend inline bool operator==(const FTickGroupId& Lhs, const FTickGroupId& Rhs) { return Lhs.Phase == Rhs.Phase && Lhs.Name == Rhs.Name; }
			friend inline bool operator!=(const FTickGroupId& Lhs, const FTickGroupId& Rhs) { return Lhs.Phase != Rhs.Phase || Lhs.Name != Rhs.Name; }
		};

		struct FTickGroupDescription
		{
			TArray<FName> BeforeGroups;
			TArray<FName> AfterGroups;
			bool bRequiresMainThread{ false };
		};

		template<typename CallbackReference>
		TypedElementDataStorage::FQueryResult RunQueryCallbackCommon(
			FMassEntityManager& EntityManager,
			FEnvironment& Environment,
			FMassExecutionContext* ParentContext,
			Handle Query,
			CallbackReference Callback);

		FMassEntityQuery& SetupNativeQuery(ITypedElementDataStorageInterface::FQueryDescription& Query, FExtendedQuery& StoredQuery);
		bool SetupSelectedColumns(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
		bool SetupConditions(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
		bool SetupChunkFilters(Handle QueryHandle, ITypedElementDataStorageInterface::FQueryDescription& Query, FEnvironment& Environment, FMassEntityQuery& NativeQuery);
		bool SetupDependencies(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
		bool SetupTickGroupDefaults(ITypedElementDataStorageInterface::FQueryDescription& Query);
		bool SetupProcessors(Handle QueryHandle, FExtendedQuery& StoredQuery, FEnvironment& Environment,
			FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager);
		bool SetupActivatable(Handle QueryHandle, ITypedElementDataStorageInterface::FQueryDescription& Query);

		EMassFragmentAccess ConvertToNativeAccessType(ITypedElementDataStorageInterface::EQueryAccessType AccessType);
		EMassFragmentPresence ConvertToNativePresenceType(ITypedElementDataStorageInterface::EQueryAccessType AccessType);

		void RegisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
		void RegisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
		void UnregisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
		void UnregisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
		void RunPhasePreOrPostAmbleQueries(FMassEntityManager& EntityManager, FEnvironment& Environment,
			ITypedElementDataStorageInterface::EQueryTickPhase Phase, float DeltaTime, TArray<Handle>& QueryHandles);

		void UnregisterQueryData(Handle Query, FExtendedQuery& QueryData, FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager);

		static const ITypedElementDataStorageInterface::FQueryDescription EmptyDescription;

		QueryStore Queries;
		TMultiMap<FName, Handle> ActivatableMapping;
		TMap<FTickGroupId, FTickGroupDescription> TickGroupDescriptions;
		TArray<Handle> PhasePreparationQueries[MaxTickPhase];
		TArray<Handle> PhaseFinalizationQueries[MaxTickPhase];
		TArray<Handle> PendingActivatables;
		TArray<Handle> ActiveActivatables;
	};
} // namepsace UE::Editor::DataStorage
