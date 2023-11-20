// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementQueryBuilder.h"

#include "Algo/BinarySearch.h"
#include "Elements/Framework/TypedElementMetaData.h"
#include "GenericPlatform/GenericPlatformMath.h"

namespace TypedElementQueryBuilder
{
	const UScriptStruct* Type(FTopLevelAssetPath Name)
	{
		const UScriptStruct* StructInfo = TypeOptional(Name);
		checkf(StructInfo, TEXT("Type name '%s' used as part of building a typed element query was not found."), *Name.ToString());
		return StructInfo;
	}

	const UScriptStruct* TypeOptional(FTopLevelAssetPath Name)
	{
		constexpr bool bExactMatch = true;
		return static_cast<UScriptStruct*>(StaticFindObject(UScriptStruct::StaticClass(), Name, bExactMatch));
	}

	const UScriptStruct* operator""_Type(const char* Name, std::size_t NameSize)
	{
		return Type(FTopLevelAssetPath{ FAnsiStringView{ Name, IntCastChecked<int32>(NameSize) } });
	}

	const UScriptStruct* operator""_TypeOptional(const char* Name, std::size_t NameSize)
	{
		return TypeOptional(FTopLevelAssetPath{ FAnsiStringView{ Name, IntCastChecked<int32>(NameSize) } });
	}



	//
	// FQueryConditions
	//

	FQueryConditions::FQueryConditions(FColumnBase Column)
		: ColumnCount(1)
	{
		Columns[0] = Column.TypeInfo;
	}

	void FQueryConditions::AppendToString(FString& Output) const
	{
		if (TokenCount > 0)
		{
			Output += "{ ";
			uint8_t ColumnIndex = 0;
			if (Tokens[0] != Token::ScopeOpen)
			{
				ColumnIndex = 1;
				AppendName(Output, Columns[0]);
			}
			for (uint8_t TokenIndex = 0; TokenIndex < TokenCount; ++TokenIndex)
			{
				switch (Tokens[TokenIndex])
				{
				case Token::And:
					Output += " && ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::Or:
					Output += " || ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::ScopeOpen:
					Output += "( ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::ScopeClose:
					Output += " )";
					break;
				default:
					checkf(false, TEXT("Invalid query token"));
					break;
				}
			}
			Output += " }";
		}
	}

