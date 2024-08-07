// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/TypedElementProcessorAdaptors.h"

#include <utility>
#include "Elements/Common/TypedElementQueryTypes.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "Queries/TypedElementExtendedQueryStore.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseEnvironment.h"

namespace UE::Editor::DataStorage
{
	namespace Processors::Private
	{
		struct FMassContextCommon
		{
			FMassExecutionContext& Context;
	
			explicit FMassContextCommon(FMassExecutionContext& InContext)
				: Context(InContext)
			{}
	
			uint32 GetRowCount() const
			{
				return Context.GetNumEntities();
			}

			TConstArrayView<TypedElementRowHandle> GetRowHandles() const
			{
				static_assert(
					sizeof(TypedElementRowHandle) == sizeof(FMassEntityHandle) && alignof(TypedElementRowHandle) == alignof(FMassEntityHandle),
					"TypedElementRowHandle and FMassEntityHandle need to by layout compatible to support TEDS.");
				TConstArrayView<FMassEntityHandle> Entities = Context.GetEntities();
				return TConstArrayView<TypedElementRowHandle>(reinterpret_cast<const TypedElementRowHandle*>(Entities.GetData()), Entities.Num());
			}
	
			const void* GetColumn(const UScriptStruct* ColumnType) const
			{
				return Context.GetFragmentView(ColumnType).GetData();
			}

			void* GetMutableColumn(const UScriptStruct* ColumnType)
			{
				return Context.GetMutableFragmentView(ColumnType).GetData();
			}

			void GetColumns(TArrayView<char*> RetrievedAddresses,
				TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes,
				TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes)
			{
				checkf(RetrievedAddresses.Num() == ColumnTypes.Num(), TEXT("Unable to retrieve a batch of columns as the number of addresses "
					"doesn't match the number of requested column."));
				checkf(RetrievedAddresses.Num() == AccessTypes.Num(), TEXT("Unable to retrieve a batch of columns as the number of addresses "
					"doesn't match the number of access types."));

				GetColumnsUnguarded(ColumnTypes.Num(), RetrievedAddresses.GetData(), ColumnTypes.GetData(), AccessTypes.GetData());
			}
	
			void GetColumnsUnguarded(
				int32 TypeCount,
				char** RetrievedAddresses,
				const TWeakObjectPtr<const UScriptStruct>* ColumnTypes,
				const TypedElementDataStorage::EQueryAccessType* AccessTypes)
			{
				for (int32 Index = 0; Index < TypeCount; ++Index)
				{
					checkf(ColumnTypes->IsValid(), TEXT("Attempting to retrieve a column that is not available."));
					*RetrievedAddresses = *AccessTypes == ITypedElementDataStorageInterface::EQueryAccessType::ReadWrite
						? reinterpret_cast<char*>(Context.GetMutableFragmentView(ColumnTypes->Get()).GetData())
						: const_cast<char*>(reinterpret_cast<const char*>(Context.GetFragmentView(ColumnTypes->Get()).GetData()));

					++RetrievedAddresses;
					++ColumnTypes;
					++AccessTypes;
				}
			}

			bool HasColumn(const UScriptStruct* ColumnType) const
			{
				if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
				{
					return Context.DoesArchetypeHaveTag(*ColumnType);
				}
				if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
				{
					return Context.DoesArchetypeHaveFragment(*ColumnType);
				}
				const bool bIsTagOrFragment = false;
				checkf(bIsTagOrFragment, TEXT("Attempting to check for a column type that is not a column or tag."));
				return false;
			}

			bool HasColumn(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) const
			{
				FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
				FMassEntityManager& Manager = Context.GetEntityManagerChecked();
				FMassArchetypeHandle Archetype = Manager.GetArchetypeForEntity(Entity);
				const FMassArchetypeCompositionDescriptor& Composition = Manager.GetArchetypeComposition(Archetype);

				if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
				{
					return Composition.Tags.Contains(*ColumnType);
				}
				if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
				{
					return Composition.Fragments.Contains(*ColumnType);
				}
				const bool bIsTagOrFragment = false;
				checkf(bIsTagOrFragment, TEXT("Attempting to check for a column type that is not a column or tag."));
				return false;
			}
		};

		struct FMassWithEnvironmentContextCommon : public FMassContextCommon
		{
			using Parent = FMassContextCommon;
		protected:
			void TedsColumnsToMassDescriptorIfActiveTable(
				FMassArchetypeCompositionDescriptor& Descriptor,
				TConstArrayView<const UScriptStruct*> ColumnTypes)
			{
				for (const UScriptStruct* ColumnType : ColumnTypes)
				{
					if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
					{
						if (this->Context.DoesArchetypeHaveTag(*ColumnType))
						{
							Descriptor.Tags.Add(*ColumnType);
						}
					}
					else
					{
						checkf(ColumnType->IsChildOf(FMassFragment::StaticStruct()),
							TEXT("Given struct type is not a valid fragment or tag type."));
						if (this->Context.DoesArchetypeHaveFragment(*ColumnType))
						{
							Descriptor.Fragments.Add(*ColumnType);
						}
					}
				}
			}

			void TedsColumnsToMassDescriptor(
				FMassArchetypeCompositionDescriptor& Descriptor,
				TConstArrayView<const UScriptStruct*> ColumnTypes)
			{
				for (const UScriptStruct* ColumnType : ColumnTypes)
				{
					if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
					{
						Descriptor.Tags.Add(*ColumnType);
					}
					else
					{
						checkf(ColumnType->IsChildOf(FMassFragment::StaticStruct()),
							TEXT("Given struct type is not a valid fragment or tag type."));
						Descriptor.Fragments.Add(*ColumnType);

					}
				}
			}

		public:
			using ObjectCopyOrMove = void (*)(const UScriptStruct& TypeInfo, void* Destination, void* Source);
	
			FMassWithEnvironmentContextCommon(FMassExecutionContext& InContext, FEnvironment& InEnvironment)
				: FMassContextCommon(InContext)
				, Environment(InEnvironment)
			{}

			uint64 GetUpdateCycleId() const 
			{
				return Environment.GetUpdateCycleId();
			}

			bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const
			{
				return Environment.GetMassEntityManager().IsEntityValid(FMassEntityHandle::FromNumber(Row));
			}

			bool IsRowAssigned(TypedElementDataStorage::RowHandle Row) const
			{
				return Environment.GetMassEntityManager().IsEntityActive(FMassEntityHandle::FromNumber(Row));
			}
	
			void ActivateQueries(FName ActivationName)
			{
				this->Context.Defer().template PushCommand<FMassDeferredCommand<EMassCommandOperationType::None>>(
					[Environment = &this->Environment, ActivationName](FMassEntityManager&)
					{
						Environment->GetQueryStore().ActivateQueries(ActivationName);
					});
			}

			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType) 
			{
				return AddColumnUninitialized(Row, ObjectType,
					[](const UScriptStruct& TypeInfo, void* Destination, void* Source)
					{
						TypeInfo.CopyScriptStruct(Destination, Source);
					});
			}

			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType, ObjectCopyOrMove Relocator)
			{
				checkf(ObjectType->IsChildOf(FMassFragment::StaticStruct()), TEXT("Column [%s] can not be a tag"), *ObjectType->GetName());
		
				struct FAddValueColumn
				{
					ObjectCopyOrMove Relocator;
					const UScriptStruct* FragmentType;
					FMassEntityHandle Entity;
					void* Object;

					FAddValueColumn() = default;
					FAddValueColumn(ObjectCopyOrMove InRelocator, const UScriptStruct* InFragmentType, FMassEntityHandle InEntity, void* InObject)
						: Relocator(InRelocator)
						, FragmentType(InFragmentType)
						, Entity(InEntity)
						, Object(InObject)
					{}

					~FAddValueColumn()
					{
						if ((this->FragmentType->StructFlags & (STRUCT_IsPlainOldData | STRUCT_NoDestructor)) == 0)
						{
							this->FragmentType->DestroyStruct(this->Object);
						}
					}
				};

				FScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
				void* ColumnData = ScratchBuffer.Allocate(ObjectType->GetStructureSize(), ObjectType->GetMinAlignment());
				FAddValueColumn* AddedColumn =
					ScratchBuffer.Emplace<FAddValueColumn>(Relocator, ObjectType, FMassEntityHandle::FromNumber(Row), ColumnData);
		
				this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
					[AddedColumn](FMassEntityManager& System)
					{
						// Check entity before proceeding. It's possible it may have been invalidated before this deferred call fired.
						if (System.IsEntityActive(AddedColumn->Entity))
						{
							// Check before adding.  Mass's AddFragmentToEntity is not idempotent and will assert if adding
							// column to a row that already has one
							FStructView Fragment = System.GetFragmentDataStruct(AddedColumn->Entity, AddedColumn->FragmentType);
							if (!Fragment.IsValid())
							{
								System.AddFragmentToEntity(AddedColumn->Entity, AddedColumn->FragmentType, 
									[AddedColumn](void* Fragment, const UScriptStruct& FragmentType)
									{
										AddedColumn->Relocator(FragmentType, Fragment, AddedColumn->Object);
									});
							}
							else
							{
								AddedColumn->Relocator(*AddedColumn->FragmentType, Fragment.GetMemory(), AddedColumn->Object);
							}
						}
					});
		
