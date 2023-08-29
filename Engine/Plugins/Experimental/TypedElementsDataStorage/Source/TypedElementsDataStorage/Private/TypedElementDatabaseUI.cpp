// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseUI.h"

#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Widgets/SlateControlledConstruction.h"

DEFINE_LOG_CATEGORY(LogTypedElementDatabaseUI);

namespace Internal
{
	// Source: https://en.cppreference.com/w/cpp/utility/variant/visit
	template<class... Ts>
	struct TOverloaded : Ts...
	{ 
		using Ts::operator()...; 
	};

	template<class... Ts> TOverloaded(Ts...) -> TOverloaded<Ts...>;
}

void UTypedElementDatabaseUi::Initialize(
	ITypedElementDataStorageInterface* StorageInterface,
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibilityInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));

	Storage = StorageInterface;
	StorageCompatibility = StorageCompatibilityInterface;
	CreateStandardArchetypes();
}

void UTypedElementDatabaseUi::Deinitialize()
{
}

void UTypedElementDatabaseUi::RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description)
{
	FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose);
	if (!PurposeInfo)
	{
		FPurposeInfo& NewInfo = WidgetPurposes.Add(Purpose);
		NewInfo.Type = Type;
		NewInfo.Description = MoveTemp(Description);
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor)
{
	checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
		TEXT("Attempting to register a Typed Elements widget constructor '%s' that isn't derived from FTypedElementWidgetConstructor."),
		*Constructor->GetFullName());
	
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		switch (PurposeInfo->Type)
		{
		case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
			PurposeInfo->Factories.Emplace(Constructor);
			PurposeInfo->bIsSorted = false;
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
			if (PurposeInfo->Factories.IsEmpty())
			{
				PurposeInfo->Factories.Emplace(Constructor);
				PurposeInfo->bIsSorted = false;
			}
			else
			{
				PurposeInfo->Factories.EmplaceAt(0, Constructor);
			}
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' requires at least one column for matching."), 
				*Constructor->GetName(), *Purpose.ToString());
			return false;
		default:
			checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTypedElementDatabaseUI, Warning, 
			TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());
		return false;
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(
	FName Purpose, const UScriptStruct* Constructor, TypedElementQueryBuilder::FQueryConditions Columns)
{
	if (!Columns.IsEmpty())
	{
		checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
			TEXT("Attempting to register a Typed Elements widget constructor '%s' that isn't deriving from FTypedElementWidgetConstructor."),
			*Constructor->GetFullName());

		if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
		{
			switch (PurposeInfo->Type)
			{
			case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
				PurposeInfo->Factories.Emplace(Constructor);
				PurposeInfo->bIsSorted = false;
				return true;
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					if (PurposeInfo->Factories.IsEmpty())
					{
						PurposeInfo->Factories.Emplace(Constructor);
						PurposeInfo->bIsSorted = false;
					}
					else
					{
						PurposeInfo->Factories.EmplaceAt(0, Constructor);
					}
					return true;
				}
				else
				{
					return false;
				}
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
				if (!Columns.IsEmpty())
				{
					PurposeInfo->Factories.Emplace(Constructor, MoveTemp(Columns));
					PurposeInfo->bIsSorted = false;
					return true;
				}
				else
				{
					return false;
				}
			default:
				checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		else
		{
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());
			return false;
		}
	}
	else
	{
		return RegisterWidgetFactory(Purpose, Constructor);
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor)
{
	checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));
	
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		switch (PurposeInfo->Type)
		{
		case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
			PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
			PurposeInfo->bIsSorted = false;
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
			if (PurposeInfo->Factories.IsEmpty())
			{
				PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
				PurposeInfo->bIsSorted = false;
			}
			else
			{
				PurposeInfo->Factories.EmplaceAt(0, MoveTemp(Constructor));
			}
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' requires at least one column for matching."),
				*Constructor->GetTypeInfo()->GetName(), *Purpose.ToString());
			return false;
		default:
			checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTypedElementDatabaseUI, Warning, 
			TEXT("Unable to register widget factory as purpose '%s' isn't registered."), *Purpose.ToString());
		return false;
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, 
	TypedElementQueryBuilder::FQueryConditions Columns)
{
	if (!Columns.IsEmpty())
	{
		checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));

		if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
		{
			switch (PurposeInfo->Type)
			{
			case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
				PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
				PurposeInfo->bIsSorted = false;
				return true;
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					if (PurposeInfo->Factories.IsEmpty())
					{
						PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
						PurposeInfo->bIsSorted = false;
					}
					else
					{
						PurposeInfo->Factories.EmplaceAt(0, MoveTemp(Constructor));
					}
					return true;
				}
				else
				{
					return false;
				}
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
				if (!Columns.IsEmpty())
				{
					PurposeInfo->Factories.Emplace(MoveTemp(Constructor), MoveTemp(Columns));
					PurposeInfo->bIsSorted = false;
					return true;
				}
				else
				{
					return false;
				}
			default:
				checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		else
		{
			UE_LOG(LogTypedElementDatabaseUI, Warning, TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), 
				*Constructor->GetTypeInfo()->GetName(), *Purpose.ToString());
			return false;
		}
	}
	else
	{
		return RegisterWidgetFactory(Purpose, MoveTemp(Constructor));
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors(FName Purpose,
	TypedElementDataStorage::FMetaDataView Arguments, const WidgetConstructorCallback& Callback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		for (const FWidgetFactory& Factory : PurposeInfo->Factories)
		{
			if (!CreateSingleWidgetConstructor(Factory.Constructor, Arguments, {}, Callback))
			{
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, 
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetConstructorCallback& Callback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		// Sort so searching can be done in a single pass. This would also allow for binary searching, but the number of columns
		// is typically small enough for a binary search to end up being more expensive than a linear search. This may change
		// if/when there are a sufficient enough number of widgets that are bound to a large number of columns.
		Columns.Sort(
			[](const TWeakObjectPtr<const UScriptStruct>& Lhs, const TWeakObjectPtr<const UScriptStruct>& Rhs)
			{
				return Lhs.Get() < Rhs.Get();
			});

		if (!PurposeInfo->bIsSorted)
		{
			// This is the only call that requires the array of factories to be sorted from largest to smallest number
			// of columns, so lazily sort only when needed.
			PurposeInfo->Factories.StableSort(
				[](const FWidgetFactory& Lhs, const FWidgetFactory& Rhs)
				{
					int32 LeftSize = Lhs.Columns.MinimumColumnMatchRequired();
					int32 RightSize = Rhs.Columns.MinimumColumnMatchRequired();
					return LeftSize > RightSize;
				});
			PurposeInfo->bIsSorted = true;
		}

		switch (MatchApproach)
		{
		case EMatchApproach::LongestMatch:
			CreateWidgetConstructors_LongestMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		case EMatchApproach::ExactMatch:
			CreateWidgetConstructors_ExactMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		case EMatchApproach::SingleMatch:
			CreateWidgetConstructors_SingleMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		default:
			checkf(false, TEXT("Unsupported match type (%i) for CreateWidgetConstructors."), 
				static_cast<std::underlying_type_t<EMatchApproach>>(MatchApproach));
		}
	}
}

void UTypedElementDatabaseUi::ConstructWidgets(FName Purpose, TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		for (const FWidgetFactory& Factory : PurposeInfo->Factories)
		{
			std::visit(Internal::TOverloaded
				{
					[this, &Arguments, &ConstructionCallback](const UScriptStruct* Constructor)
					{ 
						CreateWidgetInstanceFromDescription(Constructor, Arguments, ConstructionCallback); 
					},
					[this, &Arguments, &ConstructionCallback](const TUniquePtr<FTypedElementWidgetConstructor>& Constructor)
					{
						CreateWidgetInstanceFromInstance(Constructor.Get(), Arguments, ConstructionCallback);
					}
				}, Factory.Constructor);
		}
	}
}

bool UTypedElementDatabaseUi::CreateSingleWidgetConstructor(
	const FWidgetFactory::ConstructorType& Constructor,
	TypedElementDataStorage::FMetaDataView Arguments,
	TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes, 
	const WidgetConstructorCallback& Callback)
{
	return std::visit(Internal::TOverloaded
		{
			[this, &Arguments, &MatchedColumnTypes, &Callback](const UScriptStruct* Target)
			{
				TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
					FMemory::Malloc(Target->GetStructureSize(), Target->GetMinAlignment())));
				if (Result)
				{
					Target->InitializeStruct(Result.Get());
					return Callback(MoveTemp(Result), MatchedColumnTypes);
				}
				return true;
			},
			[this, &Arguments, &MatchedColumnTypes, &Callback](const TUniquePtr<FTypedElementWidgetConstructor>& Target)
			{
				const UScriptStruct* TargetType = Target->GetTypeInfo();
				checkf(TargetType, TEXT("Expected valid type information from a widget constructor."));
				TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
					FMemory::Malloc(TargetType->GetStructureSize(), TargetType->GetMinAlignment())));
				if (Result)
				{
					TargetType->InitializeStruct(Result.Get());
					TargetType->CopyScriptStruct(Result.Get(), Target.Get());
					return Callback(MoveTemp(Result), MatchedColumnTypes);
				}
				return true;
			}
		}, Constructor);
}

