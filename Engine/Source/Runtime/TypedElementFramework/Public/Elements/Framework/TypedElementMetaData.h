// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/TVariant.h"

class UScriptStruct;

namespace TypedElementDataStorage
{
	struct FQueryDescription;

	inline static const FName IsEditableName(TEXT("IsEditable"));
	inline static const FName IsConstName(TEXT("IsConst"));

	using MetaDataType = TVariant<bool, uint64, int64, double, FString>;
	using MetaDataTypeView = TVariant<FEmptyVariantState, bool, uint64, int64, double, const FString*>;

	/**
	 * Short lived view of single entry in the meta data container.
	 */
	class FMetaDataEntryView final
	{
	public:
		TYPEDELEMENTFRAMEWORK_API FMetaDataEntryView();
		TYPEDELEMENTFRAMEWORK_API explicit FMetaDataEntryView(const MetaDataType& MetaData);
		TYPEDELEMENTFRAMEWORK_API explicit FMetaDataEntryView(const FString& MetaDataString);
		/** Explicit constructor. The provided type must match exactly to one of the available stored types. */
		template<typename T>
		explicit FMetaDataEntryView(T&& MetaDataValue);

		/** Returns true if set to a value, otherwise false. */
		TYPEDELEMENTFRAMEWORK_API bool IsSet() const;
		/** Checks if the stored value matches the requested type. */
		template<typename T>
		bool IsType() const;
		/** Returns the value if the requested type matches exactly with the stored type, otherwise returns a nullptr. */
		template<typename T>
		const T* TryGetExact() const;

	private:
		MetaDataTypeView DataView;
	};

	/**
	 * Base class to store meta data for use within the Typed Elements Data Storage.
	 */
	class FMetaDataBase
	{
	public:
		template<typename T>
		bool AddImmutableData(FName Name, T&& Value);
		template<typename T>
		void AddOrSetMutableData(FName Name, T&& Value);

		TYPEDELEMENTFRAMEWORK_API virtual FMetaDataEntryView Find(FName Name) const;

		TYPEDELEMENTFRAMEWORK_API virtual void Shrink();

	protected:
		FMetaDataBase() = default;
		virtual ~FMetaDataBase() = default;

		/** Data that can be added once but can't be changed afterwards. Values here always take priority over other values. */
		TMap<FName, MetaDataType> ImmutableData;
		/** Data that can be added and can have their value updated afterwards. */
		TMap<FName, MetaDataType> MutableData;
	};

	/** General storage for meta data for the Typed Elements Data Storage. */
	class FMetaData final : public FMetaDataBase {};

	/**
	 * Meta data that's specifically associated with a single column.
	 */
	class FColumnMetaData final : public FMetaDataBase
	{
	public:
		enum class EFlags
		{
			None = 0,
			IsMutable = 1 << 0
		};

		FColumnMetaData() = default;
		TYPEDELEMENTFRAMEWORK_API FColumnMetaData(const UScriptStruct* InColumnType, EFlags InFlags);

		TYPEDELEMENTFRAMEWORK_API FMetaDataEntryView Find(FName Name) const override;

	private:
		/** If set, properties on the column will also be included if a value isn't found in the (im)mutable data map. */
		const UScriptStruct* ColumnType{ nullptr };
		/** Flags indicating the behavior of the column */
		EFlags Flags = EFlags::None;
	};

	/**
	 * Short lived view of a meta data container optionally associated with a query.
	 */
	class FMetaDataView final
	{
	public:
		enum class ESearchScope
		{
			FallbackOnGeneric, // If the attribute isn't found on the column or the column isn't found, search the generic meta data for the attribute.
			ColumnOnly // Only use the data on the column.
		};

		FMetaDataView() = default;
		TYPEDELEMENTFRAMEWORK_API FMetaDataView(const TypedElementDataStorage::FQueryDescription& InQuery); // Deliberately avoided "explicit".
		TYPEDELEMENTFRAMEWORK_API FMetaDataView(const FMetaData& InQueryWideMetaData); // Deliberately avoided "explicit".
		TYPEDELEMENTFRAMEWORK_API FMetaDataView(
			const TypedElementDataStorage::FQueryDescription& InQuery, const FMetaData& InQueryWideMetaData);

		TYPEDELEMENTFRAMEWORK_API FMetaDataEntryView FindGeneric(FName AttributeName) const;
		TYPEDELEMENTFRAMEWORK_API FMetaDataEntryView FindForColumn(
			TWeakObjectPtr<const UScriptStruct> Column, FName AttributeName, ESearchScope Scope = ESearchScope::ColumnOnly) const;
		template<typename Column>
		FMetaDataEntryView FindForColumn(FName AttributeName, ESearchScope Scope = ESearchScope::ColumnOnly) const;

	private:
		const TypedElementDataStorage::FQueryDescription* Query{ nullptr };
		const FMetaData* QueryWideMetaData{ nullptr };
	};
} // TypedElementDataStorage

#include "Elements/Framework/TypedElementMetaData.inl"