				return ColumnData;
			}

			void AddColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) 
			{
				struct FAddedColumns
				{
					FMassArchetypeCompositionDescriptor AddDescriptor;
					FMassEntityHandle Entity;
				};

				FAddedColumns* AddedColumns = Environment.GetScratchBuffer().template Emplace<FAddedColumns>();
				TedsColumnsToMassDescriptor(AddedColumns->AddDescriptor, ColumnTypes);
				AddedColumns->Entity = FMassEntityHandle::FromNumber(Row);

				this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
					[AddedColumns](FMassEntityManager& System)
					{
						if (System.IsEntityValid(AddedColumns->Entity))
						{
							System.AddCompositionToEntity_GetDelta(AddedColumns->Entity, AddedColumns->AddDescriptor);
						}
					});
			}

			void AddColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) 
			{
				struct FAddedColumns
				{
					FMassArchetypeCompositionDescriptor AddDescriptor;
					FMassEntityHandle* Entities;
					int32 EntityCount;
				};

				FScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
				FAddedColumns* AddedColumns = ScratchBuffer.Emplace<FAddedColumns>();
				TedsColumnsToMassDescriptor(AddedColumns->AddDescriptor, ColumnTypes);
		
				FMassEntityHandle* Entities = ScratchBuffer.EmplaceArray<FMassEntityHandle>(Rows.Num());
				AddedColumns->Entities = Entities;
				for (TypedElementRowHandle Row : Rows)
				{
					*Entities = FMassEntityHandle::FromNumber(Row);
					Entities++;
				}
				AddedColumns->EntityCount = Rows.Num();

				this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
					[AddedColumns](FMassEntityManager& System)
					{
						FMassEntityHandle* Entities = AddedColumns->Entities;
						int32 Count = AddedColumns->EntityCount;
						for (int32 Counter = 0; Counter < Count; ++Counter)
						{
							if (System.IsEntityValid(*Entities))
							{
								System.AddCompositionToEntity_GetDelta(*Entities++, AddedColumns->AddDescriptor);
							}
						}
					});
			}

			void RemoveColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) 
			{
				struct FRemovedColumns
				{
					FMassArchetypeCompositionDescriptor RemoveDescriptor;
					FMassEntityHandle Entity;
				};

				FRemovedColumns* RemovedColumns = Environment.GetScratchBuffer().template Emplace<FRemovedColumns>();
				TedsColumnsToMassDescriptorIfActiveTable(RemovedColumns->RemoveDescriptor, ColumnTypes);
				if (!RemovedColumns->RemoveDescriptor.IsEmpty())
				{
					RemovedColumns->Entity = FMassEntityHandle::FromNumber(Row);

					this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
						[RemovedColumns](FMassEntityManager& System)
						{
							if (System.IsEntityValid(RemovedColumns->Entity))
							{
								System.RemoveCompositionFromEntity(RemovedColumns->Entity, RemovedColumns->RemoveDescriptor);
							}
						});
				}
			}

			void RemoveColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) 
			{
				struct FRemovedColumns
				{
					FMassArchetypeCompositionDescriptor RemoveDescriptor;
					FMassEntityHandle* Entities;
					int32 EntityCount;
				};

				FScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
				FRemovedColumns* RemovedColumns = ScratchBuffer.Emplace<FRemovedColumns>();
				TedsColumnsToMassDescriptorIfActiveTable(RemovedColumns->RemoveDescriptor, ColumnTypes);

				FMassEntityHandle* Entities = ScratchBuffer.EmplaceArray<FMassEntityHandle>(Rows.Num());
				RemovedColumns->Entities = Entities;
				for (TypedElementRowHandle Row : Rows)
				{
					*Entities = FMassEntityHandle::FromNumber(Row);
					Entities++;
				}
				RemovedColumns->EntityCount = Rows.Num();

				this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
					[RemovedColumns](FMassEntityManager& System)
					{
						FMassEntityHandle* Entities = RemovedColumns->Entities;
						int32 Count = RemovedColumns->EntityCount;

						using EntityHandleArray = TArray<FMassEntityHandle, TInlineAllocator<32>>;
						using EntityArchetypeLookup = TMap<FMassArchetypeHandle, EntityHandleArray, TInlineSetAllocator<32>>;
						using ArchetypeEntityArray = TArray<FMassArchetypeEntityCollection, TInlineAllocator<32>>;

						// Sort rows (entities) into to matching table (archetype) bucket.
						EntityArchetypeLookup LookupTable;
						for (int32 Counter = 0; Counter < Count; ++Counter)
						{
							if (System.IsEntityValid(*Entities))
							{
								FMassArchetypeHandle Archetype = System.GetArchetypeForEntity(*Entities);
								EntityHandleArray& EntityCollection = LookupTable.FindOrAdd(Archetype);
								EntityCollection.Add(*Entities);
							}
							Entities++;
						}

						// Construct table (archetype) specific row (entity) collections.
						ArchetypeEntityArray EntityCollections;
						EntityCollections.Reserve(LookupTable.Num());
						for (auto It = LookupTable.CreateConstIterator(); It; ++It)
						{
							// Could be more effective but the previous implementation was robust when called with duplicate rows.
							EntityCollections.Emplace(It.Key(), It.Value(), FMassArchetypeEntityCollection::EDuplicatesHandling::FoldDuplicates);
						}


						// This could be improved by adding an operation that would both combine the Fragments and Tags change in one bath operation.
						if (!RemovedColumns->RemoveDescriptor.Fragments.IsEmpty())
						{
							System.BatchChangeFragmentCompositionForEntities(EntityCollections, FMassFragmentBitSet(), RemovedColumns->RemoveDescriptor.Fragments);
						}
						if (!RemovedColumns->RemoveDescriptor.Tags.IsEmpty())
						{
							System.BatchChangeTagsForEntities(EntityCollections, FMassTagBitSet(), RemovedColumns->RemoveDescriptor.Tags);
						}
					});
			}
	
			TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table) 
			{
				FMassEntityHandle EntityHandle = Environment.GetMassEntityManager().ReserveEntity();
				FMassArchetypeHandle ArchetypeHandle = Environment.LookupMassArchetype(Table);

				if (!ArchetypeHandle.IsValid())
				{
					return TypedElementDataStorage::InvalidRowHandle;
				}
		
				struct CommandInfo
				{
					FMassEntityHandle Entity;
					FMassArchetypeHandle Archetype;
				};

				CommandInfo CommandDataTmp{
					.Entity = EntityHandle,
					.Archetype = MoveTemp(ArchetypeHandle)
				};

				this->Context.Defer().template PushCommand<FMassDeferredCreateCommand>(
					[CommandData = MoveTemp(CommandDataTmp)](FMassEntityManager& System)
					{
						const FMassArchetypeSharedFragmentValues SharedFragmentValues;
						System.BuildEntity(CommandData.Entity, CommandData.Archetype, SharedFragmentValues);
					});
		
				const TypedElementDataStorage::RowHandle TedsRowHandle = EntityHandle.AsNumber();
				return TedsRowHandle;
			}

			void RemoveRow(TypedElementRowHandle Row) 
			{
				this->Context.Defer().DestroyEntity(FMassEntityHandle::FromNumber(Row));
			}

			void RemoveRows(TConstArrayView<TypedElementRowHandle> Rows) 
			{
				// Row handles and entities map 1:1 for data, so a reintpret_cast can be safely done to avoid
				// having to allocate memory and iterating over the rows.

				static_assert(sizeof(FMassEntityHandle) == sizeof(TypedElementRowHandle), 
					"Sizes of mass entity and data storage row have gone out of sync.");
				static_assert(alignof(FMassEntityHandle) == alignof(TypedElementRowHandle),
					"Alignment of mass entity and data storage row have gone out of sync.");

				this->Context.Defer().DestroyEntities(
					TConstArrayView<FMassEntityHandle>(reinterpret_cast<const FMassEntityHandle*>(Rows.begin()), Rows.Num()));
			}

		protected:
			FEnvironment& Environment;
		};

		struct FMassDirectContextForwarder final : public ITypedElementDataStorageInterface::IDirectQueryContext
		{
			explicit FMassDirectContextForwarder(FMassExecutionContext& InContext)
				: Implementation(InContext)
			{}
	
			uint32 GetRowCount() const override { return Implementation.GetRowCount(); }
			TConstArrayView<TypedElementDataStorage::RowHandle> GetRowHandles() const override { return Implementation.GetRowHandles(); }
			const void* GetColumn(const UScriptStruct* ColumnType) const override { return Implementation.GetColumn(ColumnType); }
			void* GetMutableColumn(const UScriptStruct* ColumnType) override { return Implementation.GetMutableColumn(ColumnType); }
			void GetColumns(TArrayView<char*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes) override { return Implementation.GetColumns(RetrievedAddresses, ColumnTypes, AccessTypes); }
			void GetColumnsUnguarded(int32 TypeCount, char** RetrievedAddresses, const TWeakObjectPtr<const UScriptStruct>* ColumnTypes, const TypedElementDataStorage::EQueryAccessType* AccessTypes) override { return Implementation.GetColumnsUnguarded(TypeCount, RetrievedAddresses, ColumnTypes, AccessTypes); }
			bool HasColumn(const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(ColumnType); }
			bool HasColumn(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(Row, ColumnType); }

			FMassContextCommon Implementation;
		};

		struct FMassSubqueryContextForwarder  : public ITypedElementDataStorageInterface::ISubqueryContext
		{
			FMassSubqueryContextForwarder(FMassExecutionContext& InContext, FEnvironment& InEnvironment)
				: Implementation(InContext, InEnvironment)
			{}

			~FMassSubqueryContextForwarder() override = default;
			uint32 GetRowCount() const override { return Implementation.GetRowCount(); }
			TConstArrayView<TypedElementDataStorage::RowHandle> GetRowHandles() const override { return Implementation.GetRowHandles(); }
			const void* GetColumn(const UScriptStruct* ColumnType) const override { return Implementation.GetColumn(ColumnType); }
			void* GetMutableColumn(const UScriptStruct* ColumnType) override { return Implementation.GetMutableColumn(ColumnType); }
			void GetColumns(TArrayView<char*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes) override { return Implementation.GetColumns(RetrievedAddresses, ColumnTypes, AccessTypes); }
			void GetColumnsUnguarded(int32 TypeCount, char** RetrievedAddresses, const TWeakObjectPtr<const UScriptStruct>* ColumnTypes, const TypedElementDataStorage::EQueryAccessType* AccessTypes) override { return Implementation.GetColumnsUnguarded(TypeCount, RetrievedAddresses, ColumnTypes, AccessTypes); }
			bool HasColumn(const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(ColumnType); }
			bool HasColumn(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(Row, ColumnType); }
			uint64 GetUpdateCycleId() const override { return Implementation.GetUpdateCycleId(); }
			bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const override { return Implementation.IsRowAvailable(Row); }
			bool IsRowAssigned(TypedElementDataStorage::RowHandle Row) const override { return Implementation.IsRowAssigned(Row); }
			void ActivateQueries(FName ActivationName) override  { return Implementation.ActivateQueries(ActivationName); }
			TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table) override  { return Implementation.AddRow(Table); }
			void RemoveRow(TypedElementDataStorage::RowHandle Row) override { return Implementation.RemoveRow(Row); }
			void RemoveRows(TConstArrayView<TypedElementDataStorage::RowHandle> Rows) override  { return Implementation.RemoveRows(Rows); }
			void AddColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override { return Implementation.AddColumns(Row, ColumnTypes); }
			void AddColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override  { return Implementation.AddColumns(Rows, ColumnTypes); }
			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) override  { return Implementation.AddColumnUninitialized(Row, ColumnType); }
			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType, ObjectCopyOrMove Relocator) override { return Implementation.AddColumnUninitialized(Row, ObjectType, Relocator); }
			void RemoveColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override { Implementation.RemoveColumns(Row, ColumnTypes); }
			void RemoveColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override { Implementation.RemoveColumns(Rows, ColumnTypes); }

			FMassWithEnvironmentContextCommon Implementation;
		};

		struct FMassQueryContextImplementation final : FMassWithEnvironmentContextCommon
		{
			FMassQueryContextImplementation(
				ITypedElementDataStorageInterface::FQueryDescription& InQueryDescription,
				FMassExecutionContext& InContext, 
				FExtendedQueryStore& InQueryStore,
				FEnvironment& InEnvironment)
				: FMassWithEnvironmentContextCommon(InContext, InEnvironment)
				, QueryDescription(InQueryDescription)
				, QueryStore(InQueryStore)
			{
		
			}

			~FMassQueryContextImplementation() = default;

			UObject* GetMutableDependency(const UClass* DependencyClass)
			{
				return Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(DependencyClass));
			}

			const UObject* GetDependency(const UClass* DependencyClass)
			{
				return Context.GetSubsystem<USubsystem>(const_cast<UClass*>(DependencyClass));
			}
	
			void GetDependencies(TArrayView<UObject*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UClass>> SubsystemTypes,
				TConstArrayView<ITypedElementDataStorageInterface::EQueryAccessType> AccessTypes)
			{
				checkf(RetrievedAddresses.Num() == SubsystemTypes.Num(), TEXT("Unable to retrieve a batch of subsystem as the number of addresses "
					"doesn't match the number of requested subsystem types."));

				GetDependenciesUnguarded(RetrievedAddresses.Num(), RetrievedAddresses.GetData(), SubsystemTypes.GetData(), AccessTypes.GetData());
			}

			void GetDependenciesUnguarded(int32 SubsystemCount, UObject** RetrievedAddresses, const TWeakObjectPtr<const UClass>* DependencyTypes,
				const ITypedElementDataStorageInterface::EQueryAccessType* AccessTypes)
			{
				for (int32 Index = 0; Index < SubsystemCount; ++Index)
				{
					checkf(DependencyTypes->IsValid(), TEXT("Attempting to retrieve a subsystem that's no longer valid."));
					*RetrievedAddresses = *AccessTypes == ITypedElementDataStorageInterface::EQueryAccessType::ReadWrite
						? Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(DependencyTypes->Get()))
						: const_cast<USubsystem*>(Context.GetSubsystem<USubsystem>(const_cast<UClass*>(DependencyTypes->Get())));

					++RetrievedAddresses;
					++DependencyTypes;
					++AccessTypes;
				}
			}
	
			TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const
			{
				using namespace UE::Editor::DataStorage;

				EGlobalLockScope Scope = FGlobalLock::GetLockStatus(EGlobalLockScope::Internal) == EGlobalLockStatus::Unlocked
					? EGlobalLockScope::Public // There's no internal lock so use a public lock instead.
					: EGlobalLockScope::Internal; // There's an internal lock set so use that.
				return Environment.GetIndexTable().FindIndexedRow(Scope, Index);
			}

			TypedElementDataStorage::FQueryResult RunQuery(TypedElementQueryHandle Query)
			{
				const FExtendedQueryStore::Handle Handle(Query);
				// This can be safely called because there's not callback, which means no columns are accessed, even for select queries.
				return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Handle);
			}

			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex)
			{
				return SubqueryIndex < QueryDescription.Subqueries.Num() ?
					RunQuery(QueryDescription.Subqueries[SubqueryIndex]) :
					TypedElementDataStorage::FQueryResult{};
			}

			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::SubqueryCallbackRef Callback)
			{
				if (SubqueryIndex < QueryDescription.Subqueries.Num())
				{
					const TypedElementQueryHandle SubqueryHandle = QueryDescription.Subqueries[SubqueryIndex];
					const FExtendedQueryStore::Handle StorageHandle(SubqueryHandle);
					return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Environment, Context, StorageHandle, Callback);
				}
				else
				{
					return TypedElementDataStorage::FQueryResult{};
				}
			}

			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::RowHandle Row,
				TypedElementDataStorage::SubqueryCallbackRef Callback)
			{
				if (SubqueryIndex < QueryDescription.Subqueries.Num())
				{
					const TypedElementQueryHandle SubqueryHandle = QueryDescription.Subqueries[SubqueryIndex];
					const FExtendedQueryStore::Handle StorageHandle(SubqueryHandle);
					return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Environment, Context, StorageHandle, Row, Callback);
				}
				else
				{
					return TypedElementDataStorage::FQueryResult{};
				}
			}
	
			ITypedElementDataStorageInterface::FQueryDescription& QueryDescription;
			FExtendedQueryStore& QueryStore;
		};

		struct FMassContextForwarder final : public ITypedElementDataStorageInterface::IQueryContext
		{
			FMassContextForwarder(
				ITypedElementDataStorageInterface::FQueryDescription& InQueryDescription,
				FMassExecutionContext& InContext, 
				FExtendedQueryStore& InQueryStore,
				FEnvironment& InEnvironment)
					: Implementation(InQueryDescription, InContext, InQueryStore, InEnvironment)
			{}
	
			uint32 GetRowCount() const override { return Implementation.GetRowCount(); }
			TConstArrayView<TypedElementDataStorage::RowHandle> GetRowHandles() const override { return Implementation.GetRowHandles(); }
			const void* GetColumn(const UScriptStruct* ColumnType) const override { return Implementation.GetColumn(ColumnType); }
			void* GetMutableColumn(const UScriptStruct* ColumnType) override { return Implementation.GetMutableColumn(ColumnType); }
			void GetColumns(TArrayView<char*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes) override { return Implementation.GetColumns(RetrievedAddresses, ColumnTypes, AccessTypes); }
			void GetColumnsUnguarded(int32 TypeCount, char** RetrievedAddresses, const TWeakObjectPtr<const UScriptStruct>* ColumnTypes, const TypedElementDataStorage::EQueryAccessType* AccessTypes) override { return Implementation.GetColumnsUnguarded(TypeCount, RetrievedAddresses, ColumnTypes, AccessTypes); }
			bool HasColumn(const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(ColumnType); }
			bool HasColumn(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) const override { return Implementation.HasColumn(Row, ColumnType); }
			uint64 GetUpdateCycleId() const override { return Implementation.GetUpdateCycleId(); }
			bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const override { return Implementation.IsRowAvailable(Row); }
			bool IsRowAssigned(TypedElementDataStorage::RowHandle Row) const override { return Implementation.IsRowAssigned(Row); }
			void ActivateQueries(FName ActivationName) override  { return Implementation.ActivateQueries(ActivationName); }
			TypedElementDataStorage::RowHandle AddRow(TypedElementDataStorage::TableHandle Table) override  { return Implementation.AddRow(Table); }
			void RemoveRow(TypedElementDataStorage::RowHandle Row) override { return Implementation.RemoveRow(Row); }
			void RemoveRows(TConstArrayView<TypedElementDataStorage::RowHandle> Rows) override  { return Implementation.RemoveRows(Rows); }
			void AddColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override { return Implementation.AddColumns(Row, ColumnTypes); }
			void AddColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override  { return Implementation.AddColumns(Rows, ColumnTypes); }
			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) override  { return Implementation.AddColumnUninitialized(Row, ColumnType); }
			void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType, ObjectCopyOrMove Relocator) override { return Implementation.AddColumnUninitialized(Row, ObjectType, Relocator); }
			void RemoveColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override { Implementation.RemoveColumns(Row, ColumnTypes); }
			void RemoveColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override { Implementation.RemoveColumns(Rows, ColumnTypes); }
	
			const UObject* GetDependency(const UClass* DependencyClass) override { return Implementation.GetDependency(DependencyClass); }
			UObject* GetMutableDependency(const UClass* DependencyClass) override { return Implementation.GetMutableDependency(DependencyClass); }
			void GetDependencies(TArrayView<UObject*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UClass>> DependencyTypes, TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes) override { return Implementation.GetDependencies(RetrievedAddresses, DependencyTypes, AccessTypes); }
			TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const override { return Implementation.FindIndexedRow(Index); }
			TypedElementDataStorage::FQueryResult RunQuery(TypedElementDataStorage::QueryHandle Query) override { return Implementation.RunQuery(Query); }
			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex) override { return Implementation.RunSubquery(SubqueryIndex); }
			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::SubqueryCallbackRef Callback) override { return Implementation.RunSubquery(SubqueryIndex, Callback); }
			TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::RowHandle Row, TypedElementDataStorage::SubqueryCallbackRef Callback) override { return Implementation.RunSubquery(SubqueryIndex, Row, Callback); }
	
			FMassQueryContextImplementation Implementation;
		};
	} // namespace Processors::Private

	/**
	 * FPhasePreOrPostAmbleExecutor
	 */
	FPhasePreOrPostAmbleExecutor::FPhasePreOrPostAmbleExecutor(FMassEntityManager& EntityManager, float DeltaTime)
		: Context(EntityManager, DeltaTime)
	{
		Context.SetDeferredCommandBuffer(MakeShared<FMassCommandBuffer>());
	}

	FPhasePreOrPostAmbleExecutor::~FPhasePreOrPostAmbleExecutor()
	{
		Context.FlushDeferred();
	}

	void FPhasePreOrPostAmbleExecutor::ExecuteQuery(
		ITypedElementDataStorageInterface::FQueryDescription& Description,
		FExtendedQueryStore& QueryStore,
		FEnvironment& Environment,
		FMassEntityQuery& NativeQuery,
		ITypedElementDataStorageInterface::QueryCallbackRef Callback)
	{
		if (Description.Callback.ActivationCount > 0)
		{
			NativeQuery.ForEachEntityChunk(Context.GetEntityManagerChecked(), Context,
				[&Callback, &QueryStore, &Environment, &Description](FMassExecutionContext& ExecutionContext)
				{
					if (FTypedElementQueryProcessorData::PrepareCachedDependenciesOnQuery(Description, ExecutionContext))
					{
						Processors::Private::FMassContextForwarder QueryContext(Description, ExecutionContext, QueryStore, Environment);
						Callback(Description, QueryContext);
					}
				}
			);
		}
	}
} // namespace UE::Editor::DataStorage

