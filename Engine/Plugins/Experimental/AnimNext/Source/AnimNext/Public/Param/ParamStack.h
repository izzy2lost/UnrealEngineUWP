// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/PagedArray.h"
#include "Param/ParamId.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamCompatibility.h"
#include "Misc/MemStack.h"

struct FInstancedPropertyBag;

namespace UE::AnimNext::Tests
{
	class FParamStackTest;
}

namespace UE::AnimNext
{

// Stack of parameter layers.
// Acts as an associative container - allows retrieval of parameter calues (by ID) that have been
// pushed onto the stack in 'layers'.
// Parameter values (and notably types) on higher stack layers override those on lower layers. 
// Older values and types are restored when a layer is popped.
// Parameter values, once pushed, can be overridden by subsequent layers, but they cannot be 
// removed until the layer that initially introduces the parameter is popped.
struct FParamStack
{
	friend class Tests::FParamStackTest;

private:
	enum class EParamFlags : uint8
	{
		None = 0,				// No flags
		Mutable = 1 << 0,		// Parameter is mutable, so can be mutated after it has been created
		Embedded = 1 << 1,		// Parameter will be stored as a Value, but stored directly on the Data pointer
		Reference = 1 << 2,		// Parameter is a reference, so will not be embdedded even if it fits in the Data pointer
	};

	FRIEND_ENUM_CLASS_FLAGS(EParamFlags);

	// Parameter memory wrapper
	struct FParam
	{
		FParam() = default;
		ANIMNEXT_API FParam(const FParam& InOtherParam);
		ANIMNEXT_API FParam& operator=(const FParam& InOtherParam);
		ANIMNEXT_API FParam(FParam&& InOtherParam);
		ANIMNEXT_API FParam& operator=(FParam&& InOtherParam);
		ANIMNEXT_API ~FParam();

		ANIMNEXT_API FParam(const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InData, bool bInIsReference, bool bInIsMutable);

		// Get the type handle of this param 
		FParamTypeHandle GetTypeHandle() const { return TypeHandle; }

		// Check whether this parameter is able to be mutated
		bool IsMutable() const { return EnumHasAnyFlags(Flags, EParamFlags::Mutable); }

		// Check whether this parameter is of reference type (i.e. the parameter refers to user data owned outside of the parameter)
		bool IsReference() const { return EnumHasAnyFlags(Flags, EParamFlags::Reference); }

		// Check whether this parameter is embedded in the parameter (stored internally rather than as a ptr)
		bool IsEmbedded() const { return EnumHasAnyFlags(Flags, EParamFlags::Embedded); }

		// Get an immutable view of the parameter's data
		TConstArrayView<uint8> GetData() const
		{
			if (EnumHasAnyFlags(Flags, EParamFlags::Embedded))
			{
				return TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&Data), Size);
			}
			else
			{
				return TConstArrayView<uint8>(static_cast<const uint8*>(Data), Size);
			}
		}

