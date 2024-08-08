// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseDebugTypes.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Common/TypedElementDataStorageLog.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementTestColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Private
{
	static const UStruct* GetTypeInfo(const TWeakObjectPtr<const UClass>& TypeInfo) { return TypeInfo.Get(); }
	static const UStruct* GetTypeInfo(const TWeakObjectPtr<const UScriptStruct>& TypeInfo) { return TypeInfo.Get(); }
	static const UStruct* GetTypeInfo(const TObjectPtr<const UClass>& TypeInfo) { return TypeInfo; }
	static const UStruct* GetTypeInfo(const TObjectPtr<const UScriptStruct>& TypeInfo) { return TypeInfo; }

	template<typename TypeInfoType>
	void PrintObjectTypeInformation(ITypedElementDataStorageInterface* DataStorage, FString Message, FOutputDevice& Output)
	{
		using namespace TypedElementDataStorage;
		using namespace TypedElementQueryBuilder;

		static QueryHandle Query = [DataStorage]
		{
			return DataStorage->RegisterQuery(
				Select()
					.ReadOnly<TypeInfoType>()
				.Compile());
		}();

		if (Query != InvalidQueryHandle)
		{
			DataStorage->RunQuery(Query, CreateDirectQueryCallbackBinding(
				[&Output, &Message](IDirectQueryContext& Context, const TypeInfoType* Types)
				{
					Message.Reset();
					Message += TEXT("  Batch start\n");

					TConstArrayView<TypeInfoType> TypeList(Types, Context.GetRowCount());
					for (const TypeInfoType& Type : TypeList)
					{
						if (const UStruct* TypeInfo = GetTypeInfo(Type.TypeInfo))
						{
							Message += TEXT("    Type: ");
							TypeInfo->AppendName(Message);
							Message += TEXT('\n');
						}
						else
						{
							Message += TEXT("    Type: [Invalid]\n");
						}
					}
					Message += TEXT("  Batch end\n");
					Output.Log(Message);
				}));
		}
	}

	template<typename... Conditions>
	void PrintObjectLabels(FOutputDevice& Output)
	{
		using namespace TypedElementQueryBuilder;
		using DSI = ITypedElementDataStorageInterface;

		if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
		{
			static TypedElementQueryHandle LabelQuery = [DataStorage]
			{
				if constexpr (sizeof...(Conditions) > 0)
				{
					return DataStorage->RegisterQuery(
						Select()
							.ReadOnly<FTypedElementUObjectColumn, FTypedElementLabelColumn>()
						.Where()
							.All(Conditions::StaticStruct()...)
						.Compile());
				}
				else
				{
					return DataStorage->RegisterQuery(
						Select()
							.ReadOnly<FTypedElementUObjectColumn, FTypedElementLabelColumn>()
						.Compile());
				}
			}();

			if (LabelQuery != TypedElementInvalidQueryHandle)
			{
				FString Message;
				DataStorage->RunQuery(LabelQuery, CreateDirectQueryCallbackBinding(
					[&Output, &Message](DSI::IDirectQueryContext& Context, const FTypedElementUObjectColumn* Objects, const FTypedElementLabelColumn* Labels)
					{
						const uint32 Count = Context.GetRowCount();

						const FTypedElementLabelColumn* LabelsIt = Labels;
						int32 CharacterCount = 2; // Initial blank space and new line.
						// Reserve memory first to avoid repeated memory allocations.
						for (uint32 Index = 0; Index < Count; ++Index)
						{
							CharacterCount
								+= 4 /* Indention */
								+ 16 /* Hex address of actor */
								+ 2 /* Colon and space */
								+ LabelsIt->Label.Len()
								+ 1 /* Trailing new line */;
							++LabelsIt;
						}
						Message.Reset(CharacterCount);
						Message = TEXT(" \n");

						LabelsIt = Labels;
						for (uint32 Index = 0; Index < Count; ++Index)
						{
							Message.Appendf(TEXT("    0x%p: %s\n"), Objects->Object.Get(), *LabelsIt->Label);

							++LabelsIt;
							++Objects;
						}

						Output.Log(Message);
					}));
			}
		}
	}
}