/**
 * FTypedElementQueryProcessorData
 */

FTypedElementQueryProcessorData::FTypedElementQueryProcessorData(UMassProcessor& Owner)
	: NativeQuery(Owner)
{
}

bool FTypedElementQueryProcessorData::CommonQueryConfiguration(
	UMassProcessor& InOwner,
	FExtendedQuery& InQuery,
	FExtendedQueryStore::Handle InQueryHandle,
	FExtendedQueryStore& InQueryStore,
	FEnvironment& InEnvironment,
	TArrayView<FMassEntityQuery> Subqueries)
{
	ParentQuery = InQueryHandle;
	QueryStore = &InQueryStore;
	Environment = &InEnvironment;

	if (ensureMsgf(InQuery.Description.Subqueries.Num() <= Subqueries.Num(),
		TEXT("Provided query has too many (%i) subqueries."), InQuery.Description.Subqueries.Num()))
	{
		bool Result = true;
		int32 CurrentSubqueryIndex = 0;
		for (TypedElementQueryHandle SubqueryHandle : InQuery.Description.Subqueries)
		{
			const FExtendedQueryStore::Handle SubqueryStoreHandle(SubqueryHandle);
			if (const FExtendedQuery* Subquery = InQueryStore.Get(SubqueryStoreHandle))
			{
				if (ensureMsgf(Subquery->NativeQuery.CheckValidity(), TEXT("Provided subquery isn't valid. This can be because it couldn't be "
					"constructed properly or because it's been bound to a callback.")))
				{
					Subqueries[CurrentSubqueryIndex] = Subquery->NativeQuery;
					Subqueries[CurrentSubqueryIndex].RegisterWithProcessor(InOwner);
					++CurrentSubqueryIndex;
				}
				else
				{
					Result = false;
				}
			}
			else
			{
				Result = false;
			}
		}
		return Result;
	}
	return false;
}

