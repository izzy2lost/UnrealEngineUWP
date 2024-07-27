// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
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

