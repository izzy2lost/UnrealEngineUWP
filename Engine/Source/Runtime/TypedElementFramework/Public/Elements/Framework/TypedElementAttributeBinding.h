// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Misc/Attribute.h"

class ITypedElementDataStorageInterface;

namespace UE::Editor::DataStorage
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
	 * FAttributeBinder Binder(RowHandle);
	 * TAttribute<int> TestAttribute(Binder.BindData(&FTestColumnInt::TestInt))
	 */
	class FAttributeBinder
	{
	public:

		/* Create an attribute binder for a given row. */
		TYPEDELEMENTFRAMEWORK_API FAttributeBinder(RowHandle InTargetRow);
		
		/* Create an attribute binder for a given row. */
		TYPEDELEMENTFRAMEWORK_API FAttributeBinder(RowHandle InTargetRow, ITypedElementDataStorageInterface* InDataStorage);

		/**
		 * Bind a specific data member inside a TEDS column to an attribute of the same type as the data
		 * 
		 * @param InVariable The data member inside a column to be bound
		 * @param InDefaultValue The default value to be used when the column isn't present on a row
		 * @return A TAttribute bound to the row, column pair specified
		 *
		 * Example:
		 * FAttributeBinder Binder(RowHandle);
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
		 * UE::Editor::DataStorage::FAttributeBinder Binder(RowHandle);
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

		/**
		 * Directly bind an FString member in a Teds column to an FText attribute as a shortcut
		 * @param InFStringVariable The FString variable 
		 * @return A delegate that can be provided to a text widget in Slate (e.g STextBlock)
		 */
		template <TDataColumnType ColumnType>
		TAttribute<FText> BindText(FString ColumnType::* InFStringVariable);

		/**
		 * Directly bind an FName member in a Teds column to an FText attribute as a shortcut
		 * @param InFNameVariable The FName variable 
		 * @return A delegate that can be provided to a text widget in Slate (e.g STextBlock)
		 */
		template <TDataColumnType ColumnType>
		TAttribute<FText> BindText(FName ColumnType::* InFNameVariable);
		
	private:

		// The target row for this binder
		RowHandle TargetRow;

		// A ptr to the data storage for quick access
		ITypedElementDataStorageInterface* DataStorage;
	};
} // namespace UE::Editor::DataStorage

#include "TypedElementAttributeBinding.inl"