EMassProcessingPhase FTypedElementQueryProcessorData::MapToMassProcessingPhase(ITypedElementDataStorageInterface::EQueryTickPhase Phase)
{
	switch(Phase)
	{
	case ITypedElementDataStorageInterface::EQueryTickPhase::PrePhysics:
		return EMassProcessingPhase::PrePhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::DuringPhysics:
		return EMassProcessingPhase::DuringPhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::PostPhysics:
		return EMassProcessingPhase::PostPhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::FrameEnd:
		return EMassProcessingPhase::FrameEnd;
	default:
		checkf(false, TEXT("Query tick phase '%i' is unsupported."), static_cast<int>(Phase));
		return EMassProcessingPhase::MAX;
	};
}

FString FTypedElementQueryProcessorData::GetProcessorName() const
{
	if (const FExtendedQuery* StoredQuery = QueryStore ? QueryStore->Get(ParentQuery) : nullptr)
	{
		return StoredQuery->Description.Callback.Name.ToString();
	}
	else
	{
		return TEXT("<unnamed>");
	}
}

void FTypedElementQueryProcessorData::DebugOutputDescription(FOutputDevice& Ar, int32 Indent) const
{
#if WITH_MASSENTITY_DEBUG
	if (const FExtendedQuery* StoredQuery = QueryStore ? QueryStore->Get(ParentQuery) : nullptr)
	{
		const ITypedElementDataStorageInterface::FQueryDescription& Description = StoredQuery->Description;
		const ITypedElementDataStorageInterface::FQueryDescription::FCallbackData& Callback = Description.Callback;
		
		if (!Callback.Group.IsNone())
		{
			Ar.Logf(TEXT("\n%*sGroup: %s"), Indent, TEXT(""), *Callback.Group.ToString());
		}
		if (!Callback.BeforeGroups.IsEmpty())
		{
			Ar.Logf(TEXT("\n%*sBefore:"), Indent, TEXT(""));
			int32 Index = 0;
			for (FName BeforeName : Callback.BeforeGroups)
			{
				Ar.Logf(TEXT("\n%*s[%i] %s"), Indent + 4, TEXT(""), Index++, *BeforeName.ToString());
			}
		}
		if (!Callback.AfterGroups.IsEmpty())
		{
			Ar.Logf(TEXT("\n%*sAfter:"), Indent, TEXT(""));
			int32 Index = 0;
			for (FName AfterName : Callback.AfterGroups)
			{
				Ar.Logf(TEXT("\n%*s[%i] %s"), Indent + 4, TEXT(""), Index++, *AfterName.ToString());
			}
		}
		
		if (!Callback.ActivationName.IsNone())
		{
			Ar.Logf(TEXT("\n%*sActivatable: %s"), Indent, TEXT(""), *Callback.ActivationName.ToString());
		}

		if (Callback.MonitoredType)
		{
			Ar.Logf(TEXT("\n%*sMonitored type: %s"), Indent, TEXT(""), *Callback.MonitoredType->GetName());
		}

		Ar.Logf(TEXT("\n%*sIs forced to GameThread: %s"), Indent, TEXT(""), Callback.bForceToGameThread ? TEXT("True") : TEXT("False"));
	}
#endif // WITH_MASSENTITY_DEBUG
}

