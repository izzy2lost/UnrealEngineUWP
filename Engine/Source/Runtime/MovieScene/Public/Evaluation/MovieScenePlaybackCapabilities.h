// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Evaluation/IMovieScenePlaybackCapability.h"
#include "EntitySystem/RelativePtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/TVariant.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/PointerIsConvertibleFromTo.h"

class IMovieScenePlayer;
class IMovieScenePlaybackClient;

namespace UE::MovieScene
{

enum class EPlaybackCapabilityStorageMode : uint8
{
	Inline = 0,
	RawPointer,
	SharedPointer
};

/**
 * Return value for accessing raw capabalities.
 */
struct FPlaybackCapabilityPtr
{
	void* Ptr = 0;
	EPlaybackCapabilityStorageMode StorageMode;

	template<typename T>
	T* ResolveOptional() const
	{
		if (Ptr)
		{
			switch (StorageMode)
			{
				case EPlaybackCapabilityStorageMode::Inline:
					return static_cast<T*>(Ptr);
				case EPlaybackCapabilityStorageMode::RawPointer:
					return *static_cast<T**>(Ptr);
				case EPlaybackCapabilityStorageMode::SharedPointer:
					return static_cast<TSharedPtr<T>*>(Ptr)->Get();
				default:
					checkf(false, TEXT("Unexpected playback capability storage mode"));
					return nullptr;
			}
		}
		return nullptr;
	}

	template<typename T>
	T& ResolveChecked() const
	{
		check(Ptr);
		switch (StorageMode)
		{
			case EPlaybackCapabilityStorageMode::Inline:
				return *static_cast<T*>(Ptr);
			case EPlaybackCapabilityStorageMode::RawPointer:
				return **static_cast<T**>(Ptr);
			case EPlaybackCapabilityStorageMode::SharedPointer:
				return *static_cast<TSharedPtr<T>*>(Ptr)->Get();
			default:
				checkf(false, TEXT("Unexpected playback capability storage mode"));
				// Nothing reasonable to do, let's just proceed as if it was inline
				return *static_cast<T*>(Ptr);
		}
	}
};

// Utility callbacks for the concrete capability objects we have in a container.
using FPlaybackCapabilityInterfaceCastHelper = IPlaybackCapability*(*)(void* Ptr);
using FPlaybackCapabilityDestructionHelper = void(*)(void* Ptr);

// Callback for casting a stored capabilty object to an IPlaybackCapability pointer if possible.
template<typename StorageType>
struct TPlaybackCapabilityInterfaceCast
{
	static IPlaybackCapability* InterfaceCast(void* Ptr)
	{
		if constexpr (TPointerIsConvertibleFromTo<StorageType, IPlaybackCapability>::Value)
		{
			return static_cast<IPlaybackCapability*>((StorageType*)Ptr);
		}
		else
		{
			return nullptr;
		}
	}
};
template<typename PointedType>
struct TPlaybackCapabilityInterfaceCast<PointedType*>
{
	static IPlaybackCapability* InterfaceCast(void* Ptr) 
	{
		if constexpr (TPointerIsConvertibleFromTo<PointedType, IPlaybackCapability>::Value)
		{
			PointedType* TypedPtr = *(PointedType**)Ptr;
			return static_cast<IPlaybackCapability*>(TypedPtr);
		}
		else
		{
			return nullptr;
		}
	};
};
template<typename PointedType>
struct TPlaybackCapabilityInterfaceCast<TSharedPtr<PointedType>>
{
	static IPlaybackCapability* InterfaceCast(void* Ptr) 
	{
		if constexpr (TPointerIsConvertibleFromTo<PointedType, IPlaybackCapability>::Value)
		{
			TSharedPtr<PointedType>& TypedPtr = *(TSharedPtr<PointedType>*)Ptr;
			return static_cast<IPlaybackCapability*>(TypedPtr.Get());
		}
		else
		{
			return nullptr;
		}
	};
};

// Callback for destroying the stored capability object, whether that's the capability itself
// (when stored inline), or a shared pointer, or whatever else.
template<typename StorageType>
struct TPlaybackCapabilityDestructor
{
	static void Destroy(void* Ptr)
	{
		StorageType* StoragePtr = (StorageType*)Ptr;
		StoragePtr->~StorageType();
	}
};

// Helper callbacks for managing the lifetime of a capability.
struct FPlaybackCapabilityHelpers
{
	FPlaybackCapabilityInterfaceCastHelper InterfaceCast = nullptr;
	FPlaybackCapabilityDestructionHelper Destructor = nullptr;
};

template<typename StorageType>
struct TPlaybackCapabilityHelpers
{
	static FPlaybackCapabilityHelpers GetHelpers()
	{
		FPlaybackCapabilityHelpers Helpers;
		Helpers.InterfaceCast = &TPlaybackCapabilityInterfaceCast<StorageType>::InterfaceCast;
		Helpers.Destructor = &TPlaybackCapabilityDestructor<StorageType>::Destroy;
		return Helpers;
	}
};

// Storage traits for playback capabilities, only for internal use.
template<typename StorageType, typename CapabilityType, typename=void>
struct TPlaybackCapabilityStorageTraits;

template<typename StorageType, typename CapabilityType>
struct TPlaybackCapabilityStorageTraits<StorageType, CapabilityType, typename TEnableIf<TPointerIsConvertibleFromTo<StorageType, CapabilityType>::Value>::Type>
{
	static EPlaybackCapabilityStorageMode GetStorageMode() { return EPlaybackCapabilityStorageMode::Inline; }