FAutoConsoleCommandWithOutputDevice PrintObjectTypeInformationConsoleCommand(
	TEXT("TEDS.Debug.PrintObjectTypeInfo"),
	TEXT("Prints the type information of any rows that has a type information column."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.PrintObjectTypeInfo);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				FString Message;
				Output.Log(TEXT("The Typed Elements Data Storage has the types:"));
				Private::PrintObjectTypeInformation<FTypedElementClassTypeInfoColumn>(DataStorage, Message, Output);
				Private::PrintObjectTypeInformation<FTypedElementScriptStructTypeInfoColumn>(DataStorage, Message, Output);
				Output.Log(TEXT("End of Typed Elements Data Storage type list."));
			}
		}
	));

FAutoConsoleCommandWithOutputDevice PrintAllUObjectsLabelsConsoleCommand(
	TEXT("TEDS.Debug.PrintAllUObjectsLabels"),
	TEXT("Prints out the labels for all UObjects found in the Typed Elements Data Storage."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.PrintAllUObjectsLabels);
			Output.Log(TEXT("The Typed Elements Data Storage has the following UObjects:"));
			Private::PrintObjectLabels(Output);
			Output.Log(TEXT("End of Typed Elements Data Storage UObjects list."));
		}));

FAutoConsoleCommandWithOutputDevice PrintActorLabelsConsoleCommand(
	TEXT("TEDS.Debug.PrintActorLabels"),
	TEXT("Prints out the labels for all actors found in the Typed Elements Data Storage."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.PrintActorLabels);
			Output.Log(TEXT("The Typed Elements Data Storage has the following actors:"));
			Private::PrintObjectLabels<FTypedElementActorTag>(Output);
			Output.Log(TEXT("End of Typed Elements Data Storage actors list."));
		}));

FAutoConsoleCommandWithOutputDevice ListExtensionsConsoleCommand(
	TEXT("TEDS.Debug.ListExtensions"),
	TEXT("Prints a list for all available extension names."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.ListExtensions);

			FString Message;
			auto RecordExtensions = [&Message](FName Extension)
			{
				Message += TEXT("    ");
				Extension.AppendString(Message);
				Message += TEXT('\n');
			};

			UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

			if (const ITypedElementDataStorageInterface* DataStorage = Registry->GetDataStorage())
			{
				Message = TEXT("Data Storage Extensions: \n");
				DataStorage->ListExtensions(RecordExtensions);
			}
			if (const ITypedElementDataStorageCompatibilityInterface* DataStorageCompat = Registry->GetDataStorageCompatibility())
			{
				Message += TEXT("Data Storage Compatibility Extensions: \n");
				DataStorageCompat->ListExtensions(RecordExtensions);
			}
			if (const ITypedElementDataStorageUiInterface* DataStorageUi = Registry->GetDataStorageUi())
			{
				Message += TEXT("Data Storage UI Extensions: \n");
				DataStorageUi->ListExtensions(RecordExtensions);
			}

			Output.Log(Message);
		}
	));



static FAutoConsoleCommand CVarCreateRow(
	TEXT("TEDS.Debug.CreateRow"),
	TEXT("Argument: \n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
		static TypedElementDataStorage::TableHandle Table = DataStorage->RegisterTable<FTestColumnA>(FName(TEXT("Debug.CreateRow Table")));

		const TypedElementDataStorage::RowHandle RowHandle = DataStorage->AddRow(Table);

		UE_LOG(LogEditorDataStorage, Warning, TEXT("Added Row %llu"), static_cast<uint64>(RowHandle));
	}));

