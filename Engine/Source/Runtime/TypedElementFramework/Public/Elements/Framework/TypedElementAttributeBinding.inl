// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TypedElementRegistry.h"

namespace UE::EditorDataStorage
{
	// A property that can be directly be accessed from an object
	template <typename PropertyType>
	struct DirectProperty final
	{
		// Offset to a particular data member inside the object
		size_t Offset;

		PropertyType* Get(void* Object) const
		{ 
			return reinterpret_cast<PropertyType*>((reinterpret_cast<char*>(Object) + Offset));
		}
		const PropertyType* Get(const void* Object) const
		{ 
			return reinterpret_cast<const PropertyType*>((reinterpret_cast<const char*>(Object) + Offset)); 
		}
	};

	// A property that goes through a conversion function before being accessed from the object
	template <typename PropertyType>
	struct ConvertibleProperty final
	{
		// Conversion function
		TFunction<PropertyType(const void*)> Converter;

		// Cache to avoid a copy when returning if not necessary
		mutable PropertyType Cache;

		PropertyType* Get(void* Object) const
		{ 
			Cache = Converter(Object);
			return &Cache;
		}
		const PropertyType* Get(const void* Object) const
		{ 
			Cache = Converter(Object);
			return &Cache;
		}
	};

	template <typename PropertyType>
	class Property
	{
	public:
		
		// Bind this property directly
		template <TypedElementDataStorage::TDataColumnType ObjectType>
		void Bind(PropertyType ObjectType::* Variable)
		{
			DirectProperty<PropertyType> Result;
			Result.Offset = reinterpret_cast<size_t>(&(reinterpret_cast<const ObjectType*>(0)->*Variable));
			
			ObjectTypeInfo = ObjectType::StaticStruct();
			InternalProperty = InternalPropertyType(TInPlaceType<DirectProperty<PropertyType>>(), Result);;
		}

		// Bind this property using a conversion function
		template <typename InputType, TypedElementDataStorage::TDataColumnType ObjectType>
		void Bind(InputType ObjectType::* Variable, TFunction<PropertyType(const InputType&)> Converter)
		{
			ConvertibleProperty<PropertyType> Result;
			Result.Converter = [Converter, Variable](const void* Object)
			{
				return Converter(reinterpret_cast<const ObjectType*>(Object)->*Variable);
			};
			
			ObjectTypeInfo = ObjectType::StaticStruct();
			InternalProperty = InternalPropertyType(TInPlaceType<ConvertibleProperty<PropertyType>>(), Result);;
		}

		// Get the bound property for the specified object
		template <TypedElementDataStorage::TDataColumnType ObjectType>
		PropertyType& Get(ObjectType& Object) const
		{
			return Get(&Object, ObjectType::StaticStruct());
		}

		// Get the bound property for the specified object
		template <TypedElementDataStorage::TDataColumnType ObjectType>
		const PropertyType& Get(const ObjectType& Object) const
		{
			return Get(&Object, ObjectType::StaticStruct());
		}
		
		// Get the bound property for this specified object ptr by providing type information about the object
		PropertyType& Get(void* Object, const UScriptStruct* Type) const
		{
			ensureMsgf(Type == ObjectTypeInfo, TEXT("Provided object type (%s) did not match bound object type (%s)."),
				*Type->GetFName().ToString(), *ObjectTypeInfo->GetFName().ToString());

			return *Visit([Object](auto&& Prop) { return Prop.Get(Object); }, InternalProperty);
		}
		
		// Get the bound property for this specified object ptr by providing type information about the object
		const PropertyType& Get(const void* Object, const UScriptStruct* Type) const
		{
			ensureMsgf(Type == ObjectTypeInfo, TEXT("Provided object type (%s) did not match bound object type (%s)."),
				*Type->GetFName().ToString(), *ObjectTypeInfo->GetFName().ToString());

			return *Visit([Object](auto&& Prop) { return Prop.Get(Object); }, InternalProperty);
		}

	private:

		// Internally we could be storing a direct property or a convertible property
		using InternalPropertyType = TVariant<DirectProperty<PropertyType>, ConvertibleProperty<PropertyType>>;

		// The actual property
		InternalPropertyType InternalProperty;

		// The type of the object we are bound to
		UScriptStruct* ObjectTypeInfo = nullptr;
	};
	
	template <typename AttributeType, TypedElementDataStorage::TDataColumnType ColumnType>
	TAttribute<AttributeType> FAttributeBinder::BindData(AttributeType ColumnType::* InVariable, const AttributeType& InDefaultValue)
	{
		if(!DataStorage)
		{
			return TAttribute<AttributeType>();
		}
		
		// Create a direct property and bind it to the given variable
		Property<AttributeType> Prop;
		Prop.Bind(InVariable);

		// We don't want any references to this in the lambda because binders are designed to be used and destructed on the stack
		return TAttribute<AttributeType>::CreateLambda([Property = Prop, Storage = DataStorage, Row = TargetRow, DefaultValue = InDefaultValue]()
		{
			// Get the column from the given row and use that to return the stored property
			if(ColumnType* Column = Storage->GetColumn<ColumnType>(Row))
			{
				return Property.Get(Column, ColumnType::StaticStruct());
			}
			return DefaultValue;
		});
	}

	template <typename AttributeType, typename DataType, TypedElementDataStorage::TDataColumnType ColumnType>
	TAttribute<AttributeType> FAttributeBinder::BindData(DataType ColumnType::* InVariable, const TFunction<AttributeType(const DataType&)>& InConverter, const DataType& InDefaultValue)
	{
		if(!DataStorage)
		{
			return TAttribute<AttributeType>();
		}
		
		// Create a convertible property and bind it to the given variable
		Property<AttributeType> Prop;
		Prop.Bind(InVariable, InConverter);
	
		return TAttribute<AttributeType>::CreateLambda([Property = Prop, Storage = DataStorage, Row = TargetRow, DefaultValue = InDefaultValue, Converter = InConverter]()
		{
			if(ColumnType* Column = Storage->GetColumn<ColumnType>(Row))
			{
				return Property.Get(Column, ColumnType::StaticStruct());
			}

			return Converter(DefaultValue);
		});
	}
	
	template <typename DataType, TypedElementDataStorage::TDataColumnType ColumnType, typename FunctionType>
		requires AttributeBinderInvocable<FunctionType, DataType>
	auto FAttributeBinder::BindData(DataType ColumnType::* InVariable, FunctionType InConverter, const DataType& InDefaultValue)
	{
		// Deduce the attribute type from the return value of the converter function
		using AttributeType = decltype(InConverter(std::declval<DataType>()));
		
		return BindData<AttributeType, DataType>(InVariable, TFunction<AttributeType(const DataType&)>(InConverter), InDefaultValue);
	}
	
}

	