	static uint8 ComputePointerOffset(StorageType* StoragePtr)
	{
		const uint64 PointerOffset =
			reinterpret_cast<uint64>(static_cast<CapabilityType*>(StoragePtr))
			-
			reinterpret_cast<uint64>(StoragePtr);
		check(PointerOffset <= TNumericLimits<uint8>::Max());
		return static_cast<uint8>(PointerOffset);
	}
};

template<typename CapabilityType>
struct TPlaybackCapabilityStorageTraits<CapabilityType*, CapabilityType, void>
{
	static EPlaybackCapabilityStorageMode GetStorageMode() { return EPlaybackCapabilityStorageMode::RawPointer; }

	static uint8 ComputePointerOffset(CapabilityType** StoragePtr) { return 0; }
};

template<typename CapabilityType>
struct TPlaybackCapabilityStorageTraits<TSharedPtr<CapabilityType>, CapabilityType, void>
{
	static EPlaybackCapabilityStorageMode GetStorageMode() { return EPlaybackCapabilityStorageMode::SharedPointer; }

	static uint8 ComputePointerOffset(TSharedPtr<CapabilityType>* StoragePtr) { return 0; }
};

/**
 * Header that describes an entry in the capabilities buffer.
 */
struct FPlaybackCapabilityHeader
{
	// 2 Bytes
	TRelativePtr<void, uint16> Capability;

	// 1 Byte
	uint8 Sizeof = 0;

	// 1 Byte
	uint8 Alignment = 0;

	// 1 Byte
	uint8 PointerOffset = 0;

	// 1 Byte
	EPlaybackCapabilityStorageMode StorageMode = EPlaybackCapabilityStorageMode::Inline;

	/** Resolve the given raw pointer into a capability pointer */
	FPlaybackCapabilityPtr Resolve(void* InMemory) const
	{
		void* DerivedPointer = Capability.Resolve(InMemory);
		return FPlaybackCapabilityPtr{ (void*)((uint8*)DerivedPointer + PointerOffset), StorageMode };
	}
};

/**
 * Basic, mostly untyped, implementation of the capabilities buffer.
 */
struct FPlaybackCapabilitiesImpl
{
protected:

	FPlaybackCapabilitiesImpl() = default;

	FPlaybackCapabilitiesImpl(const FPlaybackCapabilitiesImpl&) = delete;
	FPlaybackCapabilitiesImpl& operator=(const FPlaybackCapabilitiesImpl&) = delete;

	FPlaybackCapabilitiesImpl(FPlaybackCapabilitiesImpl&&) = delete;
	FPlaybackCapabilitiesImpl& operator=(FPlaybackCapabilitiesImpl&&) = delete;

	bool HasCapability(uint32 CapabilityBit) const
	{
		return (AllCapabilities & CapabilityBit) != 0;
	}

	FPlaybackCapabilityPtr FindCapability(uint32 CapabilityBit) const
	{
		if (HasCapability(CapabilityBit))
		{
			const int32 Index = GetCapabilityIndex(CapabilityBit);
			check(Index >= 0 && Index < 255);
			return GetHeader(static_cast<uint8>(Index)).Resolve(Memory);
		}

		return FPlaybackCapabilityPtr();
	}

