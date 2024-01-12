// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevisionControlProcessors.h"

#include "ISourceControlModule.h"
#include "SourceControlFileStatusMonitor.h"
#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

extern TYPEDELEMENTSDATASTORAGE_API FAutoConsoleVariableRef CVarAutoPopulateState;

FAutoConsoleCommandWithArgsAndOutputDevice SetSelectionSCCStateConsoleCommand(
	TEXT("TEDS.Debug.SetSCCState"),
	TEXT("Adds a source control state to selected objects."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Output)
		{
			using namespace TypedElementQueryBuilder;
			using DSI = ITypedElementDataStorageInterface;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.SetSCCState);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle AllSelectedQuery = TypedElementInvalidQueryHandle;
				if (AllSelectedQuery == TypedElementInvalidQueryHandle)
				{
					AllSelectedQuery = DataStorage->RegisterQuery(
						Select()
						.Where()
							.All<FTypedElementSelectionColumn>()
						.Compile());
				}
				
				if (AllSelectedQuery == TypedElementInvalidQueryHandle)
				{
					return;
				}

				ESCCModification Modification = ESCCModification::Modified;

				if (Args.Num() > 0)
				{
					// Parse the index
					int32 StateIndex;
					LexFromString(StateIndex, *Args[0]);

					if (!(StateIndex >= 0 && StateIndex <= int32(ESCCModification::Conflicted)))
					{
						Output.Log(TEXT("State index out of range"));
						return;
					}

					Modification = static_cast<ESCCModification>(StateIndex);
				}
				
				TArray<TypedElementRowHandle> RowHandles;
				
				DataStorage->RunQuery(AllSelectedQuery, [&RowHandles](const DSI::FQueryDescription&, DSI::IDirectQueryContext& Context)
				{
					RowHandles = Context.GetRowHandles();
				});

				for (TypedElementRowHandle Row : RowHandles)
				{
					DataStorage->AddOrGetColumn<FSCCStatusColumn>(Row)->Modification = Modification;
					DataStorage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
				}
			}
		}
	));

void UTypedElementRevisionControlFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) const
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
			FSCCRevisionIdColumn, FSCCExternalRevisionIdColumn>(),
		FName("Editor_RevisionControlTable"));
}

void UTypedElementRevisionControlFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Change selection outline colors based on SCC status"),
			// This is in PrePhysics because the outline->actor query is in DuringPhysics and contexts don't flush changes between tick groups
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage)),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FSCCStatusColumn& SCCStatus)
			{
				constexpr int BasicSelectionColorCount = 2;
				Context.AddColumn<FTypedElementViewportColorColumn>(Row, { .SelectionOutlineColorIndex = static_cast<uint8>(int(SCCStatus.Modification) + BasicSelectionColorCount) });
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag>()
		.Compile()
	);
	
	CVarAutoPopulateState->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* AutoPopulate)
		{
			if (AutoPopulate->GetBool())
			{
				RegisterFetchUpdates(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(FetchUpdates);
			}
		}
	); 
	
	if (CVarAutoPopulateState->GetBool())
	{
		RegisterFetchUpdates(DataStorage);
	}
}

void UTypedElementRevisionControlFactory::RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

	FetchUpdates = DataStorage.RegisterQuery(
		Select(
			TEXT("Gather source control statuses for objects with unresolved package paths"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[this, &FileStatusMonitor](DSI::IQueryContext& Context, const FTypedElementPackageUnresolvedReference* InUnresolvedReferences)
			{
				TConstArrayView<TypedElementDataStorage::RowHandle> RowHandles = Context.GetRowHandles();
				TConstArrayView<FTypedElementPackageUnresolvedReference, int64> UnresolvedReferences { InUnresolvedReferences, Context.GetRowCount() };

				for (int64 UnresolvedReferenceIndex = 0; UnresolvedReferenceIndex < UnresolvedReferences.Num(); ++UnresolvedReferenceIndex)
				{
					const FTypedElementPackageUnresolvedReference& UnresolvedReference = UnresolvedReferences[UnresolvedReferenceIndex];
					if (UnresolvedReference.Index == 0)
					{
						Context.RemoveColumns<FTypedElementPackageUnresolvedReference>(RowHandles[UnresolvedReferenceIndex]);
						return;
					}
					static FSourceControlFileStatusMonitor::FOnSourceControlFileStatus EmptyDelegate{};
					
					FileStatusMonitor.StartMonitoringFile(
						reinterpret_cast<uintptr_t>(this),
						UnresolvedReference.PathOnDisk,
						EmptyDelegate
					);
				}
			}
		)
		.Compile()
	);
}
