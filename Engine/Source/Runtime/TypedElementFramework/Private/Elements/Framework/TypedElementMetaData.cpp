// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementMetaData.h"

#include "Elements/Common/TypedElementQueryDescription.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace TypedElementDataStorage
{
	template<class... Ts>
	struct TOverloaded : Ts... 
	{ 
		using Ts::operator()...;
	};
	
	template<class... Ts>
	TOverloaded(Ts...) -> TOverloaded<Ts...>;

	//
	// FMetaDataBase
	//

	FMetaDataEntryView FMetaDataBase::Find(FName Name) const
	{
		if (const MetaDataType* Result = ImmutableData.Find(Name))
		{
			return FMetaDataEntryView(*Result);
		}

		if (const MetaDataType* Result = MutableData.Find(Name))
		{
			return FMetaDataEntryView(*Result);
		}

		return FMetaDataEntryView();
	}

	void FMetaDataBase::Shrink()
	{
		ImmutableData.Shrink();
		MutableData.Shrink();
	}



	//
	// FColumnMetadata
	//

	FColumnMetaData::FColumnMetaData(const UScriptStruct* InColumnType, EFlags InFlags)
		: ColumnType(InColumnType)
		, Flags(InFlags)
	{}

	FMetaDataEntryView FColumnMetaData::Find(FName Name) const
	{
		if (Name == IsEditableName)
		{
			return FMetaDataEntryView((Flags & EFlags::IsMutable) != EFlags::None);
		}
		else if (Name == IsConstName)
		{
			return FMetaDataEntryView(!(Flags & EFlags::IsMutable));
		}

		FMetaDataEntryView Result = FMetaDataBase::Find(Name);

#if WITH_EDITORONLY_DATA
		if (ColumnType && !Result.IsSet())
		{
			if (const FString* FoundMetaData = ColumnType->FindMetaData(Name))
			{
				return FMetaDataEntryView(*FoundMetaData);
			}
		}
#endif

		return Result;
	}


	//
	// FMetaDataEntryView
	// 

	FMetaDataEntryView::FMetaDataEntryView()
		: DataView(TInPlaceType<FEmptyVariantState>())
	{}

	FMetaDataEntryView::FMetaDataEntryView(const MetaDataType& MetaData)
	{
		Visit(TOverloaded
			{
				[this](const auto& Value)
				{
					using TargetType = typename TDecay<decltype(Value)>::Type;
					DataView.Emplace<TargetType>(Value);
				},
				[this](const FString& String)
				{
					DataView.Emplace<const FString*>(&String);
				}
			}, MetaData);
	}

	FMetaDataEntryView::FMetaDataEntryView(const FString& MetaDataString)
		: DataView(TInPlaceType<const FString*>(), &MetaDataString)
	{}

	bool FMetaDataEntryView::IsSet() const
	{
		return !DataView.IsType<FEmptyVariantState>();
	}



	//
	// FMetaDataView
	//

	FMetaDataView::FMetaDataView(const TypedElementDataStorage::FQueryDescription& InQuery)
		: Query(&InQuery)
	{}

	FMetaDataView::FMetaDataView(const FMetaData& InQueryWideMetaData)
		: QueryWideMetaData(&InQueryWideMetaData)
	{}

	FMetaDataView::FMetaDataView(const TypedElementDataStorage::FQueryDescription& InQuery, const FMetaData& InQueryWideMetaData)
		: Query(&InQuery)
		, QueryWideMetaData(&InQueryWideMetaData)
	{}

	FMetaDataEntryView FMetaDataView::FindGeneric(FName AttributeName) const
	{
		return QueryWideMetaData ? QueryWideMetaData->Find(AttributeName) : FMetaDataEntryView();
	}

	FMetaDataEntryView FMetaDataView::FindForColumn(TWeakObjectPtr<const UScriptStruct> Column, FName AttributeName, ESearchScope Scope) const
	{
		if (Query)
		{
			// There are typically a small number of columns in a query, so a linear search if often fast enough and can even
			// be faster than a map.
			int32 Index = 0;
			if (Query->SelectionTypes.Find(Column, Index))
			{
				FMetaDataEntryView Result = Query->SelectionMetaData[Index].Find(AttributeName);
				if (Result.IsSet())
				{
					return Result;
				}
			}
		}
		return Scope == ESearchScope::FallbackOnGeneric && QueryWideMetaData
			? QueryWideMetaData->Find(AttributeName)
			: FMetaDataEntryView();
	}
} // namespace TypedElementDataStorage
