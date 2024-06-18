// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Misc/Attribute.h"

class ITypedElementDataStorageInterface;

namespace UE::EditorDataStorage
{
	// Concept for a conversion function used by the attribute binder to bind a column data member to an attribute of a different type
	template <typename FunctionType, typename ArgumentType>
	concept AttributeBinderInvocable = std::is_invocable_v<std::decay_t<FunctionType>, const ArgumentType&>;

	/**
	 * Builder class that can be used as a shorthand to bind data inside a TEDS row, column pair to a TAttribute so the attribute updates if the data
	 * in the column is changed.
	 * 
	 * Usage Example:
	 * 
	 * TypedElementDataStorage::FAttributeBinder Binder(RowHandle);
	 * TAttribute<int> TestAttribute(Binder.BindData(&FTestColumnInt::TestInt))
	 */
	class FAttributeBinder
	{
	public:



		// Create an attribute binder for a given row
		TYPEDELEMENTFRAMEWORK_API FAttributeBinder(TypedElementDataStorage::RowHandle InTargetRow);

		/**
		 * Bind a specific data member inside a TEDS column to an attribute of the same type as the data
		 *
		 * @param Variable The data member inside a column to be bound
		 * @return A TAttribute bound to the row, column pair specified
		 *
		 * Example:
		 * TypedElementDataStorage::FAttributeBinder Binder(RowHandle);
		 * TAttribute<FString> TestAttribute(Binder.BindData(&FTypedElementLabelColumn::Label))
		 */
		template <typename AttributeType, TypedElementDataStorage::TDataColumnType ColumnType>
		TAttribute<AttributeType> BindData(AttributeType ColumnType::* Variable);

		/**
		 * Bind a specific data member inside a TEDS column to an attribute of a different type than the data by providing a conversion function
		 *
		 * @param Variable The data member inside a column to be bound
		 * @param Converter Conversion function to convert from DataType -> AttributeType
		 * @return A TAttribute bound to the row, column pair specified
		 *
		 * Example:
		 * TypedElementDataStorage::FAttributeBinder Binder(RowHandle);
		 * TAttribute<FText> TestAttribute(Binder.BindData(&FTypedElementLabelColumn::Label),
		 *                                 [](const FString& Data)
		 *                                   {
		 *                                      return FText::FromString(Data);
		 *                                   }
		 *                                 ));
		 */
		template <typename AttributeType, typename DataType, TypedElementDataStorage::TDataColumnType ColumnType>
		TAttribute<AttributeType> BindData(DataType ColumnType::* Variable, const TFunction<AttributeType(const DataType&)>& Converter);

		/**
		 * Overload for the conversion binder to accept lambdas instead of TFunctions
		 *
		 * @param Variable The data member inside a column to be bound
		 * @param Converter Conversion function to convert from DataType -> AttributeType
		 * @return A TAttribute bound to the row, column pair specified
		 */
		template <typename DataType, TypedElementDataStorage::TDataColumnType ColumnType, typename FunctionType>
			requires AttributeBinderInvocable<FunctionType, DataType>
		auto BindData(DataType ColumnType::* Variable, FunctionType Converter);
		
	private:

		// The target row for this binder
		TypedElementDataStorage::RowHandle TargetRow;

		// A ptr to the data storage for quick access
		ITypedElementDataStorageInterface* DataStorage;
	};
}


#include "TypedElementAttributeBinding.inl"