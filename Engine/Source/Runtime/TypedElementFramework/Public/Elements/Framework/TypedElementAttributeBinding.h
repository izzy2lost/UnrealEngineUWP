// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Misc/Attribute.h"

class ITypedElementDataStorageInterface;

namespace UE::EditorDataStorage
{
	using namespace UE::Editor::DataStorage;

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

		/* Create an attribute binder for a given row. */
		TYPEDELEMENTFRAMEWORK_API explicit FAttributeBinder(TypedElementDataStorage::RowHandle InTargetRow);
		
		/* Create an attribute binder for a given row. */
		TYPEDELEMENTFRAMEWORK_API FAttributeBinder(TypedElementDataStorage::RowHandle InTargetRow, ITypedElementDataStorageInterface* InDataStorage);

		/**
		 * Bind a specific data member inside a TEDS column to an attribute of the same type as the data
		 * 
		 * @param InVariable The data member inside a column to be bound
		 * @param InDefaultValue The default value to be used when the column isn't present on a row
		 * @return A TAttribute bound to the row, column pair specified
		 *
		 * Example:
		 * TypedElementDataStorage::FAttributeBinder Binder(RowHandle);
		 * TAttribute<FString> TestAttribute(Binder.BindData(&FTypedElementLabelColumn::Label))
		 */
		template <typename AttributeType, TDataColumnType ColumnType>
		TAttribute<AttributeType> BindData(AttributeType ColumnType::* InVariable, const AttributeType& InDefaultValue = AttributeType());

		/**
		 * Bind a specific data member inside a TEDS column to an attribute of a different type than the data by providing a conversion function
		 * NOTE: the default value is not the actual attribute type but rather the data type in the column and it gets passed to the conversion function
		 *
		 * @param InVariable The data member inside a column to be bound
		 * @param InConverter Conversion function to convert from DataType -> AttributeType
		 * @param InDefaultValue The default value to be used when the column isn't present on a row 
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
		template <typename AttributeType, typename DataType, TDataColumnType ColumnType>
		TAttribute<AttributeType> BindData(DataType ColumnType::* InVariable, const TFunction<AttributeType(const DataType&)>& InConverter, const DataType& InDefaultValue = DataType());

		/**
		 * Overload for the conversion binder to accept lambdas instead of TFunctions
		 *
		 * @param InVariable The data member inside a column to be bound
		 * @param InConverter Conversion function to convert from DataType -> AttributeType
		 * @param InDefaultValue The default value to be used when the column isn't present on a row
		 * @return A TAttribute bound to the row, column pair specified
		 */
		template <typename DataType, TDataColumnType ColumnType, typename FunctionType>
			requires AttributeBinderInvocable<FunctionType, DataType>
		auto BindData(DataType ColumnType::* InVariable, FunctionType InConverter, const DataType& InDefaultValue = DataType());

		/**
		 * Bind a delegate inside a Teds column to a SLATE_EVENT macro on a widget
		 * @param InVariable The delegate inside the Teds column
		 * @return A delegate that can be provided to an event on a slate widget
		 */
		template <typename InRetValType, typename... ParamTypes, typename ColumnType>
		TDelegate<InRetValType(ParamTypes...)> BindEvent(TDelegate<InRetValType(ParamTypes...)> ColumnType::* InVariable);
		
	private:

		// The target row for this binder
		TypedElementDataStorage::RowHandle TargetRow;

		// A ptr to the data storage for quick access
		ITypedElementDataStorageInterface* DataStorage;
	};
}

#include "TypedElementAttributeBinding.inl"