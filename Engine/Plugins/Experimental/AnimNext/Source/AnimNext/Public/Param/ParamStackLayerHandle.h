// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamResult.h"
#include "Param/ParamId.h"
#include "Param/ParamTypeHandle.h"

class UAnimNextParameterBlock;

namespace UE::AnimNext
{
	struct FParamStackLayer;
}

namespace UE::AnimNext::Private
{
	struct FParamEntry;
}

namespace UE::AnimNext
{

// Opaque handle to a layer that can be held by external systems
// Move-only, single ownership
struct FParamStackLayerHandle
{
public:
	ANIMNEXT_API FParamStackLayerHandle();
	FParamStackLayerHandle(const FParamStackLayerHandle& InOther) = delete;
	FParamStackLayerHandle& operator=(const FParamStackLayerHandle& InOther) = delete;
	ANIMNEXT_API FParamStackLayerHandle(FParamStackLayerHandle&& InLayer) noexcept;
	ANIMNEXT_API FParamStackLayerHandle& operator=(FParamStackLayerHandle&& InLayer) noexcept;
	ANIMNEXT_API ~FParamStackLayerHandle();

private:
	FParamStackLayerHandle(TUniquePtr<FParamStackLayer>&& InLayer);

public:
	// Check this layer's validity
	bool IsValid() const;

	// Invalidate the layer
	void Invalidate();

	// Set a parameter's value in this layer
	// @param	InParamId			Parameter ID for the parameter
	// @param	InValue				Value to set
	template<typename... ValueType>
	FParamResult SetValue(FParamId InParamId, ValueType&&... InValue)
	{
		return SetValues(InParamId, Forward<ValueType>(InValue)...);
	}

	// Set a parameter's value in this layer
	// @param	InName				Parameter name for the parameter
	// @param	InValue				Value to set
	template<typename... ValueType>
	FParamResult SetValue(FName InName, ValueType&&... InValue)
	{
		return SetValues(FParamId(InName), Forward<ValueType>(InValue)...);
	}

	// Set a parameter's value in this layer
	// @param	InParamId			Parameter ID for the parameter
	// @param	InTypeHandle		The type of the parameter
	// @param	InData				Value to set
	ANIMNEXT_API FParamResult SetValueRaw(FParamId InParamId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8> InData);

	// Get a parameter's value from this layer
	// @param	InParamId			Parameter ID for the parameter
	// @param	InTypeHandle		The type of the parameter
	// @param	InData				Value to get
	ANIMNEXT_API FParamResult GetValueRaw(FParamId InParamId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutData);

	// Set multiple parameter values in this layer
	// @param	InValues	Names & values to set, interleaved
	template<typename... Args>
	FParamResult SetValues(Args&&... InValues)
	{
		constexpr int32 NumItems = sizeof...(InValues) / 2;
		TArray<TPair<FParamId, Private::FParamEntry>, TInlineAllocator<NumItems>> ParamIdValues;
		ParamIdValues.Reserve(NumItems);
		return SetValuesHelper(ParamIdValues, Forward<Args>(InValues)...);
	}

	// Get the underlying storage for this layer.
	// @return nullptr if the layer does not use the supplied underlying storage type
	template<typename WrappedType>
	WrappedType* As() const
	{
		if constexpr (TModels<CStaticClassProvider, WrappedType>::Value)
		{
			return Cast<WrappedType>(GetUObjectFromLayer());
		}
		else if constexpr (std::is_same_v<WrappedType, FInstancedPropertyBag>)
		{
			return GetInstancedPropertyBagFromLayer();
		}

		return nullptr;
	}

private:
	friend struct FParamStack;
	friend class UAnimNextParameterBlock;

	// Set parameter values
	ANIMNEXT_API FParamResult SetValuesInternal(TConstArrayView<TPair<FParamId, Private::FParamEntry>> InParams);

	// Helpers for As() to hide Layer
	ANIMNEXT_API UObject* GetUObjectFromLayer() const;
	ANIMNEXT_API FInstancedPropertyBag* GetInstancedPropertyBagFromLayer() const;

	// Recursive helper function for SetValues
	template <uint32 NumItems, typename FirstType, typename SecondType, typename... OtherTypes>
	FParamResult SetValuesHelper(TArray<TPair<FParamId, Private::FParamEntry>, TInlineAllocator<NumItems>>& InArray, FirstType&& InFirst, SecondType&& InSecond, OtherTypes&&... InOthers)
	{
		InArray.Emplace(FParamId(InFirst),
			Private::FParamEntry(
				FParamTypeHandle::GetHandle<std::remove_reference_t<SecondType>>(),
				TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InSecond)), sizeof(std::remove_reference_t<SecondType>)),
				std::is_reference_v<SecondType>,
				!std::is_const_v<std::remove_reference_t<SecondType>>
			)
		);

		if constexpr (sizeof...(InOthers) > 0)
		{
			return SetValuesHelper(InArray, Forward<OtherTypes>(InOthers)...);
		}
		else
		{
			return SetValuesInternal(InArray);
		}
	}

	TUniquePtr<FParamStackLayer> Layer;
};

}