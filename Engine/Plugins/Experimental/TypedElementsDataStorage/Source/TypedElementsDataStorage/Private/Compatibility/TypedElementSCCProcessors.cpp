// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementSCCProcessors.h"

#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

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

void UTypedElementSCCFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage) const
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
			FSCCRevisionIdColumn, FSCCExternalRevisionIdColumn>(),
		FName("Editor_RevisionControlTable"));
}

void UTypedElementSCCFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
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
}