	FPlaybackCapabilityPtr GetCapabilityChecked(uint32 CapabilityBit) const
	{
		const int32 Index = GetCapabilityIndex(CapabilityBit);
		check(Index >= 0 && Index < 255);
		return GetHeader(static_cast<uint8>(Index)).Resolve(Memory);
	}

	/**
	 * Creates and stores a new capability object at the given bit.
	 * 
	 * - StorageType is what is actually constructed and stored in our memory buffer. It can be the same as
	 *			CapabilityType, a subclass of it, a pointer to it, or a shared pointer to it.
	 * 
	 * - CapabilityType is the base capability type, used onlyfor inline storage when the StorageType is a subclass
	 *			and we need to compute the pointer offset.
	 */
	template<typename StorageType, typename CapabilityType, typename ...ArgTypes>
	FPlaybackCapabilityPtr AddCapability(uint32 CapabilityBit, ArgTypes&&... InArgs)
	{
		checkf((AllCapabilities & CapabilityBit) == 0, TEXT("Capability already exists!"));

		// Add the enum entry
		AllCapabilities |= CapabilityBit;

		// Find the index of the new capability by counting how many bits are set before it
		// For example, given CapabilityBit=0b00010000 and AllCapabilities=0b00011011:
		//                      (CapabilityBit-1) = 0b00001111
		//    AllCapabilities & (CapabilityBit-1) = 0b00001011
		//                  CountBits(0b00001011) = 3
		const int32 NewCapabilityIndex = static_cast<int32>(FMath::CountBits(AllCapabilities & (CapabilityBit-1u)));

		const FPlaybackCapabilityHeader* ExistingHeaders = reinterpret_cast<const FPlaybackCapabilityHeader*>(Memory);

		uint64 RequiredAlignment = FMath::Max(alignof(StorageType), alignof(FPlaybackCapabilityHeader));

		const int32 ExistingNum = static_cast<int32>(Num);

		// Compute our required alignment for the allocation
		{
			for (int32 Index = 0; Index < ExistingNum; ++Index)
			{
				RequiredAlignment = FMath::Max(RequiredAlignment, (uint64)ExistingHeaders[Index].Alignment);
			}
		}

		uint64 RequiredSizeof = 0u;

		// Compute the required size of our allocation
		{
			// Allocate space for headers
			RequiredSizeof += (ExistingNum+1) * sizeof(FPlaybackCapabilityHeader);

			int32 Index = 0;

			// Count up the sizes and alignments of pre-existing capabilities that exist before this new entry
			for (; Index < NewCapabilityIndex; ++Index)
			{
				RequiredSizeof = Align(RequiredSizeof, (uint64)ExistingHeaders[Index].Alignment);
				RequiredSizeof += ExistingHeaders[Index].Sizeof;
			}

			// Count up the size and alignment for the new capability
			RequiredSizeof = Align(RequiredSizeof, alignof(StorageType));
			RequiredSizeof += sizeof(StorageType);
			++Index;

			// Now count up the sizes and alignments of pre-existing capabilities that exist after this new entry
			for (; Index < ExistingNum+1; ++Index)
			{
				RequiredSizeof = Align(RequiredSizeof, (uint64)ExistingHeaders[Index-1].Alignment);
				RequiredSizeof += ExistingHeaders[Index-1].Sizeof;
			}
		}

		check( RequiredAlignment <= 0XFF );

		/// ----------------------------------------

		uint8* OldAllocation = Memory;

		// Make a new allocation if necessary
		const bool bNeedsReallocation = RequiredAlignment > Alignment || RequiredSizeof > Capacity;
		if (bNeedsReallocation)
		{
			// Use the greater of the required size or double the current size to allow some additional capcity
			Capacity = FMath::Max(RequiredSizeof, uint64(Capacity)*2);
			Memory = reinterpret_cast<uint8*>(FMemory::Malloc(Capacity, RequiredAlignment));
		}

		// We now have an extra entry
		++Num;

		FPlaybackCapabilityHeader* CurrentHeader = reinterpret_cast<FPlaybackCapabilityHeader*>(Memory) + (Num-1);
		uint8* CapabilityPtr = Memory + RequiredSizeof;

		auto RelocateCapability = [this, ExistingHeaders, OldAllocation, &CurrentHeader, &CapabilityPtr](int32 OldIndex)
		{
			// Go back
			CapabilityPtr -= ExistingHeaders[OldIndex].Sizeof;
			CapabilityPtr = AlignDown(CapabilityPtr, ExistingHeaders[OldIndex].Alignment);

			// Copy the header
			new (CurrentHeader) FPlaybackCapabilityHeader(ExistingHeaders[OldIndex]);
			CurrentHeader->Capability = TRelativePtr<void, uint16>(this->Memory, CapabilityPtr);

			--CurrentHeader;

			void* OldCapability = ExistingHeaders[OldIndex].Capability.Resolve(OldAllocation);
			FMemory::Memmove(CapabilityPtr, OldCapability, ExistingHeaders[OldIndex].Sizeof);
		};

		int32 Index = static_cast<int32>(Num) - 1;
		for (; Index > NewCapabilityIndex; --Index)
		{
			RelocateCapability(Index-1);
		}

		// Make the new entry
		CapabilityPtr -= sizeof(StorageType);
		CapabilityPtr = AlignDown(CapabilityPtr, alignof(StorageType));

		StorageType* NewCapabilityPtr = reinterpret_cast<StorageType*>(CapabilityPtr);

		// Allocate the new type
		new (NewCapabilityPtr) StorageType (Forward<ArgTypes>(InArgs)...);

		static_assert(alignof(StorageType) < 0x7F, "Required alignment of capability must fit in 7 bytes");

		using FStorageTraits = TPlaybackCapabilityStorageTraits<StorageType, CapabilityType>;

		// Construct the header
		const FPlaybackCapabilityHeader* NewHeader = CurrentHeader;
		CurrentHeader->Capability.Reset(Memory, NewCapabilityPtr);
		CurrentHeader->Sizeof = sizeof(StorageType);
		CurrentHeader->Alignment = alignof(StorageType);
		CurrentHeader->StorageMode = FStorageTraits::GetStorageMode();
		// The pointer offset is whatever we need to add to the capability pointer in order to get
		// a pointer to capability type itself (in case the storage type is a sub-class of it)
		// We only need it if the capability object is stored inline inside our memory buffer.
		CurrentHeader->PointerOffset = FStorageTraits::ComputePointerOffset(NewCapabilityPtr);

		--CurrentHeader;
		--Index;

		// Relocate the entries that are before the new one
		for (; Index >= 0; --Index)
		{
			RelocateCapability(Index);
		}

		// Tidy up the old allocation. We do not call destructors here because we relocated everything.
		if (bNeedsReallocation && OldAllocation)
		{
			FMemory::Free(OldAllocation);
		}

		// Insert the helpers for the new capability.
		Helpers.Insert(TPlaybackCapabilityHelpers<StorageType>::GetHelpers(), NewCapabilityIndex);

		// Return the new capability pointer. We call the header's Resolve method here because returning NewCapabilityPtr
		// would return the derived type pointer. We want the base (capability) pointer.
		return NewHeader->Resolve(Memory);
	}