bool FTypedElementQueryProcessorData::PrepareCachedDependenciesOnQuery(
	ITypedElementDataStorageInterface::FQueryDescription& Description, FMassExecutionContext& Context)
{
	const int32 DependencyCount = Description.DependencyTypes.Num();
	const TWeakObjectPtr<const UClass>* Types = Description.DependencyTypes.GetData();
	const ITypedElementDataStorageInterface::EQueryDependencyFlags* Flags = Description.DependencyFlags.GetData();
	TWeakObjectPtr<UObject>* Caches = Description.CachedDependencies.GetData();

	for (int32 Index = 0; Index < DependencyCount; ++Index)
	{
		checkf(Types->IsValid(), TEXT("Attempting to retrieve a dependency type that's no longer available."));
		
		if (EnumHasAnyFlags(*Flags, ITypedElementDataStorageInterface::EQueryDependencyFlags::AlwaysRefresh) || !Caches->IsValid())
		{
			*Caches = EnumHasAnyFlags(*Flags, ITypedElementDataStorageInterface::EQueryDependencyFlags::ReadOnly)
				? const_cast<USubsystem*>(Context.GetSubsystem<USubsystem>(const_cast<UClass*>(Types->Get())))
				: Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(Types->Get()));
			if (*Caches != nullptr)
			{
				++Types;
				++Flags;
				++Caches;
			}
			else
			{
				checkf(false, TEXT("Unable to retrieve instance of dependency '%s'."), *((*Types)->GetName()));
				return false;
			}
		}
	}
	return true;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::DirectQueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	FMassEntityQuery& NativeQuery, 
	FMassEntityManager& EntityManager,
	FEnvironment& Environment)
{
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;
	
	if (Description.Callback.ActivationCount > 0)
	{
		FMassExecutionContext Context(EntityManager);
		
		NativeQuery.ForEachEntityChunk(EntityManager, Context,
			[&Result, &Callback, &Description](FMassExecutionContext& Context)
			{
				// No need to cache any subsystem dependencies as these are not accessible from a direct query.
				UE::Editor::DataStorage::Processors::Private::FMassDirectContextForwarder QueryContext(Context);
				Callback(Description, QueryContext);
				Result.Count += Context.GetNumEntities();
			}
		);
	}
	return Result;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::SubqueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	FMassEntityQuery& NativeQuery,
	FMassEntityManager& EntityManager,
	FEnvironment& Environment,
	FMassExecutionContext& ParentContext)
{
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;

	if (Description.Callback.ActivationCount > 0)
	{
		FMassExecutionContext Context(EntityManager);
		Context.SetDeferredCommandBuffer(ParentContext.GetSharedDeferredCommandBuffer());
		Context.SetFlushDeferredCommands(false);

		NativeQuery.ForEachEntityChunk(EntityManager, Context,
			[&Result, &Callback, &Description, &Environment](FMassExecutionContext& Context)
			{
				// No need to cache any subsystem dependencies as these are not accessible from a subquery.
				UE::Editor::DataStorage::Processors::Private::FMassSubqueryContextForwarder QueryContext(Context, Environment);
				Callback(Description, QueryContext);
				Result.Count += Context.GetNumEntities();
			}
		);
	}
	return Result;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::SubqueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	TypedElementDataStorage::RowHandle RowHandle,
	FMassEntityQuery& NativeQuery,
	FMassEntityManager& EntityManager,
	FEnvironment& Environment,
	FMassExecutionContext& ParentContext)
{
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;

	FMassEntityHandle NativeEntity = FMassEntityHandle::FromNumber(RowHandle);
	if (Description.Callback.ActivationCount > 0 && EntityManager.IsEntityActive(NativeEntity))
	{
		FMassArchetypeHandle NativeArchetype = EntityManager.GetArchetypeForEntityUnsafe(NativeEntity);
		FMassExecutionContext Context(EntityManager);
		Context.SetEntityCollection(FMassArchetypeEntityCollection(NativeArchetype, { NativeEntity }, FMassArchetypeEntityCollection::NoDuplicates));
		Context.SetDeferredCommandBuffer(ParentContext.GetSharedDeferredCommandBuffer());
		Context.SetFlushDeferredCommands(false);

		NativeQuery.ForEachEntityChunk(EntityManager, Context,
			[&Result, &Callback, &Description, &Environment, &EntityManager, RowHandle](FMassExecutionContext& Context)
			{
				// No need to cache any subsystem dependencies as these are not accessible from a subquery.
				UE::Editor::DataStorage::Processors::Private::FMassSubqueryContextForwarder QueryContext(Context, Environment);
				Callback(Description, QueryContext);
				Result.Count += Context.GetNumEntities();
			}
		);
		checkf(Result.Count < 2, TEXT("Single row subquery produced multiple results."));
	}
	return Result;
}

