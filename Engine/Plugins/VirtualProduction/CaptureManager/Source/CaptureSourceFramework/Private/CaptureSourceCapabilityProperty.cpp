// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceCapabilityProperty.h"

FPropertyDesc::FPropertyDesc(FString InName, EType InType, EAccess InAccess)
	: Name(MoveTemp(InName))
	, Type(InType)
	, Access(InAccess)
{
}

FPropertyDesc::FPropertyDesc(FString InName, EType InType)
	: Name(MoveTemp(InName))
	, Type(InType)
	, Access(EAccess::ReadWrite)
{
}

void FPropertyList::AddProperty(FPropertyDesc InPropertyDesc)
{
	Properties.Add(MoveTemp(InPropertyDesc));
}

TArray<FPropertyDesc> FPropertyList::GetProperties() const
{
	return Properties;
}

void FPropertyList::CheckPropertyExists(const FString& InName)
{
	check(Properties.ContainsByPredicate([&InName](const FPropertyDesc& InProperty)
	{
		return InProperty.Name == InName;
	}));
}

void FPropertyList::CheckPropertyValueTypeMatch(const FString& InName, const FPropertyValue& InValue)
{
	const FPropertyDesc* Property = Properties.FindByPredicate([&InName](const FPropertyDesc& InProperty)
	{
		return InProperty.Name == InName;
	});

	check(Property);

	check(CheckPropertyValueType(Property->Type, InValue));
}

void FPropertyList::CheckPropertyAllowsWrite(const FString& InName)
{
	const FPropertyDesc* Property = Properties.FindByPredicate([&InName](const FPropertyDesc& InProperty)
	{
		return InProperty.Name == InName;
	});

	check(Property);

	check(Property->Access == FPropertyDesc::EAccess::ReadWrite);
}

bool FPropertyList::CheckPropertyValueType(FPropertyDesc::EType InType, const FPropertyValue& InValue)
{
	switch (InType)
	{
		case FPropertyDesc::EType::Bool:
			return InValue.IsType<bool>();
		case FPropertyDesc::EType::Number:
			return InValue.IsType<int64>();
		case FPropertyDesc::EType::String:
			return InValue.IsType<FString>();
		case FPropertyDesc::EType::FloatingPoint:
			return InValue.IsType<double>();
		case FPropertyDesc::EType::ArrayBool:
			return InValue.IsType<TArray<bool>>();
		case FPropertyDesc::EType::ArrayNumber:
			return InValue.IsType<TArray<int64>>();
		case FPropertyDesc::EType::ArrayString:
			return InValue.IsType<TArray<FString>>();
		case FPropertyDesc::EType::ArrayFloatingPoint:
			return InValue.IsType<TArray<double>>();
		default:
			return false;
	}
}