void UTypedElementDatabaseUi::CreateWidgetInstanceFromDescription(
	const UScriptStruct* Target,
	TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
		FMemory_Alloca_Aligned(Target->GetStructureSize(), Target->GetMinAlignment()));
	if (Constructor)
	{
		Target->InitializeStruct(Constructor);
		CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback);
		Target->DestroyStruct(&Constructor);
	}
	else
	{
		checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from a description."));
	}
}

void UTypedElementDatabaseUi::CreateWidgetInstanceFromInstance(
	FTypedElementWidgetConstructor* SourceConstructor,
	TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	if (SourceConstructor->CanBeReused())
	{
		CreateWidgetInstance(*SourceConstructor, Arguments, ConstructionCallback);
	}
	else
	{
		const UScriptStruct* Target = SourceConstructor->GetTypeInfo();
		checkf(Target, TEXT("Expected valid type information from a widget constructor."));
		FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
			FMemory_Alloca_Aligned(Target->GetStructureSize(), Target->GetMinAlignment()));
		if (Constructor)
		{
			Target->InitializeStruct(Constructor);
			Target->CopyScriptStruct(Constructor, SourceConstructor);
			CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback);
			Target->DestroyStruct(&Constructor);
		}
		else
		{
			checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from an instance."));
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetInstance(
	FTypedElementWidgetConstructor& Constructor, 
	TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	TypedElementRowHandle Row = Storage->AddRow(WidgetTable);
	Storage->AddColumns(Row, Constructor.GetAdditionalColumnsList());
	TSharedPtr<SWidget> Widget = Constructor.Construct(Row, Storage, this, Arguments);
	if (Widget)
	{
		ConstructionCallback(Widget.ToSharedRef(), Row);
	}
	else
	{
		Storage->RemoveRow(Row);
	}
}

TSharedPtr<SWidget> UTypedElementDatabaseUi::ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
	TypedElementDataStorage::FMetaDataView Arguments)
{
	if (Constructor.CanBeReused())
	{
		return Constructor.Construct(Row, Storage, this, Arguments);
	}
	else
	{
		const UScriptStruct* Target = Constructor.GetTypeInfo();
		FTypedElementWidgetConstructor* ConstructorCopy = reinterpret_cast<FTypedElementWidgetConstructor*>(
			FMemory_Alloca_Aligned(Target->GetStructureSize(), Target->GetMinAlignment()));
		if (ConstructorCopy)
		{
			Target->InitializeStruct(ConstructorCopy);
			Target->CopyScriptStruct(ConstructorCopy, &Constructor);
			TSharedPtr<SWidget> Widget = ConstructorCopy->Construct(Row, Storage, this, Arguments);
			Target->DestroyStruct(&ConstructorCopy);
			return Widget;
		}
		else
		{
			checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from an instance."));
			return nullptr;
		}
		return nullptr;
	}
}