void FTypedElementQueryProcessorData::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FExtendedQuery* StoredQuery = QueryStore->GetMutable(ParentQuery);
	checkf(StoredQuery, TEXT("A query callback was registered for execution without an associated query."));
	
	ITypedElementDataStorageInterface::FQueryDescription& Description = StoredQuery->Description;
	if (Description.Callback.ActivationCount > 0)
	{
		NativeQuery.ForEachEntityChunk(EntityManager, Context,
			[this, &Description](FMassExecutionContext& Context)
			{
				if (PrepareCachedDependenciesOnQuery(Description, Context))
				{
					UE::Editor::DataStorage::Processors::Private::FMassContextForwarder QueryContext(Description, Context, *QueryStore, *Environment);
					Description.Callback.Function(Description, QueryContext);
				}
			}
		);
	}
}



/**
 * UTypedElementQueryProcessorCallbackAdapterProcessor
 */

UTypedElementQueryProcessorCallbackAdapterProcessorBase::UTypedElementQueryProcessorCallbackAdapterProcessorBase()
	: Data(*this)
{
	bAllowMultipleInstances = true;
	bAutoRegisterWithProcessingPhases = false;
}

FMassEntityQuery& UTypedElementQueryProcessorCallbackAdapterProcessorBase::GetQuery()
{
	return Data.NativeQuery;
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueryCallback(
	FExtendedQuery& Query,
	FExtendedQueryStore::Handle QueryHandle,
	FExtendedQueryStore& QueryStore,
	FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, {});
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ShouldAllowQueryBasedPruning(
	const bool bRuntimeMode) const
{
	// TEDS is much more dynamic with when tables and processors are added and removed
	// Don't prune processors if they have queries where no table is defined, it is possible
	// the table will be dynamically created later.
	return false;
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueryCallbackData(
	FExtendedQuery& Query,
	FExtendedQueryStore::Handle QueryHandle,
	FExtendedQueryStore& QueryStore,
	FEnvironment& Environment,
	TArrayView<FMassEntityQuery> Subqueries)
{
	bool Result = Data.CommonQueryConfiguration(*this, Query, QueryHandle, QueryStore, Environment, Subqueries);

	bRequiresGameThreadExecution = Query.Description.Callback.bForceToGameThread;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor); 
	ExecutionOrder.ExecuteInGroup = Query.Description.Callback.Group;
	ExecutionOrder.ExecuteBefore = Query.Description.Callback.BeforeGroups;
	ExecutionOrder.ExecuteAfter = Query.Description.Callback.AfterGroups;
	ProcessingPhase = Data.MapToMassProcessingPhase(Query.Description.Callback.Phase);

	Super::PostInitProperties();
	return Result;
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueries()
{
	// When the extended query information is provided the native query will already be fully configured.
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::PostInitProperties()
{
	Super::Super::PostInitProperties();
}

FString UTypedElementQueryProcessorCallbackAdapterProcessorBase::GetProcessorName() const
{
	return Data.GetProcessorName();
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::DebugOutputDescription(FOutputDevice& Ar, int32 Indent) const
{
#if WITH_MASSENTITY_DEBUG
	UMassProcessor::DebugOutputDescription(Ar, Indent);
	Ar.Logf(TEXT("\n%*sType: Editor Processor"), Indent, TEXT(""));
	Data.DebugOutputDescription(Ar, Indent);
#endif // WITH_MASSENTITY_DEBUG
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Data.Execute(EntityManager, Context);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith1Subquery::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith2Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith3Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith4Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith5Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith6Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith7Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith8Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}


/**
 * UTypedElementQueryObserverCallbackAdapterProcessor
 */

UTypedElementQueryObserverCallbackAdapterProcessorBase::UTypedElementQueryObserverCallbackAdapterProcessorBase()
	: Data(*this)
{
	bAllowMultipleInstances = true;
	bAutoRegisterWithProcessingPhases = false;
}

FMassEntityQuery& UTypedElementQueryObserverCallbackAdapterProcessorBase::GetQuery()
{
	return Data.NativeQuery;
}

const UScriptStruct* UTypedElementQueryObserverCallbackAdapterProcessorBase::GetObservedType() const
{
	return ObservedType;
}

EMassObservedOperation UTypedElementQueryObserverCallbackAdapterProcessorBase::GetObservedOperation() const
{
	return Operation;
}

bool UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, {});
}

bool UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueryCallbackData(
	FExtendedQuery& Query,
	FExtendedQueryStore::Handle QueryHandle,
	FExtendedQueryStore& QueryStore,
	FEnvironment& Environment, TArrayView<FMassEntityQuery> Subqueries)
{
	bool Result = Data.CommonQueryConfiguration(*this, Query, QueryHandle, QueryStore, Environment, Subqueries);

	bRequiresGameThreadExecution = Query.Description.Callback.bForceToGameThread;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	
	ObservedType = const_cast<UScriptStruct*>(Query.Description.Callback.MonitoredType);
	
	switch (Query.Description.Callback.Type)
	{
	case ITypedElementDataStorageInterface::EQueryCallbackType::ObserveAdd:
		Operation = EMassObservedOperation::Add;
		break;
	case ITypedElementDataStorageInterface::EQueryCallbackType::ObserveRemove:
		Operation = EMassObservedOperation::Remove;
		break;
	default:
		checkf(false, TEXT("Query type %i is not supported from the observer processor adapter."),
			static_cast<int>(Query.Description.Callback.Type));
		return false;
	}

	Super::PostInitProperties();
	return Result;
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueries()
{
	// When the extended query information is provided the native query will already be fully configured.
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::PostInitProperties()
{
	Super::Super::PostInitProperties();
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::Register()
{ 
	// Do nothing as this processor will be explicitly registered.
}

FString UTypedElementQueryObserverCallbackAdapterProcessorBase::GetProcessorName() const
{
	return Data.GetProcessorName();
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::DebugOutputDescription(FOutputDevice& Ar, int32 Indent) const
{
#if WITH_MASSENTITY_DEBUG
	UMassObserverProcessor::DebugOutputDescription(Ar, Indent);
	EMassObservedOperation ObservationType = GetObservedOperation();
	if (ObservationType == EMassObservedOperation::Add)
	{
		Ar.Logf(TEXT("\n%*sType: Editor Add Observer"), Indent, TEXT(""));
	}
	else if (ObservationType == EMassObservedOperation::Remove)
	{
		Ar.Logf(TEXT("\n%*sType: Editor Remove Observer"), Indent, TEXT(""));
	}
	else
	{
		Ar.Logf(TEXT("\n%*sType: Editor <Unknown> Observer"), Indent, TEXT(""));
	}
	Data.DebugOutputDescription(Ar, Indent);
#endif // WITH_MASSENTITY_DEBUG
}


void UTypedElementQueryObserverCallbackAdapterProcessorBase::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Data.Execute(EntityManager, Context);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith1Subquery::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith2Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith3Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith4Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith5Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith6Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith7Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith8Subqueries::ConfigureQueryCallback(
	FExtendedQuery& Query, FExtendedQueryStore::Handle QueryHandle, FExtendedQueryStore& QueryStore, FEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}