	const FPlaybackCapabilityHeader& GetHeader(uint8 Index) const
	{
		check(Index < Num);
		return reinterpret_cast<FPlaybackCapabilityHeader*>(Memory)[Index];
	}

	TArrayView<const FPlaybackCapabilityHeader> GetHeaders() const
	{
		return MakeArrayView(reinterpret_cast<FPlaybackCapabilityHeader*>(Memory), Num);
	}

	int32 GetCapabilityIndex(uint32 CapabilityBit) const
	{
		if ( (AllCapabilities & CapabilityBit) == 0)
		{
			return INDEX_NONE;
		}
		return FMath::CountBits(AllCapabilities & (CapabilityBit-1u));
	}
	
	// Schema:
	// [header_1|...|header_n|entry_0|...|entry_n]
	uint8* Memory = nullptr;
	uint16 Alignment = 0u;
	uint16 Capacity = 0u;
	uint8 Num = 0u;
	uint32 AllCapabilities = 0u;

	// Function pointers for invalidating, destroying, etc. capabilities.
	TArray<FPlaybackCapabilityHelpers> Helpers;
};

/**
 * Actual playback capabilities container.
 */
struct FPlaybackCapabilities : FPlaybackCapabilitiesImpl
{
	FPlaybackCapabilities() = default;