static FAutoConsoleCommand CVarAddDynamicTag(
	TEXT("TEDS.Debug.DynamicTag.AddColumn"),
	TEXT("Argument: Row, Tag, Value\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		using namespace UE::Editor::DataStorage;
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() != 3)
		{
			return;
		}
		
		const uint64 RowAsU64 = FCString::Strtoui64(*Args[0], nullptr, 10);
		const TypedElementDataStorage::RowHandle Row = RowAsU64;

		
		const FName Value(*Args[2]);

		constexpr bool bUseTemplateSugar = true;
		if constexpr (bUseTemplateSugar)
		{
			const FName Tag = *Args[1];
			DataStorage->AddColumn<FDynamicTag>(Row, Tag, Value); 
		}
		else
		{
			const FDynamicTag Tag(*Args[1]);
			DataStorage->AddColumn(Row, Tag, Value);
		}
		
		
	}),
	ECVF_Default);

static FAutoConsoleCommand CVarRemoveDynamicTag(
	TEXT("TEDS.Debug.DynamicTag.RemoveColumn"),
	TEXT("Argument: Row, Group\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() != 2)
		{
			return;
		}
		
		const uint64 RowAsU64 = FCString::Strtoui64(*Args[0], nullptr, 10);
		const TypedElementDataStorage::RowHandle Row = RowAsU64;
		
		constexpr bool bUseTemplateSugar = true;
		if constexpr (bUseTemplateSugar)
		{
			using namespace UE::Editor::DataStorage;
			const FName Tag = *Args[1];
			DataStorage->RemoveColumn<FDynamicTag>(Row, Tag);
		}
		else
		{
			const UE::Editor::DataStorage::FDynamicTag Tag(*Args[1]);
			DataStorage->RemoveColumn(Row, Tag);
		}		
	}),
	ECVF_Default);
	
static FAutoConsoleCommand CVarMatchDynamicTag(
	TEXT("TEDS.Debug.DynamicTag.RunQuery"),
	TEXT("Argument: Tag, [optional] Value\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		using namespace TypedElementQueryBuilder;
		using DSI = ITypedElementDataStorageInterface;
		using namespace UE::Editor::DataStorage;
		
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() < 1 || Args.Num() > 2)
		{
			return;
		}			

		const TypedElementQueryHandle Query = [&Args, DataStorage]() -> TypedElementQueryHandle
		{
			const FName Tag(*Args[0]);
			if (Args.Num() == 1)
			{
				// Matches all rows with the 
				return DataStorage->RegisterQuery(
					Select().
						Where().
							// Match all rows with a dynamic tag of type Tag (ie. all rows with a dynamic tag of "Color")
							All<FDynamicTag>(Tag).
							All<FTestColumnA>().
						Compile());
			}
			else
			{
				const FName MatchValue(*Args[1]);
				return DataStorage->RegisterQuery(
					Select().
					Where().
						// Match all rows with a dynamic tag of type Tag that has a MatchValue (ie. all rows with dynamic tag "Color" with value "Red")
						All<FDynamicTag>(Tag, MatchValue).
						All<FTestColumnA>().
					Compile());
			}
		}();

		uint64 Count = 0;
		
		const TypedElementDataStorage::FQueryResult Result = DataStorage->RunQuery(Query, CreateDirectQueryCallbackBinding(
			[&Count](const DSI::IDirectQueryContext& Context, const TypedElementRowHandle*)
			{
				Count += Context.GetRowCount();
			}));
		DataStorage->UnregisterQuery(Query);

		UE_LOG(LogEditorDataStorage, Warning, TEXT("Processed %llu rows"), static_cast<uint64>(Count));
	}),
	ECVF_Default);