		// Get an mutable view of the parameter's data, returns an empty array view if this parameter is immutable
		TArrayView<uint8> GetMutableData()
		{
			if (IsMutable())
			{
				if (EnumHasAnyFlags(Flags, EParamFlags::Embedded))
				{
					return TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), Size);
				}
				else
				{
					return TArrayView<uint8>(static_cast<uint8*>(Data), Size);
				}
			}
			return TArrayView<uint8>();
		}

		// Check whether this param represents valid data. Note that this doesnt check the type hande for validity.
		bool IsValid() const
		{
			return Size > 0;
		}

		// Raw ptr to the data, or the data itself if we have EFlags::Embedded
		void* Data = nullptr;

		// The type of the param
		FParamTypeHandle TypeHandle;

		// Size of the data
		uint16 Size = 0;

		// Internal flags
		EParamFlags Flags = EParamFlags::None;
	};

	// Stack layers are what actually get pushed/popped onto the layer stack.
	// They are designed to be held on an instance, their data updated in place.
	// Memory ownership for params is assumed to be outside of the stack and layers
	struct FLayer
	{
		FLayer() = delete;

		explicit FLayer(FInstancedPropertyBag& InPropertyBag, bool bInIsMutable);

		explicit FLayer(TConstArrayView<TPair<FParamId, FParamStack::FParam>> InParams);

		// Params that this layer supplies
		TArray<FParamStack::FParam> Params;

		// ID that the param indices start at. Maps the global param ID range into the range for this layer
		uint32 MinParamId = 0;

		// Storage offset for this layer if it is owned internally, otherwise MAX_uint32
		uint32 OwnedStorageOffset = MAX_uint32;
	};

	// A layer held on the stack, may be owned by the stack or not
	struct FPushedLayer
	{
		FPushedLayer(FLayer& InLayer, FParamStack& InStack);

		FPushedLayer(const FPushedLayer& InPreviousLayer, FLayer& InLayer, FParamStack& InStack);

		FLayer& Layer;

		// Index offset for the previous layer for each active param.
		// Offset into FParamStack::PreviousLayerIndices.
		uint32 PreviousLayerIndexStart = MAX_uint32;

		// Serial number used for identifying layers to pop
		uint32 SerialNumber = 0;
	};