	bool FQueryConditions::Verify(TConstArrayView<FColumnBase> AvailableColumns) const
	{
		return VerifyBootstrap(
			[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
			{
				for (const FColumnBase& Target : AvailableColumns)
				{
					if (Target.TypeInfo == Column)
					{
						return true;
					}
				}
				return false;
			});
	}

	bool FQueryConditions::Verify(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns, TConstArrayView<FColumnBase> AvailableColumns,
		bool AvailableColumnsAreSorted) const
	{
		static_assert(MaxColumnCount < 64, "Query conditions use a bit mask to locate matches. As a result MaxColumnCount can be larger than 64.");
		uint64 Matches = 0;
		bool Result = AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](const FColumnBase& ColumnBase) 
					{ 
						return ColumnBase.TypeInfo.Get();
					};
					if (Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE)
					{
						Matches |= (uint64)1 << ColumnIndex;
						return true;
					}
					return false;
				})
			: VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					for (const FColumnBase& Target : AvailableColumns)
					{
						if (Target.TypeInfo == Column)
						{
							Matches |= (uint64)1 << ColumnIndex;
							return true;
						}
					}
				return false;
				});
		if (Result)
		{
			ConvertColumnBitToArray(MatchedColumns, Matches);
		}
		return Result;
	}

	bool FQueryConditions::Verify(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns, bool AvailableColumnsAreSorted) const
	{
		return AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](TWeakObjectPtr<const UScriptStruct> ColumnBase)
					{ 
						return ColumnBase.Get();
					};
					return Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE;
				})
			: VerifyBootstrap(
				[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					return AvailableColumns.Find(Column) != INDEX_NONE;
				});
	}

	bool FQueryConditions::Verify(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns,
		TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns, bool AvailableColumnsAreSorted) const
	{
		static_assert(MaxColumnCount < 64, "Query conditions use a bit mask to locate matches. As a result MaxColumnCount can be larger than 64.");
		uint64 Matches = 0;
		bool Result = AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](TWeakObjectPtr<const UScriptStruct> ColumnBase)
					{
						return ColumnBase.Get();
					};
					if (Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE)
					{
						Matches |= (uint64)1 << ColumnIndex;
						return true;
					}
					return false;
				})
			: VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					return AvailableColumns.Find(Column) != INDEX_NONE;
				});
		if (Result)
		{
			ConvertColumnBitToArray(MatchedColumns, Matches);
		}
		return Result;
	}

	bool FQueryConditions::Verify(TSet<TWeakObjectPtr<const UScriptStruct>> AvailableColumns) const
	{
		return VerifyBootstrap(
			[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
			{
				return AvailableColumns.Find(Column) != nullptr;
			});
	}

	uint8_t FQueryConditions::MinimumColumnMatchRequired() const
	{
		if (TokenCount > 0)
		{
			uint8_t Front = 0;
			return MinimumColumnMatchRequiredRange(Front);
		}
		else if (ColumnCount == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	TConstArrayView<TWeakObjectPtr<const UScriptStruct>> FQueryConditions::GetColumns() const
	{
		return TConstArrayView<TWeakObjectPtr<const UScriptStruct>>(Columns, ColumnCount);
	}

	bool FQueryConditions::IsEmpty() const
	{
		return ColumnCount == 0;
	}

	void FQueryConditions::AppendName(FString& Output, TWeakObjectPtr<const UScriptStruct> TypeInfo) const
	{
#if WITH_EDITORONLY_DATA
		static FName DisplayNameName(TEXT("DisplayName"));
		if (const FString* Name = TypeInfo->FindMetaData(DisplayNameName))
		{
			Output += *Name;
		}
#else
		Output += TEXT("<Unavailable>");
#endif
	}

	bool FQueryConditions::EntersScopeNext(uint8_t Index) const
	{
		return Index < (TokenCount - 1) && Tokens[Index + 1] == Token::ScopeOpen;
	}

	bool FQueryConditions::EntersScope(uint8_t Index) const
	{
		return Tokens[Index] == Token::ScopeOpen;
	}

	bool FQueryConditions::Contains(TWeakObjectPtr<const UScriptStruct> ColumnType, const TArray<FColumnBase>& AvailableColumns) const
	{
		for (const FColumnBase& Column : AvailableColumns)
		{
			if (ColumnType == Column.TypeInfo)
			{
				return true;
			}
		}
		return false;
	}

	template<typename ContainsCallback>
	bool FQueryConditions::VerifyBootstrap(ContainsCallback&& Contains) const
	{
		if (TokenCount > 0)
		{
			uint8_t TokenIndex = 0;
			uint8_t ColumnIndex = 0;
			return VerifyRange(TokenIndex, ColumnIndex, Forward<ContainsCallback>(Contains));
		}
		else if (ColumnCount == 1)
		{
			return Contains(0, Columns[0]);
		}
		else
		{
			return false;
		}
	}

	template<typename ContainsCallback>
	bool FQueryConditions::VerifyRange(uint8_t& TokenIndex, uint8_t& ColumnIndex, ContainsCallback Contains) const
	{
		auto Init = [&]()
		{
			if (EntersScope(TokenIndex))
			{
				return VerifyRange(++TokenIndex, ColumnIndex, Contains);
			}
			else
			{
				bool Result =  Contains(ColumnIndex, Columns[ColumnIndex]);
				ColumnIndex++;
				return Result;
			}
		};
		bool Result = Init();

		while (TokenIndex < TokenCount)
		{
			Token T = Tokens[TokenIndex];
			++TokenIndex;
			switch (T)
			{
			case Token::And:
				if (EntersScope(TokenIndex))
				{
					Result = VerifyRange(++TokenIndex, ColumnIndex, Contains) && Result;
				}
				else
				{
					Result = Contains(ColumnIndex, Columns[ColumnIndex]) && Result;
					ColumnIndex++;
				}
				break;
			case Token::Or:
				if (EntersScope(TokenIndex))
				{
					Result = VerifyRange(++TokenIndex, ColumnIndex, Contains) || Result;
				}
				else
				{
					Result = Contains(ColumnIndex, Columns[ColumnIndex]) || Result;
					ColumnIndex++;
				}
				break;
			case Token::ScopeOpen:
				checkf(false, TEXT("The scope open in a query should be called during processing as it should be captured by an earlier statement."));
				break;
			case Token::ScopeClose:
				return Result;
			default:
				checkf(false, TEXT("Encountered an unknown query token."));
				break;
			}
		}
		return Result;
	}

	void FQueryConditions::ConvertColumnBitToArray(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns, uint64 ColumnBits) const
	{
		uint64 Index = 0;
		while (ColumnBits)
		{
			if (ColumnBits & 1)
			{
				MatchedColumns.Add(Columns[Index]);
			}
			++Index;
			ColumnBits >>= 1;
		}
	}

	uint8_t FQueryConditions::MinimumColumnMatchRequiredRange(uint8_t& Front) const
	{
		uint8_t Result = EntersScope(Front) ? MinimumColumnMatchRequiredRange(++Front) : 1;
		for (; Front < TokenCount; ++Front)
		{
			Token T = Tokens[Front];
			switch (T)
			{
			case Token::And:
				if (EntersScopeNext(Front))
				{
					Front += 2;
					Result += MinimumColumnMatchRequiredRange(Front);
				}
				else
				{
					++Result;
				}
				break;
			case Token::Or:
				if (EntersScopeNext(Front))
				{
					Front += 2;
					uint8_t Lhs = MinimumColumnMatchRequiredRange(Front);
					Result = FMath::Min(Result, Lhs);
				}
				break;
			case Token::ScopeOpen:
				checkf(false, TEXT("The scope open in a query should be called during processing as it should be captured by an earlier statement."));
				break;
			case Token::ScopeClose:
				++Front;
				return Result;
			default:
				checkf(false, TEXT("Encountered an unknown query token."));
				break;
			}
		}
		return Result;
	}

	void FQueryConditions::AppendQuery(FQueryConditions& Target, const FQueryConditions& Source)
	{
		checkf(Target.ColumnCount + Source.ColumnCount < MaxColumnCount, TEXT("Too many columns in the query."));
		for (uint8_t Index = 0; Index < Source.ColumnCount; ++Index)
		{
			Target.Columns[Target.ColumnCount + Index] = Source.Columns[Index];
		}

		checkf(Target.TokenCount + Source.TokenCount < MaxTokenCount, TEXT("Too many operations in the query. Try simplifying your query."));
		for (uint8_t Index = 0; Index < Source.TokenCount; ++Index)
		{
			Target.Tokens[Target.TokenCount + Index] = Source.Tokens[Index];
		}

		Target.ColumnCount += Source.ColumnCount;
		Target.TokenCount += Source.TokenCount;
	}

	FQueryConditions operator&&(const FQueryConditions& Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result = Lhs;
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		return Result;
	}

	FQueryConditions operator&&(const FQueryConditions& Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		return Result;
	}

	FQueryConditions operator&&(FColumnBase Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		return Result;
	}

	FQueryConditions operator&&(FColumnBase Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;
		return Result;
	}

	FQueryConditions operator||(const FQueryConditions& Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result = Lhs;
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		return Result;
	}

	FQueryConditions operator||(const FQueryConditions& Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		return Result;
	}

	FQueryConditions operator||(FColumnBase Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		return Result;
	}

	FQueryConditions operator||(FColumnBase Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;
		return Result;
	}


	//
	// DependsOn
	//

	FDependency::FDependency(ITypedElementDataStorageInterface::FQueryDescription* Query)
		: Query(Query)
	{
	}

	FDependency& FDependency::ReadOnly(const UClass* Target)
	{
		checkf(Target, TEXT("The Dependency section in the Typed Elements query builder doesn't support nullptrs as Read-Only input."));
		Query->DependencyTypes.Emplace(Target);
		Query->DependencyFlags.Emplace(ITypedElementDataStorageInterface::EQueryDependencyFlags::ReadOnly);
		Query->CachedDependencies.AddDefaulted();
		return *this;
	}

	FDependency& FDependency::ReadOnly(TConstArrayView<const UClass*> Targets)
	{
		int32 NewSize = Query->CachedDependencies.Num() + Targets.Num();
		Query->DependencyTypes.Reserve(NewSize);
		Query->CachedDependencies.Reserve(NewSize);
		Query->DependencyFlags.Reserve(NewSize);
		
		for (const UClass* Target : Targets)
		{
			ReadOnly(Target);
		}
		return *this;
	}

	FDependency& FDependency::ReadWrite(const UClass* Target)
	{
		checkf(Target, TEXT("The Dependency section in the Typed Elements query builder doesn't support nullptrs as Read/Write input."));
		Query->DependencyTypes.Emplace(Target);
		Query->DependencyFlags.Emplace(ITypedElementDataStorageInterface::EQueryDependencyFlags::None);
		Query->CachedDependencies.AddDefaulted();
		return *this;
	}

	FDependency& FDependency::ReadWrite(TConstArrayView<const UClass*> Targets)
	{
		int32 NewSize = Query->CachedDependencies.Num() + Targets.Num();
		Query->DependencyTypes.Reserve(NewSize);
		Query->CachedDependencies.Reserve(NewSize);
		Query->DependencyFlags.Reserve(NewSize);
		
		for (const UClass* Target : Targets)
		{
			ReadWrite(Target);
		}
		return *this;
	}

	FDependency& FDependency::SubQuery(TypedElementQueryHandle Handle)
	{
		Query->Subqueries.Add(Handle);
		return *this;
	}
	
	FDependency& FDependency::SubQuery(TConstArrayView<TypedElementQueryHandle> Handles)
	{
		Query->Subqueries.Insert(Handles.GetData(), Handles.Num(), Query->Subqueries.Num());
		return *this;
	}

	ITypedElementDataStorageInterface::FQueryDescription&& FDependency::Compile()
	{
		return MoveTemp(*Query);
	}


	/**
	 * Simple Query
	 */

	FSimpleQuery::FSimpleQuery(ITypedElementDataStorageInterface::FQueryDescription* Query)
		: Query(Query)
	{
		Query->bSimpleQuery = true;
	}

	FSimpleQuery& FSimpleQuery::All(const UScriptStruct* Target)
	{
		if (Target)
		{
			Query->ConditionTypes.Add(ITypedElementDataStorageInterface::FQueryDescription::EOperatorType::SimpleAll);
			Query->ConditionOperators.AddZeroed_GetRef().Type = Target;
		}
		return *this;
	}

	FSimpleQuery& FSimpleQuery::All(TConstArrayView<const UScriptStruct*> Targets)
	{
		int32 NewSize = Query->ConditionTypes.Num() + Targets.Num();
		Query->ConditionTypes.Reserve(NewSize);
		Query->ConditionOperators.Reserve(NewSize);
		
		for (const UScriptStruct* Target : Targets)
		{
			All(Target);
		}
		return *this;
	}

	FSimpleQuery& FSimpleQuery::Any(const UScriptStruct* Target)
	{
		if (Target)
		{
			Query->ConditionTypes.Add(ITypedElementDataStorageInterface::FQueryDescription::EOperatorType::SimpleAny);
			Query->ConditionOperators.AddZeroed_GetRef().Type = Target;
		}
		return *this;
	}

	FSimpleQuery& FSimpleQuery::Any(TConstArrayView<const UScriptStruct*> Targets)
	{
		int32 NewSize = Query->ConditionTypes.Num() + Targets.Num();
		Query->ConditionTypes.Reserve(NewSize);
		Query->ConditionOperators.Reserve(NewSize);

		for (const UScriptStruct* Target : Targets)
		{
			Any(Target);
		}
		return *this;
	}

	FSimpleQuery& FSimpleQuery::None(const UScriptStruct* Target)
	{
		if (Target)
		{
			Query->ConditionTypes.Add(ITypedElementDataStorageInterface::FQueryDescription::EOperatorType::SimpleNone);
			Query->ConditionOperators.AddZeroed_GetRef().Type = Target;
		}
		return *this;
	}

	FSimpleQuery& FSimpleQuery::None(TConstArrayView<const UScriptStruct*> Targets)
	{
		int32 NewSize = Query->ConditionTypes.Num() + Targets.Num();
		Query->ConditionTypes.Reserve(NewSize);
		Query->ConditionOperators.Reserve(NewSize);

		for (const UScriptStruct* Target : Targets)
		{
			None(Target);
		}
		return *this;
	}

	FDependency FSimpleQuery::DependsOn()
	{
		return FDependency{ Query };
	}

	ITypedElementDataStorageInterface::FQueryDescription&& FSimpleQuery::Compile()
	{
		Query->Callback.BeforeGroups.Shrink();
		Query->Callback.AfterGroups.Shrink();
		Query->SelectionTypes.Shrink();
		Query->SelectionAccessTypes.Shrink();
		for (TypedElementDataStorage::FColumnMetaData& Metadata : Query->SelectionMetaData)
		{
			Metadata.Shrink();
		}
		Query->SelectionMetaData.Shrink();
		Query->ConditionTypes.Shrink();
		Query->ConditionOperators.Shrink();
		Query->DependencyTypes.Shrink();
		Query->DependencyFlags.Shrink();
		Query->CachedDependencies.Shrink();
		Query->Subqueries.Shrink();
		Query->MetaData.Shrink();
		return MoveTemp(*Query);
	}


	/**
	 * FProcessor
	 */
	FProcessor::FProcessor(ITypedElementDataStorageInterface::EQueryTickPhase Phase, FName Group)
		: Phase(Phase)
		, Group(Group)
	{}

	FProcessor& FProcessor::SetPhase(ITypedElementDataStorageInterface::EQueryTickPhase NewPhase)
	{
		Phase = NewPhase;
		return *this;
	}

	FProcessor& FProcessor::SetGroup(FName GroupName)
	{
		Group = GroupName;
		return *this;
	}

	FProcessor& FProcessor::SetBeforeGroup(FName GroupName)
	{
		BeforeGroup = GroupName;
		return *this;
	}

	FProcessor& FProcessor::SetAfterGroup(FName GroupName)
	{
		AfterGroup = GroupName;
		return *this;
	}

	FProcessor& FProcessor::ForceToGameThread(bool bForce)
	{
		bForceToGameThread = bForce;
		return *this;
	}


	/**
	 * FObserver
	 */
	
	FObserver::FObserver(EEvent MonitorForEvent, const UScriptStruct* MonitoredColumn)
		: Monitor(MonitoredColumn)
		, Event(MonitorForEvent)
	{}

	FObserver& FObserver::SetEvent(EEvent MonitorForEvent)
	{
		Event = MonitorForEvent;
		return *this;
	}

	FObserver& FObserver::SetMonitoredColumn(const UScriptStruct* MonitoredColumn)
	{
		Monitor = MonitoredColumn;
		return *this;
	}

	FObserver& FObserver::ForceToGameThread(bool bForce)
	{
		bForceToGameThread = bForce;
		return *this;
	}


	/**
	 * FPhaseAmble
	 */

	FPhaseAmble::FPhaseAmble(ELocation InLocation, ITypedElementDataStorageInterface::EQueryTickPhase InPhase)
		: Phase(InPhase)
		, Location(InLocation)
	{}

	FPhaseAmble& FPhaseAmble::SetLocation(ELocation NewLocation)
	{
		Location = NewLocation;
		return *this;
	}

	FPhaseAmble& FPhaseAmble::SetPhase(ITypedElementDataStorageInterface::EQueryTickPhase NewPhase)
	{
		Phase = NewPhase;
		return *this;
	}

	FPhaseAmble& FPhaseAmble::ForceToGameThread(bool bForce)
	{
		bForceToGameThread = bForce;
		return *this;
	}


	/**
	 * Select
	 */

	Select::Select()
	{
		Query.Action = ITypedElementDataStorageInterface::FQueryDescription::EActionType::Select;
	}
	
	Select& Select::ReadOnly(const UScriptStruct* Target)
	{
		checkf(Target, TEXT("The Select section in the Typed Elements query builder doesn't support nullptrs as Read-Only input."));
		Query.SelectionTypes.Emplace(Target);
		Query.SelectionAccessTypes.Emplace(ITypedElementDataStorageInterface::EQueryAccessType::ReadOnly);
		Query.SelectionMetaData.Emplace(Target, TypedElementDataStorage::FColumnMetaData::EFlags::None);
		
		return *this;
	}

	Select& Select::ReadOnly(TConstArrayView<const UScriptStruct*> Targets)
	{
		int32 NewCount = Query.SelectionTypes.Num() + Targets.Num();
		Query.SelectionTypes.Reserve(NewCount);
		Query.SelectionAccessTypes.Reserve(NewCount);
		Query.SelectionMetaData.Reserve(NewCount);

		for (const UScriptStruct* Target : Targets)
		{
			ReadOnly(Target);
		}
		return *this;
	}

	Select& Select::ReadWrite(const UScriptStruct* Target)
	{
		checkf(Target, TEXT("The Select section in the Typed Elements query builder doesn't support nullptrs as Read/Write input."));
		Query.SelectionTypes.Emplace(Target);
		Query.SelectionAccessTypes.Emplace(ITypedElementDataStorageInterface::EQueryAccessType::ReadWrite);
		Query.SelectionMetaData.Emplace(Target, TypedElementDataStorage::FColumnMetaData::EFlags::IsMutable);
		return *this;
	}

	Select& Select::ReadWrite(TConstArrayView<const UScriptStruct*> Targets)
	{
		int32 NewCount = Query.SelectionTypes.Num() + Targets.Num();
		Query.SelectionTypes.Reserve(NewCount);
		Query.SelectionAccessTypes.Reserve(NewCount);
		Query.SelectionMetaData.Reserve(NewCount);

		for (const UScriptStruct* Target : Targets)
		{
			ReadWrite(Target);
		}
		return *this;
	}

	FSimpleQuery Select::Where()
	{
		return FSimpleQuery{ &Query };
	}

	FDependency Select::DependsOn()
	{
		return FDependency{ &Query };
	}

	ITypedElementDataStorageInterface::FQueryDescription&& Select::Compile()
	{
		return MoveTemp(Query);
	}


	/**
	 * Count
	 */

	Count::Count()
	{
		Query.Action = ITypedElementDataStorageInterface::FQueryDescription::EActionType::Count;
	}

	FSimpleQuery Count::Where()
	{
		return FSimpleQuery{ &Query };
	}

	FDependency Count::DependsOn()
	{
		return FDependency{ &Query };
	}
}