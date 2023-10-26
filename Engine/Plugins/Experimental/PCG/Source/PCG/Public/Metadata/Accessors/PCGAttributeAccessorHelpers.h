// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

enum class EPCGExtraProperties : uint8;
class IPCGAttributeAccessor;
class IPCGAttributeAccessorKeyIterator;
class IPCGAttributeAccessorKeys;
class FProperty;
class UClass;
class UPCGData;
class UStruct;
struct FPCGAttributePropertySelector;
struct FPCGDataCollection;
struct FPCGSettingsOverridableParam;


namespace PCGAttributeAccessorHelpers
{
	PCG_API bool IsPropertyAccessorSupported(const FProperty* InProperty);
	PCG_API bool IsPropertyAccessorSupported(const FName InPropertyName, const UStruct* InStruct);
	PCG_API bool IsPropertyAccessorChainSupported(const TArray<FName>& InPropertyNames, const UStruct* InStruct);

	PCG_API TUniquePtr<IPCGAttributeAccessor> CreatePropertyAccessor(const FProperty* InProperty);
	PCG_API TUniquePtr<IPCGAttributeAccessor> CreatePropertyAccessor(const FName InPropertyName, const UStruct* InStruct);

	PCG_API TUniquePtr<IPCGAttributeAccessor> CreatePropertyChainAccessor(TArray<const FProperty*>&& InProperties);
	PCG_API TUniquePtr<IPCGAttributeAccessor> CreatePropertyChainAccessor(const TArray<FName>& InPropertyNames, const UStruct* InStruct);

	PCG_API TUniquePtr<IPCGAttributeAccessor> CreateExtraAccessor(EPCGExtraProperties InExtraProperties);

	PCG_API TUniquePtr<IPCGAttributeAccessor> CreateChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, FName Name, bool& bOutSuccess);

	struct AccessorParamResult
	{
		FName AttributeName = NAME_None;
		FName AliasUsed = NAME_None;
		bool bUsedAliases = false;
		bool bPinConnected = false;
	};

	UE_DEPRECATED(5.3, "Use the CreateConstAccessorForOverrideParamWithResult version")
	PCG_API TUniquePtr<const IPCGAttributeAccessor> CreateConstAccessorForOverrideParam(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, FName* OutAttributeName = nullptr);

	/**
	* Create a const accessor depending on an overridable param
	*/
	PCG_API TUniquePtr<const IPCGAttributeAccessor> CreateConstAccessorForOverrideParamWithResult(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, AccessorParamResult* OutResult = nullptr);

	/**
	* Creates a const accessor to the property or attribute pointed at by the InSelector.
	* Note that InData must not be null if the selector points to an attribute,
	* but in the case of properties, it either has to be the appropriate type or null.
	* Make sure to update your selector before-hand if you want to support "@Last"
	*/
	PCG_API TUniquePtr<const IPCGAttributeAccessor> CreateConstAccessor(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector);

	/**
	* Creates a accessor to the property or attribute pointed at by the InSelector.
	* Note that InData must not be null if the selector points to an attribute,
	* but in the case of properties, it either has to be the appropriate type or null.
	* Make sure to update your selector before-hand if you want to support "@Source". Otherwise the creation will fail.
	*/
	PCG_API TUniquePtr<IPCGAttributeAccessor> CreateAccessor(UPCGData* InData, const FPCGAttributePropertySelector& InSelector);

	PCG_API TUniquePtr<const IPCGAttributeAccessorKeys> CreateConstKeys(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector);
	PCG_API TUniquePtr<IPCGAttributeAccessorKeys> CreateKeys(UPCGData* InData, const FPCGAttributePropertySelector& InSelector);

	/**
	* Sorts array given the accessors and keys of the array
	*/
	template <typename T>
	void SortByAttribute(const IPCGAttributeAccessor& InAccessor, const IPCGAttributeAccessorKeys& InKeys, TArray<T>& InArray, bool bAscending)
	{
		SortByAttribute(InAccessor, InKeys, InArray, bAscending, [](int Index) { return Index; });
	}

	/**
	* Sorts array given the accessors and keys of the array.
	* A custom function is used to extract the index of the elements of the array,
	* and sort by the values associated with that index.
	*/
	template <typename T, typename Func>
	void SortByAttribute(const IPCGAttributeAccessor& InAccessor, const IPCGAttributeAccessorKeys& InKeys, TArray<T>& InArray, bool bAscending, Func&& CustomGetIndex)
	{
		check(InArray.Num() <= InKeys.GetNum())

		if (InArray.IsEmpty())
		{
			return;
		}

		auto Callback = [&InAccessor, &InKeys, &InArray, bAscending, &CustomGetIndex](auto Dummy)
		{
			using ValueType = decltype(Dummy);

			if constexpr (PCG::Private::MetadataTraits<ValueType>::CanCompare)
			{
				TArray<ValueType> CachedValues;

				if constexpr (std::is_trivially_copyable_v<ValueType>)
				{
					CachedValues.SetNumUninitialized(InKeys.GetNum());
				}
				else
				{
					CachedValues.SetNum(InKeys.GetNum());
				}

				InAccessor.GetRange(TArrayView<ValueType>(CachedValues), 0, InKeys);

				auto CompareAscending = [&CachedValues, &CustomGetIndex](int LHS, int RHS)
				{
					const int32 LHSIndex = CustomGetIndex(LHS);
					const int32 RHSIndex = CustomGetIndex(RHS);

					const ValueType& LHSValue = CachedValues[LHSIndex];
					const ValueType& RHSValue = CachedValues[RHSIndex];

					if (PCG::Private::MetadataTraits<ValueType>::Equal(LHSValue, RHSValue))
					{
						return LHSIndex < RHSIndex;
					}

					return PCG::Private::MetadataTraits<ValueType>::Less(LHSValue, RHSValue);
				};

				auto CompareDescending = [&CachedValues, &CustomGetIndex](int LHS, int RHS)
				{
					const int32 LHSIndex = CustomGetIndex(LHS);
					const int32 RHSIndex = CustomGetIndex(RHS);

					const ValueType& LHSValue = CachedValues[LHSIndex];
					const ValueType& RHSValue = CachedValues[RHSIndex];

					if (PCG::Private::MetadataTraits<ValueType>::Equal(LHSValue, RHSValue))
					{
						return LHSIndex > RHSIndex;
					}

					return PCG::Private::MetadataTraits<ValueType>::Greater(LHSValue, RHSValue);
				};

				// Fill integer sequence
				TArray<int32> ElementsIndexes;
				ElementsIndexes.Reserve(InArray.Num());
				for (int i = 0; i < InArray.Num(); ++i)
				{
					ElementsIndexes.Add(i);
				}

				if (bAscending)
				{
					ElementsIndexes.Sort(CompareAscending);
				}
				else
				{
					ElementsIndexes.Sort(CompareDescending);
				}

				TArray<T> SortedArray;
				SortedArray.Reserve(InArray.Num());

				for (int i = 0; i < InArray.Num(); ++i)
				{
					SortedArray.Add(MoveTemp(InArray[ElementsIndexes[i]]));
				}

				InArray = MoveTemp(SortedArray);
			}
		};

		PCGMetadataAttribute::CallbackWithRightType(InAccessor.GetUnderlyingType(), Callback);
	}
}
