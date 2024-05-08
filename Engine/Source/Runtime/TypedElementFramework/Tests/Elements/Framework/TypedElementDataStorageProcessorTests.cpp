// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "TypedElementTestColumns.h"

namespace TEDS::Debug::ProcessorTests
{
	void OnProcessorTestsEnabled(IConsoleVariable*);

	bool GProcessorTestsEnabled = false;
	FAutoConsoleVariableRef CVarAllowUnversionedContentInEditor(
		TEXT("TEDS.Tests.ProcessorTestsEnabled"),
		GProcessorTestsEnabled,
		TEXT("If true, registers processors and additional commands with TEDS to test processors."),
		FConsoleVariableDelegate::CreateStatic(&OnProcessorTestsEnabled),
		ECVF_Default
	);

	TypedElementDataStorage::TableHandle PrimaryTable = TypedElementDataStorage::InvalidTableHandle;
	TypedElementDataStorage::TableHandle SecondaryTable = TypedElementDataStorage::InvalidTableHandle;

	TArray<TypedElementQueryHandle> RegisteredQueries;
	TArray<IConsoleCommand*> RegisteredCommands;

	void RegisterProcessors()
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
		using namespace TypedElementQueryBuilder;
		using DSI = ITypedElementDataStorageInterface;
		namespace DS = TypedElementDataStorage;

		if (PrimaryTable == TypedElementDataStorage::InvalidTableHandle)
		{
			PrimaryTable =
				DataStorage->RegisterTable<FTEDSProcessorTestsReferenceColumn, FTEDSProcessorTests_PrimaryTag>(FName(TEXT("ProcessorTests Primary Table")));
		}
		if (SecondaryTable == TypedElementDataStorage::InvalidTableHandle)
		{
			SecondaryTable =
				DataStorage->RegisterTable<FTEDSProcessorTestsReferenceColumn, FTEDSProcessorTests_SecondaryTag>(FName(TEXT("ProcessorTests Secondary Table")));
		}

		// Test creation of a row from within a query processor
		TypedElementQueryHandle PrimaryRowQuery = DataStorage->RegisterQuery(
			Select(TEXT("TEST: Creating a row for primary reference column"),
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage->GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
			[](DS::IQueryContext& Context, const TypedElementRowHandle* Rows, FTEDSProcessorTestsReferenceColumn* ReferenceColumns)
			{
				const int32 RowCount = Context.GetRowCount();
				TConstArrayView<TypedElementRowHandle> RowsView = MakeArrayView(Rows, RowCount);
				TArrayView<FTEDSProcessorTestsReferenceColumn> ReferenceColumnsView = MakeArrayView(ReferenceColumns, RowCount);

				for (int32 Index = 0; Index < RowCount; ++Index)
				{
					FTEDSProcessorTestsReferenceColumn& PrimaryReferenceColumn = ReferenceColumnsView[Index];
					// Auto-create a secondary row if this points to no row
					if (!Context.IsRowAvailable(PrimaryReferenceColumn.Reference))
					{
						TypedElementDataStorage::RowHandle SecondaryRow = Context.AddRow(SecondaryTable);
						// Initialize bi-directional row references
						PrimaryReferenceColumn.Reference = SecondaryRow;
						Context.AddColumn(SecondaryRow, FTEDSProcessorTestsReferenceColumn{.Reference = RowsView[Index]});
					}
				}
			}
			)
			.Where()
				.All<FTEDSProcessorTests_PrimaryTag>()
				.None<FTEDSProcessorTests_Linked>()
			.Compile());

		RegisteredQueries.Emplace(PrimaryRowQuery);

		TypedElementQueryHandle UpdateTransformWidget = DataStorage->RegisterQuery(
			Select()
				.ReadOnly<FTEDSProcessorTestsReferenceColumn>()
			.Where()
				.All<FTEDSProcessorTests_PrimaryTag>()
				.None<FTEDSProcessorTests_Linked>()
			.Compile());
		
		RegisteredQueries.Emplace(UpdateTransformWidget);

		TypedElementQueryHandle SecondaryRowQuery = DataStorage->RegisterQuery(
			Select(TEXT("TEST: Creating a row for secondary reference column"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage->GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
			[](DS::IQueryContext& Context, const TypedElementRowHandle* Rows, FTEDSProcessorTestsReferenceColumn* ReferenceColumns)
			{
				const int32 RowCount = Context.GetRowCount();
				TConstArrayView<TypedElementRowHandle> RowsView = MakeArrayView(Rows, RowCount);
				TArrayView<FTEDSProcessorTestsReferenceColumn> ReferenceColumnsView = MakeArrayView(ReferenceColumns, RowCount);

				for (int32 Index = 0; Index < RowCount; ++Index)
				{
					const TypedElementRowHandle& SecondaryRow = RowsView[Index];
					Context.RunSubquery(
						0,
						ReferenceColumnsView[Index].Reference,
						CreateSubqueryCallbackBinding(
						[SecondaryRow](TypedElementDataStorage::ISubqueryContext& SubqueryContext, TypedElementDataStorage::RowHandle PrimaryRow, const FTEDSProcessorTestsReferenceColumn& ReferenceColumn)
						{
							if (ReferenceColumn.Reference == SecondaryRow)
							{
								// Add these tags to prevent further processing
								SubqueryContext.AddColumns<FTEDSProcessorTests_Linked>(SecondaryRow);
								SubqueryContext.AddColumns<FTEDSProcessorTests_Linked>(PrimaryRow);
							}
						}
					));
				}
			}
			)
			.Where()
				.All<FTEDSProcessorTests_SecondaryTag>()
				.None<FTEDSProcessorTests_Linked>()
			.DependsOn()
				.SubQuery(UpdateTransformWidget)
			.Compile());

		RegisteredQueries.Emplace(SecondaryRowQuery);
	}

	void UnregisterProcessors()
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
		for (const TypedElementQueryHandle& Handle : RegisteredQueries)
		{
			DataStorage->UnregisterQuery(Handle);
		}
		RegisteredQueries.Empty();
	}

	void RegisterCommands()
	{
		IConsoleCommand* Command = IConsoleManager::Get().RegisterConsoleCommand(TEXT("TEDS.Tests.ProcessorTests.AddPrimaryRows"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			int32 RowsToCreate = 0;
			if (Args.Num() != 1)
			{
				return;
			}
			TTypeFromString<int32>::FromString(RowsToCreate, *Args[0]);
			if (RowsToCreate <= 0)
			{
				return;
			}

			ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
			DataStorage->BatchAddRow(PrimaryTable, RowsToCreate, [](TypedElementRowHandle Row)
			{
			});
		}), ECVF_Default);
		
		RegisteredCommands.Emplace(Command);
	}

	void UnregisterCommands()
	{
		for (IConsoleCommand* Command : RegisteredCommands)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Command);
		}
		RegisteredCommands.Empty();
	}
	
	void OnProcessorTestsEnabled(IConsoleVariable* Variable)
	{
		if (Variable->GetBool())
		{
			RegisterProcessors();
			RegisterCommands();
		}
		else
		{
			UnregisterProcessors();
			UnregisterCommands();
		}
	};
}
#endif