void UTypedElementDatabaseUi::ListWidgetPurposes(const WidgetPurposeCallback& Callback) const
{
	for (auto&& It : WidgetPurposes)
	{
		Callback(It.Key, It.Value.Type, It.Value.Description);
	}
}

void UTypedElementDatabaseUi::CreateStandardArchetypes()
{
	WidgetTable = Storage->RegisterTable(MakeArrayView(
		{
			FTypedElementSlateWidgetReferenceColumn::StaticStruct(),
			FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct()
		}), FName(TEXT("Editor_WidgetTable")));
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_LongestMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetConstructorCallback& Callback)
{
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (auto FactoryIt = WidgetFactories.CreateConstIterator(); FactoryIt && !Columns.IsEmpty(); ++FactoryIt)
	{
		int32 MatchingIndex = INDEX_NONE;
		if (FactoryIt->Columns.MinimumColumnMatchRequired() > Columns.Num())
		{
			// There are more columns required for this factory than there are in the requested columns list so skip this
			// factory.
			continue;
		}

		MatchedColumns.Reset();

		if (FactoryIt->Columns.Verify(MatchedColumns, Columns, true))
		{
			// Remove the found columns from the requested list.
			Algo::SortBy(MatchedColumns, [](const TWeakObjectPtr<const UScriptStruct>& Column) { return Column.Get(); });
			TConstArrayView<TWeakObjectPtr<const UScriptStruct>> CleanMatchedColumns(MatchedColumns.GetData(), Algo::Unique(MatchedColumns));

			TWeakObjectPtr<const UScriptStruct>* ColumnsIt = Columns.GetData();
			TWeakObjectPtr<const UScriptStruct>* ColumnsEnd = ColumnsIt + Columns.Num();
			int32 ColumnIndex = 0;
			for (const TWeakObjectPtr<const UScriptStruct>& MatchedColumn : CleanMatchedColumns)
			{
				while (*ColumnsIt != MatchedColumn)
				{
					++ColumnIndex;
					++ColumnsIt;
					if (ColumnsIt == ColumnsEnd)
					{
						ensureMsgf(false, TEXT("A previous found matching column can't be found in the original array."));
						return;
					}
				}
				Columns.RemoveAt(ColumnIndex);
			}
			
			if (!CreateSingleWidgetConstructor(FactoryIt->Constructor, Arguments, CleanMatchedColumns, Callback))
			{
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_ExactMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetConstructorCallback& Callback)
{
	int32 ColumnCount = Columns.Num();
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (const FWidgetFactory& Factory : WidgetFactories)
	{
		// If there are more matches required that there are columns, then there will never be an exact match.
		// Less than the column count can still result in a match that covers all columns.
		if (Factory.Columns.MinimumColumnMatchRequired() > ColumnCount)
		{
			continue;
		}

		MatchedColumns.Reset();

		if (Factory.Columns.Verify(MatchedColumns, Columns, true))
		{
			Algo::SortBy(MatchedColumns, [](const TWeakObjectPtr<const UScriptStruct>& Column) { return Column.Get(); });
			TWeakObjectPtr<const UScriptStruct>* MatchedColumnsIt = MatchedColumns.GetData();
			TWeakObjectPtr<const UScriptStruct>* MatchedColumnsEnd = MatchedColumnsIt + MatchedColumns.Num();
			bool bFullyMatched = true;
			for (TWeakObjectPtr<const UScriptStruct> Column : Columns)
			{
				while (*MatchedColumnsIt != Column)
				{
					if (++MatchedColumnsIt == MatchedColumnsEnd)
					{
						bFullyMatched = false;
						break;
					}
				}
				if (!bFullyMatched)
				{
					break;
				}
			}

			if (bFullyMatched)
			{
				Columns.Reset();
				CreateSingleWidgetConstructor(Factory.Constructor, Arguments, MatchedColumns, Callback);
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_SingleMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, TypedElementDataStorage::FMetaDataView Arguments,
	const WidgetConstructorCallback& Callback)
{
	auto FactoryIt = WidgetFactories.rbegin();
	auto FactoryEnd = WidgetFactories.rend();

	// Start from the back as the widgets with lower counts will be last.
	int32 ColumnIndex = Columns.Num() - 1;

	auto ColumnEnd = Columns.rend();
	for (auto ColumnIt = Columns.rbegin(); ColumnIt != ColumnEnd; ++ColumnIt, --ColumnIndex)
	{
		const UScriptStruct* ColumnType = (*ColumnIt).Get();
		for (; FactoryIt != FactoryEnd; ++FactoryIt)
		{
			TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnData = (*FactoryIt).Columns.GetColumns();
			if (ColumnData.Num() > 1)
			{
				// Moved passed the point where factories only have a single column.
				return;
			}
			else if (ColumnData.Num() == 0)
			{
				// Need to move further to find factories with exactly one column.
				continue;
			}

			if (ColumnData[0] == *ColumnIt)
			{
				Columns.RemoveAt(ColumnIndex);
				CreateSingleWidgetConstructor((*FactoryIt).Constructor, Arguments, ColumnData, Callback);
				// Match was found so move on to the next column in the column.
				break;
			}
		}
	}
}



//
// FWidgetFactory
//

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(const UScriptStruct* InConstructor)
	: Constructor(InConstructor)
{
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor)
	: Constructor(MoveTemp(InConstructor))
{
	checkf(std::get<TUniquePtr<FTypedElementWidgetConstructor>>(Constructor)->GetTypeInfo(), 
		TEXT("Widget constructor registered that didn't contain valid type information."));
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(const UScriptStruct* InConstructor, 
	TypedElementQueryBuilder::FQueryConditions&& InColumns)
	: Columns(MoveTemp(InColumns))
	, Constructor(InConstructor)
{
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor, 
	TypedElementQueryBuilder::FQueryConditions&& InColumns)
	: Columns(MoveTemp(InColumns))
	, Constructor(MoveTemp(InConstructor))
{
	checkf(std::get<TUniquePtr<FTypedElementWidgetConstructor>>(Constructor)->GetTypeInfo(),
		TEXT("Widget constructor registered that didn't contain valid type information."));
}