	FPlaybackCapabilities(const FPlaybackCapabilities&) = delete;
	FPlaybackCapabilities& operator=(const FPlaybackCapabilities&) = delete;

	MOVIESCENE_API FPlaybackCapabilities(FPlaybackCapabilities&&);
	MOVIESCENE_API FPlaybackCapabilities& operator=(FPlaybackCapabilities&&);

	MOVIESCENE_API ~FPlaybackCapabilities();

	/** Checks whether this container has the given capability */
	bool HasCapability(FPlaybackCapabilityID CapabilityID) const
	{
		uint32 CapabilityBit = 1 << CapabilityID.Index;
		return FPlaybackCapabilitiesImpl::HasCapability(CapabilityBit);
	}

	/** Finds the specified capability within the container, if present */
	template<typename T>
	T* FindCapability(TPlaybackCapabilityID<T> CapabilityID) const
	{
		uint32 CapabilityBit = 1 << CapabilityID.Index;
		FPlaybackCapabilityPtr Ptr = FPlaybackCapabilitiesImpl::FindCapability(CapabilityBit);
		return Ptr.ResolveOptional<T>();
	}

	/** Returns the specified capability within the container, asserts if not found */
	template<typename T>
	T& GetCapabilityChecked(TPlaybackCapabilityID<T> CapabilityID) const
	{
		uint32 CapabilityBit = 1 << CapabilityID.Index;
		FPlaybackCapabilityPtr Ptr = FPlaybackCapabilitiesImpl::GetCapabilityChecked(CapabilityBit);
		return Ptr.ResolveChecked<T>();
	}

	/**
	 * Adds the specified capability to the container, using the supplied arguments to construct it
	 * The capability object will be stored inline and owned by this container. It will be destroyed when the
	 * container itself is destroyed.
	 */
	template<typename T, typename ...ArgTypes>
	T& AddCapability(TPlaybackCapabilityID<T> CapabilityID, ArgTypes&&... InArgs)
	{
		return DoAddCapability<T, T>(CapabilityID, Forward<ArgTypes>(InArgs)...);
	}

	/**
	 * Adds the specified capability to the container, using the supplied arguments to construct a sub-class of it 
	 * The capability object of the specified sub-class will be stored inline and owned by this container. It will
	 * be destroyed when the container itself is destroyed.
	 */
	template<typename Impl, typename T, typename ...ArgTypes>
	T& AddCapabilityImplementation(TPlaybackCapabilityID<T> CapabilityID, ArgTypes&&... InArgs)
	{
		return DoAddCapability<Impl, T>(CapabilityID, Forward<ArgTypes>(InArgs)...);
	}

	/**
	 * Adds the specified capability to the container, as a simple raw pointer
	 * Ownership of the capability object being pointed to is the caller's responsability. This container will only
	 * store a raw pointer.
	 */
	template<typename T>
	T& AddCapabilityRaw(TPlaybackCapabilityID<T> CapabilityID, T* InPointer)
	{
		return DoAddCapability<T*, T>(CapabilityID, InPointer);
	}
	
	/**
	 * Adds the specified capability to the container, as a shared pointer
	 * Ownership of the capability object being pointed to respects classic shared pointer semantics. This container
	 * only maintains one such shared pointer until the container is destroyed.
	 */
	template<typename T>
	T& AddCapabilityShared(TPlaybackCapabilityID<T> CapabilityID, TSharedRef<T> InSharedRef)
	{
		return DoAddCapability<TSharedPtr<T>, T>(CapabilityID, InSharedRef);
	}

	/**
	 * Calls InvalidateCacheData on any capability that implements the IPlaybackCapability interface.
	 */
	void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker);

private:

	template<typename Impl, typename T, typename ...ArgTypes>
	T& DoAddCapability(TPlaybackCapabilityID<T> CapabilityID, ArgTypes&&... InArgs)
	{
		uint32 CapabilityBit = 1 << CapabilityID.Index;
		FPlaybackCapabilityPtr Ptr = FPlaybackCapabilitiesImpl::AddCapability<Impl, T>(CapabilityBit, Forward<ArgTypes>(InArgs)...);
		return Ptr.ResolveChecked<T>();
	}

	void Destroy();
};

} // namespace UE::MovieScene