static FAutoConsoleCommand CVarAddDynamicTagFromEnum(
	TEXT("TEDS.Debug.DynamicTag.AddWithEnum"),
	TEXT("Argument: Row, EnumValue\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() < 1 || Args.Num() > 2)
		{
			return;
		}
		
		const uint64 RowAsU64 = FCString::Strtoui64(*Args[0], nullptr, 10);
		const TypedElementDataStorage::RowHandle Row = RowAsU64;

		if (Args.Num() == 1)
		{
			// Use of a enum value directly as a template parameter.  Only useful if enum value known at compile time
			DataStorage->AddColumn<ETedsDebugEnum::Red>(Row);
		}
		else
		{
			// Use an enum value from a runtime source
			// In this case, the argument is parsed and converted to an enum type
			UEnum* Enum = StaticEnum<ETedsDebugEnum>();
			int64 EnumValueAsI64 = Enum->GetValueByNameString(*Args[1]);
			if (EnumValueAsI64 == INDEX_NONE)
			{
				return;
			}
			const ETedsDebugEnum EnumValue = static_cast<ETedsDebugEnum>(EnumValueAsI64);
			
			DataStorage->AddColumn(Row, EnumValue);
		}
	}),
	ECVF_Default);

static FAutoConsoleCommand CVarRemoveDynamicTagFromEnum(
	TEXT("TEDS.Debug.DynamicTag.RemoveWithEnum"),
	TEXT("Argument: Row\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() != 1)
		{
			return;
		}
		
		const uint64 RowAsU64 = FCString::Strtoui64(*Args[0], nullptr, 10);
		const TypedElementDataStorage::RowHandle Row = RowAsU64;
		
		DataStorage->RemoveColumn<ETedsDebugEnum>(Row);
	}),
	ECVF_Default);


static FAutoConsoleCommand CVarMatchDynamicTagFromEnum(
	TEXT("TEDS.Debug.DynamicTag.RunQueryEnum"),
	TEXT("Argument: [optional] EnumValue\n"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		using namespace TypedElementQueryBuilder;
		using DSI = ITypedElementDataStorageInterface;
		using namespace UE::Editor::DataStorage;
		
		ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

		if (Args.Num() > 1)
		{
			UE_LOG(LogEditorDataStorage, Error, TEXT("Invalid number of arguments"));
			return;
		}

		if (Args.Num() == 1)
		{
			// Make sure that the given enum value is actually a value
			UEnum* Enum = StaticEnum<ETedsDebugEnum>();
			int64 EnumValue = Enum->GetValueByNameString(*Args[0]);
			if (EnumValue == INDEX_NONE)
			{
				return;
			}
		}

		const TypedElementQueryHandle Query = [&Args, DataStorage]() -> TypedElementQueryHandle
		{
			if (Args.Num() == 0)
			{
				// Matches all rows with the 
				return DataStorage->RegisterQuery(
					Select().
						Where().
							// Match all rows with an enum dynamic tag of the hardcoded enum type
							All<ETedsDebugEnum>().
						Compile());
			}
			else if (Args.Num() == 1)
			{
				UEnum* Enum = StaticEnum<ETedsDebugEnum>();
				int64 EnumValue = Enum->GetValueByNameString(*Args[0]);
				return DataStorage->RegisterQuery(
					Select().
					Where().
						// Match all rows with a dynamic tag of the hardcoded enum type that has the given value
						// Note, usually this would be written something like:
						//   All(ETedsDebugEnum::Red).
						// However it isn't possible to do that when getting the enum value from a string.  API is still exercised
						// using the static_cast
						All(static_cast<ETedsDebugEnum>(EnumValue)).
					Compile());
			}
			else
			{
				return TypedElementDataStorage::InvalidQueryHandle;
			}
		}();
		if (Query == TypedElementDataStorage::InvalidQueryHandle)
		{
			UE_LOG(LogEditorDataStorage, Error, TEXT("Invalid number of arguments"));
			return;
		}
		
		uint64 Count = 0;
		
		const TypedElementDataStorage::FQueryResult Result = DataStorage->RunQuery(Query, CreateDirectQueryCallbackBinding(
			[&Count](const DSI::IDirectQueryContext& Context, const TypedElementRowHandle*)
			{
				Count += Context.GetRowCount();
			}));
		DataStorage->UnregisterQuery(Query);

		UE_LOG(LogEditorDataStorage, Warning, TEXT("Processed %llu rows"), static_cast<uint64>(Count));
		
	}),
	ECVF_Default);