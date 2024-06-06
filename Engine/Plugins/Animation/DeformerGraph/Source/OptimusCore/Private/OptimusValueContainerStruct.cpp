// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusValueContainerStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusValueContainerStruct)

const TCHAR* FOptimusValueContainerStruct::ValuePropertyName = TEXT("Value");

bool FOptimusValueContainerStruct::IsInitialized() const
{
	return Value.GetNumPropertiesInBag() == 1;
}

void FOptimusValueContainerStruct::SetType(FOptimusDataTypeRef InDataType)
{
	Value.Reset();
	Value.AddProperty(ValuePropertyName, InDataType->CreateProperty(nullptr, ValuePropertyName));		
}

FShaderValueContainer FOptimusValueContainerStruct::GetShaderValue(FOptimusDataTypeRef InDataType) const
{
	check(Value.GetNumPropertiesInBag() == 1);

	const UPropertyBag* BagStruct = Value.GetPropertyBagStruct();
	TConstArrayView<FPropertyBagPropertyDesc> Descs = BagStruct->GetPropertyDescs();

	const FPropertyBagPropertyDesc& ValueDesc = Descs[0];

	if (ensure(InDataType.IsValid()) && ensure(ValueDesc.CachedProperty))
	{
		TArrayView<const uint8> ValueData(ValueDesc.CachedProperty->ContainerPtrToValuePtr<uint8>(Value.GetValue().GetMemory()), ValueDesc.CachedProperty->GetSize());
		FShaderValueContainer ValueResult = InDataType->MakeShaderValue();
		if (InDataType->ConvertPropertyValueToShader(ValueData, ValueResult))
		{
			return ValueResult;
		}
	}
	
	return {};
}

const FProperty* FOptimusValueContainerStruct::GetValueProperty() const
{
	check(Value.GetNumPropertiesInBag() == 1);
	
	const UPropertyBag* BagStruct = Value.GetPropertyBagStruct();
	TConstArrayView<FPropertyBagPropertyDesc> Descs = BagStruct->GetPropertyDescs();

	const FPropertyBagPropertyDesc& ValueDesc = Descs[0];

	return ValueDesc.CachedProperty;
}

const uint8* FOptimusValueContainerStruct::GetValueMemory() const
{
	return Value.GetValue().GetMemory();
}

uint8* FOptimusValueContainerStruct::GetMutableValueMemory()
{
	return Value.GetMutableValue().GetMemory();
}
