// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

#include "Misc/TVariant.h"

using FPropertyValue = TVariant<FEmptyVariantState, bool, int64, double, FString, TArray<bool>, TArray<int64>, TArray<double>, TArray<FString>>;

struct CAPTURESOURCEFRAMEWORK_API FPropertyDesc
{
	enum class EType
	{
		Bool = 0,
		Number,
		FloatingPoint,
		String,
		ArrayBool,
		ArrayNumber,
		ArrayString,
		ArrayFloatingPoint,
		Any
	};

	enum class EAccess
	{
		ReadOnly = 0,
		ReadWrite
	};

	FString Name;
	EType Type;
	EAccess Access;

	FPropertyDesc(FString InName, EType InType, EAccess InAccess);
	FPropertyDesc(FString InName, EType InType);
};

template<typename T>
FPropertyValue MakePropertyValue(T InValue)
{
	return FPropertyValue(TInPlaceType<T>(), MoveTemp(InValue));
}

class FPropertyList
{
public:

	FPropertyList() = default;

	void AddProperty(FPropertyDesc InPropertyDesc);
	TArray<FPropertyDesc> GetProperties() const;

	void CheckPropertyExists(const FString& InName);
	void CheckPropertyValueTypeMatch(const FString& InName, const FPropertyValue& InValue);
	void CheckPropertyAllowsWrite(const FString& InName);

	static bool CheckPropertyValueType(FPropertyDesc::EType InType, const FPropertyValue& InValue);

private:

	TArray<FPropertyDesc> Properties;
};