public:
	// Opaque handle to a layer on the stack
	struct FPushedLayerHandle
	{
		FPushedLayerHandle() = default;

		bool IsValid() const
		{
			return Index != MAX_uint32 && SerialNumber != 0;
		}

		void Invalidate()
		{
			Index = MAX_uint32;
			SerialNumber = 0;
		}

	private:
		friend struct FParamStack;

		FPushedLayerHandle(uint32 InIndex, uint32 InSerialNumber)
			: Index(InIndex)
			, SerialNumber(InSerialNumber)
		{}

		// Index into the stack
		uint32 Index = MAX_uint32;

		// Serial number of the layer
		uint32 SerialNumber = 0;
	};

	// Opaque handle to a layer that can be held by external systems
	struct FLayerHandle
	{
		FLayerHandle() = default;

		bool IsValid() const
		{
			return Layer.IsValid();
		}

		void Invalidate()
		{
			Layer = nullptr;
		}

	private:
		friend struct FParamStack;

		explicit FLayerHandle(TUniquePtr<FLayer>&& InLayer)
			: Layer(MoveTemp(InLayer))
		{}

		TUniquePtr<FLayer> Layer;
	};

	ANIMNEXT_API FParamStack();

	// Get the param stack for this thread
	static ANIMNEXT_API FParamStack& Get();

	// Push a value as a parameter layer
	// @param	InParamId			Parameter ID for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	FPushedLayerHandle PushValue(FParamId InParamId, ValueType&&... InValue)
	{
		return PushValues(InParamId, Forward<ValueType>(InValue)...);
	}

	// Push a value as a parameter layer
	// @param	InParamId			Parameter name for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	FPushedLayerHandle PushValue(FName InName, ValueType&&... InValue)
	{
		return PushValues(FParamId(InName), Forward<ValueType>(InValue)...);
	}

	// Push an interleaved parameter list of keys and values as a parameter layer
	// @param	InValues	Values to push
	template<typename... Args>
	FPushedLayerHandle PushValues(Args&&... InValues)
	{
		constexpr int32 NumItems = sizeof...(InValues) / 2;
		TArray<TPair<FParamId, FParam>, TInlineAllocator<NumItems>> ParamIdValues;
		ParamIdValues.Reserve(NumItems);
		return PushValuesHelper(ParamIdValues, Forward<Args>(InValues)...);
	}

	// Push a layer
	ANIMNEXT_API FPushedLayerHandle PushLayer(const FLayerHandle& InLayerHandle);

	// Push an internally-owned layer. Copies parameter data to internal storage.
	ANIMNEXT_API FPushedLayerHandle PushLayer(TConstArrayView<TPair<FParamId, FParam>> InParams);

	// Create a cached parameter layer from an instanced property bag
	static ANIMNEXT_API FLayerHandle MakeLayer(const FInstancedPropertyBag& InInstancedPropertyBag);

	// Create a cached mutable parameter layer from an instanced property bag
	static ANIMNEXT_API FLayerHandle MakeMutableLayer(FInstancedPropertyBag& InPropertyBag);

	// Create a cached parameter layer from set of params. Mutabilty is on a per-parameter basis.
	static ANIMNEXT_API FLayerHandle MakeLayer(TConstArrayView<TPair<FParamId, FParam>> InParams);

	// Make a parameter layer from a value
	// @param	InParamId			Parameter ID for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	static FLayerHandle MakeValueLayer(FParamId InParamId, ValueType&&... InValue)
	{
		return MakeValuesLayer(InParamId, Forward<ValueType>(InValue)...);
	}

	// Make a parameter layer from a value
	// @param	InParamId			Parameter name for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	static FLayerHandle MakeValueLayer(FName InName, ValueType&&... InValue)
	{
		return MakeValuesLayer(FParamId(InName), Forward<ValueType>(InValue)...);
	}

	// Make a parameter layer from an interleaved parameter list of keys and values
	// @param	InValues	Values to push
	template<typename... Args>
	static FLayerHandle MakeValuesLayer(Args&&... InValues)
	{
		constexpr int32 NumItems = sizeof...(InValues) / 2;
		TArray<TPair<FParamId, FParam>, TInlineAllocator<NumItems>> ParamIdValues;
		ParamIdValues.Reserve(NumItems);
		return MakeValuesLayerHelper(ParamIdValues, Forward<Args>(InValues)...);
	}

	// Pop a parameter layer - asserts if the layer supplied is not the top layer
	ANIMNEXT_API void PopLayer(FPushedLayerHandle InLayer);

	// Results for FParamStack::GetParam
	enum class EGetParamResult : uint8
	{
		// The requested parameter is present in the current scope
		Succeeded			= 0x00,

		// The requested parameter is present in the current scope, and its type is compatible according to the supplied requirements
		CompatibleType		= 0x01,

		// The requested parameter is present in the current scope, but its type is not compatible according to the supplied requirements
		IncompatibleType	= 0x02,

		// The requested parameter is not present in the current scope
		NotInScope			= 0x04,

		// The requested parameter is immutable but a mutable request was made of it
		Immutable			= 0x08,
	};

	FRIEND_ENUM_CLASS_FLAGS(FParamStack::EGetParamResult);

	// Get a pointer to a parameter's value given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to aresult that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	const ValueType* GetParamPtr(FParamId InParamId, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		TConstArrayView<uint8> Data;
		FParamTypeHandle TypeHandle;
		FParamStack::EGetParamResult Result = GetParamData(InParamId, FParamTypeHandle::GetHandle<ValueType>(), Data, TypeHandle);
		if (OutResult)
		{
			*OutResult = Result;
		}
		return reinterpret_cast<const ValueType*>(Data.GetData());
	}

	// Get a pointer to a parameter's mutable value given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped or mutable
	template<typename ValueType>
	ValueType* GetMutableParamPtr(FParamId InParamId, FParamStack::EGetParamResult* OutResult = nullptr)
	{
		TArrayView<uint8> Data;
		FParamTypeHandle TypeHandle;
		FParamStack::EGetParamResult Result = GetMutableParamData(InParamId, FParamTypeHandle::GetHandle<ValueType>(), Data, TypeHandle);
		if (OutResult)
		{
			*OutResult = Result;
		}
		return reinterpret_cast<ValueType*>(Data.GetData());
	}

	// Get a pointer to a parameter's value given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	const ValueType* GetParamPtr(FName InKey, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		return GetParamPtr<ValueType>(FParamId(InKey), OutResult);
	}

	// Get a pointer to a parameter's mutable value given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	ValueType* GetMutableParamPtr(FName InKey, FParamStack::EGetParamResult* OutResult = nullptr)
	{
		return GetMutableParamPtr<ValueType>(FParamId(InKey), OutResult);
	}

	// Get the value of a parameter given a FParamId. Potentially performs a copy of the value.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return the parameter's value, or a default value if the parameter is not present
	template<typename ValueType>
	ValueType GetParamValue(FParamId InParamId, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		if (const ValueType* ParamPtr = GetParamPtr<ValueType>(InParamId, OutResult))
		{
			return *ParamPtr;
		}
		return ValueType();
	}

	// Get the value of a parameter given a FName. Potentially performs a copy of the value.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return the parameter's value, or a default value if the parameter is not present
	template<typename ValueType>
	ValueType GetParamValue(FName InKey, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		if (const ValueType* ParamPtr = GetParamPtr<ValueType>(FParamId(InKey), OutResult))
		{
			return *ParamPtr;
		}
		return ValueType();
	}

	// Get a const reference to the value of a parameter given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a const reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	const ValueType& GetParam(FParamId InParamId, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		const ValueType* ParamPtr = GetParamPtr<ValueType>(InParamId, OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a const reference to the value of a parameter given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a const reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	const ValueType& GetParam(FName InKey, FParamStack::EGetParamResult* OutResult = nullptr) const
	{
		const ValueType* ParamPtr = GetParamPtr<ValueType>(FParamId(InKey), OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a mutable reference to the value of a parameter given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a mutable reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	ValueType& GetMutableParam(FParamId InParamId, FParamStack::EGetParamResult* OutResult = nullptr)
	{
		ValueType* ParamPtr = GetMutableParamPtr<ValueType>(InParamId, OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a mutable reference to the value of a parameter given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a mutable reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	ValueType& GetMutableParam(FName InKey, FParamStack::EGetParamResult* OutResult = nullptr)
	{
		ValueType* ParamPtr = GetMutableParamPtr<ValueType>(FParamId(InKey), OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a parameter's data given an FParamId.
	// @param	InParamId				Parameter ID to find the currently-pushed value for
	// @param	InParamTypeHandle		Handle to the type of the parameter, which must match the stored type for a value to
	//									be returned according to the supplied compatibility
	// @param	OutParamData			View into the parameters data that will be filled
	// @param	OutParamTypeHandle		Handle to the type of the parameter if the parameter is present in the stack.
	//									This will be valid even if the types are deemed incompatible.
	// @param	InRequiredCompatibility	The minimum required compatibility level
	// @return an enum describing the result of the operation @see FParamStack::EGetParamResult
	ANIMNEXT_API EGetParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;
	ANIMNEXT_API EGetParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;

	// Get a parameter's mutable data given an FParamId.
	// @param	InParamId				Parameter ID to find the currently-pushed value for
	// @param	InParamTypeHandle		Handle to the type of the parameter, which must match the stored type for a value to
	//									be returned according to the supplied compatibility
	// @param	OutParamData			View into the parameters data that will be filled
	// @param	OutParamTypeHandle		Handle to the type of the parameter if the parameter is present in the stack.
	//									This will be valid even if the types are deemed incompatible.
	// @param	InRequiredCompatibility	The minimum required compatibility level
	// @return an enum describing the result of the operation @see FParamStack::EGetParamResult
	ANIMNEXT_API EGetParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());
	ANIMNEXT_API EGetParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());

private:
	// Recursive helper function for PushValues
	template <uint32 NumItems, typename FirstType, typename SecondType, typename... OtherTypes>
	FPushedLayerHandle PushValuesHelper(TArray<TPair<FParamId, FParam>, TInlineAllocator<NumItems>>& InArray, FirstType&& InFirst, SecondType&& InSecond, OtherTypes&&... InOthers)
	{
		InArray.Emplace(FParamId(InFirst),
			FParam(
				FParamTypeHandle::GetHandle<std::remove_reference_t<SecondType>>(),
				TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InSecond)), sizeof(std::remove_reference_t<SecondType>)),
				std::is_reference_v<SecondType>,
				!std::is_const_v<std::remove_reference_t<SecondType>>
			)
		);

		if constexpr (sizeof...(InOthers) > 0)
		{
			return PushValuesHelper(InArray, Forward<OtherTypes>(InOthers)...);
		}
		else
		{
			return PushLayer(InArray);
		}
	}

	// Recursive helper function for MakeValuesLayer
	template <uint32 NumItems, typename FirstType, typename SecondType, typename... OtherTypes>
	static FLayerHandle MakeValuesLayerHelper(TArray<TPair<FParamId, FParam>, TInlineAllocator<NumItems>>& InArray, FirstType&& InFirst, SecondType&& InSecond, OtherTypes&&... InOthers)
	{
		InArray.Emplace(FParamId(InFirst), 
			FParam(
				FParamTypeHandle::GetHandle<std::remove_reference_t<SecondType>>(),
				TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InSecond)), sizeof(std::remove_reference_t<SecondType>)),
				std::is_reference_v<SecondType>,
				!std::is_const_v<std::remove_reference_t<SecondType>>
			)
		);

		if constexpr (sizeof...(InOthers) > 0)
		{
			return MakeValuesLayerHelper(InArray, Forward<OtherTypes>(InOthers)...);
		}
		else
		{
			return MakeLayer(InArray);
		}
	}

	// Helper function for layer pushes
	FPushedLayerHandle PushLayerInternal(FLayer& InLayer);

	// Check whether a given parameter is mutable
	ANIMNEXT_API bool IsMutableParam(FParamId InId) const;

	// Check whether a given parameter is mutable
	bool IsMutableParam(FName InKey) const
	{
		return IsMutableParam(FParamId(InKey));
	}

	// Check whether a given parameter is a reference
	ANIMNEXT_API bool IsReferenceParam(FParamId InId) const;

	// Check whether a given parameter is a reference
	bool IsReferenceParam(FName InKey) const
	{
		return IsReferenceParam(FParamId(InKey));
	}

	// Allocates any necessary owned storage for non-reference, non-embeded parameters that a layer holds
	// @return the base offset of the storage area
	uint32 AllocAndCopyOwnedParamStorage(TArrayView<FParam> InParams);

	// Frees any owned storage, restores owned storage offset
	void FreeOwnedParamStorage(uint32 InOffset);

	// Resize layer indices to deal with any new params we have seen since the stack was created
	void ResizeLayerIndices();

	// Get a new serial number for a pushed layer
	uint32 MakeSerialNumber();

	// Layer stack
	TArray<FPushedLayer> Layers;

	// Owned layers, paged for stable addresses as layers reference them by ptr
	// Grows with each new pushed owned layer
	TPagedArray<FLayer, 4096> OwnedStackLayers;

	// Parameter storage for owned layers, paged for stable addresses as layer parameters reference them by ptr
	// Grows with each new pushed layer, free'd when the param stack is destroyed
	TPagedArray<uint8, 4096> OwnedLayerParamStorage;

	// Indicies into layers. These are the indices into the layer stack where the current 'top' value resides.
	// Grows on creation and when new parameter IDs are encountered
	TArray<uint16> LayerIndices;

	// Indices for the previous layer for each active param. MAX_uint16 if there is no previous layer for the param.
	// Pushed layers hold views into this array.
	// Grows with each new pushed layer, free'd when the param stack is destroyed
	TArray<uint16> PreviousLayerIndices;

	// Serial number for stack layers
	uint32 SerialNumber = 0;
};

ENUM_CLASS_FLAGS(FParamStack::EGetParamResult);
ENUM_CLASS_FLAGS(FParamStack::EParamFlags);

}
